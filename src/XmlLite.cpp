#include "pch.h"
#include "XmlLite.h"
#include <fstream>
#include <sstream>
#include <cctype>

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
    return std::stoi(it->second);
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
    std::wifstream f(path);
    if (!f.is_open()) return nullptr;
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
    std::wofstream f(path);
    if (!f.is_open()) return false;
    f << m_ss.str();
    return f.good();
}
