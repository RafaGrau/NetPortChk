#include "pch.h"
#include "XmlLite.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <climits>
#include <codecvt>
#include <locale>

// ──────────────────────────────────────────────────────────────────────────────
// XmlNode helpers
// ──────────────────────────────────────────────────────────────────────────────
const std::wstring& XmlNode::Attr(const wchar_t* name, const std::wstring& def) const
{
    auto it = attrs.find(name);
    return (it != attrs.end()) ? it->second : def;
}

int XmlNode::AttrInt(const wchar_t* name, int def) const
{
    auto it = attrs.find(name);
    if (it == attrs.end() || it->second.empty()) return def;
    try {
        size_t idx = 0;
        long v = std::stol(it->second, &idx);
        if (idx == 0)                          return def;
        if (v < INT_MIN || v > INT_MAX)        return def;
        return static_cast<int>(v);
    } catch (...) {
        return def;   // invalid_argument, out_of_range, etc.
    }
}

bool XmlNode::AttrBool(const wchar_t* name, bool def) const
{
    auto it = attrs.find(name);
    if (it == attrs.end()) return def;
    return it->second == L"1" || it->second == L"true";
}

std::shared_ptr<XmlNode> XmlNode::FirstChild(const wchar_t* tag_) const
{
    for (auto& c : children)
        if (c->tag == tag_) return c;
    return nullptr;
}

std::vector<std::shared_ptr<XmlNode>> XmlNode::Children(const wchar_t* tag_) const
{
    std::vector<std::shared_ptr<XmlNode>> out;
    for (auto& c : children)
        if (c->tag == tag_) out.push_back(c);
    return out;
}

// ──────────────────────────────────────────────────────────────────────────────
// Tokeniser / Parser
// ──────────────────────────────────────────────────────────────────────────────
namespace {

struct Parser
{
    const wchar_t* p;
    const wchar_t* end;
    int   depth { 0 };
    static constexpr int kMaxDepth = 64;   // protección anti-overflow de pila

    wchar_t peek() const { return (p < end) ? *p : 0; }
    wchar_t get()        { return (p < end) ? *p++ : 0; }
    bool    eof()  const { return p >= end; }

    void SkipWS() { while (!eof() && iswspace(peek())) get(); }

    bool Expect(const wchar_t* s)
    {
        while (*s) { if (eof() || get() != *s++) return false; }
        return true;
    }

    std::wstring ReadName()
    {
        std::wstring n;
        while (!eof() && (iswalnum(peek()) || peek()==L'_' || peek()==L'-' || peek()==L':' || peek()==L'.'))
            n += get();
        return n;
    }

    std::wstring ReadAttrValue()
    {
        wchar_t q = get(); // ' or "
        std::wstring v;
        while (!eof() && peek() != q) {
            if (peek() == L'&') {
                get();
                std::wstring ent;
                while (!eof() && peek() != L';') ent += get();
                get(); // ;
                if      (ent == L"amp")  v += L'&';
                else if (ent == L"lt")   v += L'<';
                else if (ent == L"gt")   v += L'>';
                else if (ent == L"quot") v += L'"';
                else if (ent == L"apos") v += L'\'';
            } else {
                v += get();
            }
        }
        get(); // closing quote
        return v;
    }

    // Returns nullptr on error / end of input
    std::shared_ptr<XmlNode> ParseNode()
    {
        // Anti-stack-overflow: rechazar XML profundamente anidado.
        if (depth >= kMaxDepth) return nullptr;
        ++depth;
        struct DepthGuard { int& d; ~DepthGuard() { --d; } } guard{ depth };

        SkipWS();
        if (eof() || peek() != L'<') return nullptr;
        get(); // '<'

        // Comment / Processing instruction / CDATA / DOCTYPE
        if (peek() == L'!') {
            get();
            if (peek() == L'-') { // <!--
                while (!eof()) {
                    if (get() == L'-' && peek() == L'-') { get(); get(); break; }
                }
                return nullptr;
            }
            // DOCTYPE or CDATA – skip to >
            while (!eof() && get() != L'>') {}
            return nullptr;
        }
        if (peek() == L'?') { // PI
            while (!eof() && get() != L'>') {}
            return nullptr;
        }

        auto node = std::make_shared<XmlNode>();
        node->tag = ReadName();
        SkipWS();

        // Attributes
        while (!eof() && peek() != L'>' && peek() != L'/') {
            std::wstring k = ReadName();
            SkipWS();
            if (peek() == L'=') { get(); SkipWS(); node->attrs[k] = ReadAttrValue(); }
            SkipWS();
        }

        if (peek() == L'/') { get(); get(); return node; } // self-closing
        get(); // '>'

        // Children / text
        while (!eof()) {
            if (peek() == L'<') {
                // Check for closing tag
                const wchar_t* save = p;
                get(); // '<'
                if (peek() == L'/') {
                    get(); // '/'
                    std::wstring cname = ReadName();
                    SkipWS(); get(); // '>'
                    (void)cname;
                    break;
                }
                p = save; // rewind
                auto child = ParseNode();
                if (child) node->children.push_back(child);
            } else {
                // Text content
                while (!eof() && peek() != L'<') node->text += get();
                // Trim
                while (!node->text.empty() && iswspace(node->text.front())) node->text.erase(node->text.begin());
                while (!node->text.empty() && iswspace(node->text.back()))  node->text.pop_back();
            }
        }
        return node;
    }
};

} // namespace

