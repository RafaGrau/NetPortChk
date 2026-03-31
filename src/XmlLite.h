#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

// ──────────────────────────────────────────────────────────────────────────────
// Minimal XML tree: no COM, no MSXML, no external library.
// Supports: elements, attributes, text content, depth-first traversal.
// ──────────────────────────────────────────────────────────────────────────────

struct XmlNode
{
    std::wstring                          tag;
    std::wstring                          text;        // inner text (trimmed)
    std::map<std::wstring, std::wstring>  attrs;
    std::vector<std::shared_ptr<XmlNode>> children;

    // Attribute helpers
    const std::wstring& Attr(const wchar_t* name, const std::wstring& def = L"") const;
    int  AttrInt (const wchar_t* name, int  def = 0)    const;
    bool AttrBool(const wchar_t* name, bool def = false) const;

    // Child helpers
    std::shared_ptr<XmlNode>              FirstChild(const wchar_t* tag) const;
    std::vector<std::shared_ptr<XmlNode>> Children  (const wchar_t* tag) const;
};

// ──────────────────────────────────────────────────────────────────────────────
// Parser  – returns root node or nullptr on error
// ──────────────────────────────────────────────────────────────────────────────
std::shared_ptr<XmlNode> XmlParse(const std::wstring& xml);
std::shared_ptr<XmlNode> XmlParseFile(const wchar_t* path);

// ──────────────────────────────────────────────────────────────────────────────
// Writer  – builds indented XML string
// ──────────────────────────────────────────────────────────────────────────────
class XmlWriter
{
public:
    XmlWriter();

    void Open (const wchar_t* tag,
               std::initializer_list<std::pair<const wchar_t*, std::wstring>> attrs = {});
    void Close(const wchar_t* tag);
    void EmptyElement(const wchar_t* tag,
                      std::initializer_list<std::pair<const wchar_t*, std::wstring>> attrs = {});

    std::wstring ToString() const;
    bool         WriteFile(const wchar_t* path) const;

private:
    std::wostringstream m_ss;
    int                 m_depth { 0 };

    void Indent();
    static std::wstring Escape(const std::wstring& s);
};