std::shared_ptr<XmlNode> XmlParse(const std::wstring& xml)
{
    Parser ps { xml.c_str(), xml.c_str() + xml.size() };
    std::shared_ptr<XmlNode> root;
    while (!ps.eof()) {
        auto n = ps.ParseNode();
        if (n) { root = n; break; }
    }
    return root;
}

std::shared_ptr<XmlNode> XmlParseFile(const wchar_t* path)
{
    std::wifstream f(path, std::ios::binary);
    if (!f.is_open()) return nullptr;

    // Anti-DoS: rechazar ficheros excesivamente grandes (4 MiB es de sobra
    // para una configuración real de NetPortChk con cientos de servidores).
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz < 0 || sz > static_cast<std::streamoff>(4 * 1024 * 1024))
        return nullptr;

    // Imbuir UTF-8: el writer declara `encoding="UTF-8"` y emite UTF-8;
    // sin imbue, wifstream usa la cp del sistema y los acentos se corrompen
    // al mover el config entre máquinas con locales distintos.
    // consume_header descarta un BOM EF BB BF si está presente.
#pragma warning(push)
#pragma warning(disable: 4996)
    f.imbue(std::locale(f.getloc(),
        new std::codecvt_utf8<wchar_t, 0x10ffff, std::consume_header>));
#pragma warning(pop)

    std::wstring xml((std::istreambuf_iterator<wchar_t>(f)),
                      std::istreambuf_iterator<wchar_t>());
    return XmlParse(xml);
}

// ──────────────────────────────────────────────────────────────────────────────
// XmlWriter
// ──────────────────────────────────────────────────────────────────────────────
XmlWriter::XmlWriter()
{
    m_ss << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
}

void XmlWriter::Indent()
{
    for (int i = 0; i < m_depth * 2; ++i) m_ss << L' ';
}

std::wstring XmlWriter::Escape(const std::wstring& s)
{
    std::wstring r;
    for (wchar_t c : s) {
        switch (c) {
        case L'&':  r += L"&amp;";  break;
        case L'<':  r += L"&lt;";   break;
        case L'>':  r += L"&gt;";   break;
        case L'"':  r += L"&quot;"; break;
        default:    r += c;
        }
    }
    return r;
}

void XmlWriter::Open(const wchar_t* tag,
                     std::initializer_list<std::pair<const wchar_t*, std::wstring>> attrs)
{
    Indent();
    m_ss << L'<' << tag;
    for (auto& [k, v] : attrs)
        m_ss << L' ' << k << L"=\"" << Escape(v) << L'"';
    m_ss << L">\r\n";
    ++m_depth;
}

void XmlWriter::Close(const wchar_t* tag)
{
    --m_depth;
    Indent();
    m_ss << L"</" << tag << L">\r\n";
}

void XmlWriter::EmptyElement(const wchar_t* tag,
                              std::initializer_list<std::pair<const wchar_t*, std::wstring>> attrs)
{
    Indent();
    m_ss << L'<' << tag;
    for (auto& [k, v] : attrs)
        m_ss << L' ' << k << L"=\"" << Escape(v) << L'"';
    m_ss << L"/>\r\n";
}

std::wstring XmlWriter::ToString() const { return m_ss.str(); }

bool XmlWriter::WriteFile(const wchar_t* path) const
{
    std::wofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    // Coherente con la cabecera "<?xml encoding=UTF-8?>": forzar UTF-8 al
    // escribir, sin depender de la cp del sistema.
#pragma warning(push)
#pragma warning(disable: 4996)
    f.imbue(std::locale(f.getloc(), new std::codecvt_utf8<wchar_t>));
#pragma warning(pop)

    f << m_ss.str();
    return f.good();
}
