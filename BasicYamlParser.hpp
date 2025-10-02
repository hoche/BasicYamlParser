/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */

#pragma once

#include <algorithm> // std::equal
#include <cassert>
#include <cctype>
#include <charconv>
#include <climits>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <ios> // For std::streamoff, std::ios
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <regex> // For folding if needed, but manual
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits> // For std::is_same_v, std::is_integral_v, etc.
#include <utility>
#include <variant>
#include <vector>

enum class YamlNodeType { Scalar, Sequence, Mapping };

enum class ScalarStyle { Plain, Literal, Folded };

/**
 * @brief Represents a node in the YAML AST.
 *
 * Supports scalar values (with optional type deduction), sequences (lists), and mappings (dicts).
 * For scalars, raw scalarValue is stored; use convenience API for typed access.
 */
struct YamlNode {
    YamlNodeType type;
    ScalarStyle style = ScalarStyle::Plain;
    std::string scalarValue;
    std::vector<YamlNode> sequence;
    std::map<std::string, YamlNode> mapping;

    explicit YamlNode(YamlNodeType t = YamlNodeType::Scalar) : type(t) {}
};

/**
 * @brief Custom error for YAML parsing issues, including position info.
 */
struct YamlError : std::runtime_error {
    int line = -1;
    int col = -1;
    YamlError(const std::string &msg, int ln = -1, int cl = -1) : std::runtime_error(msg), line(ln), col(cl) {}
};

class YamlParser
{
  public:
    /**
     * @brief Parse YAML from a string.
     * @param input The YAML text.
     * @return Root YamlNode (always Mapping).
     * @throws YamlError on parse failure.
     */
    static YamlNode parse(const std::string &input)
    {
        std::stringstream ss(input);
        return parseStream(ss);
    }

    /**
     * @brief Parse YAML from a file.
     * @param filename Path to YAML file.
     * @return Root YamlNode.
     * @throws YamlError if file open fails or parse error.
     */
    static YamlNode parseFile(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw YamlError("Cannot open file: " + filename);
        }
        return parseStream(file);
    }

    /**
     * @brief Pretty-print the YAML structure to stdout.
     * @param node The node to print.
     * @param indent Current indentation level.
     */
    static void printYamlNode(const YamlNode &node, int indent = 0)
    {
        std::string indentStr(indent * 2, ' ');

        switch (node.type) {
        case YamlNodeType::Scalar:
            if (node.style == ScalarStyle::Literal) {
                std::cout << indentStr << "|" << std::endl;
                std::istringstream iss(node.scalarValue);
                std::string subline;
                int subindent = indent + 1;
                std::string subIndentStr(subindent * 2, ' ');
                while (std::getline(iss, subline)) {
                    std::cout << subIndentStr << subline << std::endl;
                }
            } else if (node.style == ScalarStyle::Folded) {
                std::cout << indentStr << ">" << std::endl;
                std::istringstream iss(node.scalarValue);
                std::string subline;
                int subindent = indent + 1;
                std::string subIndentStr(subindent * 2, ' ');
                while (std::getline(iss, subline)) {
                    std::cout << subIndentStr << subline << std::endl;
                }
            } else {
                std::cout << indentStr << node.scalarValue << std::endl;
            }
            break;
        case YamlNodeType::Sequence:
            for (const auto &item : node.sequence) {
                std::cout << indentStr << "- ";
                if (item.type == YamlNodeType::Scalar) {
                    std::cout << item.scalarValue << std::endl;
                } else {
                    std::cout << std::endl;
                    printYamlNode(item, indent + 1);
                }
            }
            break;
        case YamlNodeType::Mapping:
            for (const auto &pair : node.mapping) {
                const std::string &key = pair.first;
                const YamlNode &value = pair.second;
                if (value.type == YamlNodeType::Scalar) {
                    std::cout << indentStr << key << ": " << value.scalarValue << std::endl;
                } else {
                    std::cout << indentStr << key << ":" << std::endl;
                    printYamlNode(value, indent + 1);
                }
            }
            break;
        }
    }

    /**
     * @brief Deduce YAML scalar type (string, int, double, bool, null).
     * @param val The scalar string.
     * @return Typed variant; std::monostate for null/~.
     */
    static std::variant<std::monostate, std::string, long, double, bool> deduceType(const std::string &val)
    {
        std::size_t pos{};
        try {
            const long l{std::stol(val, &pos)};
            if (pos == val.length()) {
                return l;
            }
        } catch (...) {
        }
        try {
            const double d{std::stod(val, &pos)};
            if (pos == val.length()) {
                return d;
            }
        } catch (...) {
        }
        if (iequals(val, "Yes") || iequals(val, "True") || iequals(val, "on") || iequals(val, "true")) {
            return true;
        }
        if (iequals(val, "No") || iequals(val, "False") || iequals(val, "off") || iequals(val, "false")) {
            return false;
        }
        if (iequals(val, "null") || val == "~" || val.empty()) {
            return std::monostate{};
        }
        return val;
    }

    // ============================================================================
    // Convenience API: ergonomic accessors, path lookup, typed reads,
    // and emission helpers.
    // ============================================================================

    /**
     * @brief Case-insensitive string equality.
     */
    static bool iequals(const std::string &a, const std::string &b)
    {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
        });
    }

    // ---- Predicates ----
    static bool isScalar(const YamlNode &n) { return n.type == YamlNodeType::Scalar; }
    static bool isMap(const YamlNode &n) { return n.type == YamlNodeType::Mapping; }
    static bool isSeq(const YamlNode &n) { return n.type == YamlNodeType::Sequence; }

    // ---- Accessors (throwing) ----
    static const std::string &asString(const YamlNode &n)
    {
        if (!isScalar(n))
            throw YamlError("YAML: node is not a scalar");
        return n.scalarValue;
    }
    static const std::map<std::string, YamlNode> &asMap(const YamlNode &n)
    {
        if (!isMap(n))
            throw YamlError("YAML: node is not a mapping");
        return n.mapping;
    }
    static const std::vector<YamlNode> &asSeq(const YamlNode &n)
    {
        if (!isSeq(n))
            throw YamlError("YAML: node is not a sequence");
        return n.sequence;
    }

    // ---- Try-conversions (updated for null handling and case-insensitivity) ----
    static std::optional<bool> toBool(const YamlNode &n)
    {
        if (!isScalar(n))
            return std::nullopt;
        const std::string &val = n.scalarValue;
        if (iequals(val, "null") || val == "~" || val.empty())
            return std::nullopt;
        if (iequals(val, "true") || iequals(val, "yes") || iequals(val, "on"))
            return true;
        if (iequals(val, "false") || iequals(val, "no") || iequals(val, "off"))
            return false;
        return std::nullopt;
    }
    static std::optional<long long> toInt(const YamlNode &n)
    {
        if (!isScalar(n))
            return std::nullopt;
        const std::string &s = n.scalarValue;
        if (iequals(s, "null") || s == "~" || s.empty())
            return std::nullopt;
        long long v = 0;
        auto *b = s.data();
        auto *e = b + s.size();
        auto [ptr, ec] = std::from_chars(b, e, v);
        if (ec == std::errc{} && ptr == e)
            return v;
        return std::nullopt;
    }
    static std::optional<double> toDouble(const YamlNode &n)
    {
        if (!isScalar(n))
            return std::nullopt;
        const std::string &s = n.scalarValue;
        if (iequals(s, "null") || s == "~" || s.empty())
            return std::nullopt;
        errno = 0;
        char *endp = nullptr;
        double d = std::strtod(s.c_str(), &endp);
        if (errno == 0 && endp && *endp == '\0')
            return d;
        return std::nullopt;
    }

    /**
     * @brief Lightweight view with path lookup.
     */
    struct NodeView {
        const YamlNode *n{nullptr};

        explicit operator bool() const { return n != nullptr; }
        bool is_scalar() const { return n && n->type == YamlNodeType::Scalar; }
        bool is_map() const { return n && n->type == YamlNodeType::Mapping; }
        bool is_seq() const { return n && n->type == YamlNodeType::Sequence; }

        const std::string &as_str() const { return YamlParser::asString(*n); }
        const std::map<std::string, YamlNode> &as_map() const { return YamlParser::asMap(*n); }
        const std::vector<YamlNode> &as_seq() const { return YamlParser::asSeq(*n); }

        std::optional<bool> to_bool() const { return YamlParser::toBool(*n); }
        std::optional<long long> to_int() const { return YamlParser::toInt(*n); }
        std::optional<double> to_double() const { return YamlParser::toDouble(*n); }

        NodeView operator[](const std::string &key) const
        {
            if (!is_map())
                return {};
            auto it = n->mapping.find(key);
            if (it == n->mapping.end())
                return {};
            return NodeView{&it->second};
        }
        NodeView operator[](size_t idx) const
        {
            if (!is_seq())
                return {};
            if (idx >= n->sequence.size())
                return {};
            return NodeView{&n->sequence[idx]};
        }

        // Very simple path: "a.b[2].c"
        NodeView at_path(std::string_view path) const
        {
            const char *p = path.data();
            const char *end = p + path.size();
            NodeView cur{n};
            std::string token;
            while (p < end) {
                if (*p == '.') {
                    ++p;
                    continue;
                }
                token.clear();
                while (p < end && *p != '.' && *p != '[') {
                    token.push_back(*p++);
                }
                if (!token.empty()) {
                    cur = cur[token];
                    if (!cur)
                        return {};
                }
                if (p < end && *p == '[') {
                    ++p;
                    size_t idx = 0;
                    while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
                        idx = idx * 10 + (*p - '0');
                        ++p;
                    }
                    if (p == end || *p != ']')
                        return {};
                    ++p;
                    cur = cur[idx];
                    if (!cur)
                        return {};
                }
            }
            return cur;
        }

        template <class T> T value(std::string_view path, T def) const
        {
            NodeView v = at_path(path);
            if (!v)
                return def;
            if constexpr (std::is_same_v<T, std::string>) {
                return v.is_scalar() ? v.as_str() : def;
            } else if constexpr (std::is_same_v<T, const char *>) {
                return v.is_scalar() ? v.as_str().c_str() : def;
            } else if constexpr (std::is_same_v<T, bool>) {
                auto b = v.to_bool();
                return b ? *b : def;
            } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                auto i = v.to_int();
                return i ? static_cast<T>(*i) : def;
            } else if constexpr (std::is_floating_point_v<T>) {
                auto d = v.to_double();
                return d ? static_cast<T>(*d) : def;
            } else {
                return def;
            }
        }
    };

    // ---- Emission to YAML ----
    static void emitYaml(const YamlNode &node, std::ostream &os, int indent = 0)
    {
        std::string ind(indent * 2, ' ');
        switch (node.type) {
        case YamlNodeType::Scalar:
        {
            if (node.style == ScalarStyle::Literal && node.scalarValue.find('\n') != std::string::npos) {
                os << ind << "|" << std::endl;
                std::istringstream iss(node.scalarValue);
                std::string subline;
                int subindent = indent + 1;
                std::string subInd(subindent * 2, ' ');
                while (std::getline(iss, subline)) {
                    os << subInd << subline << std::endl;
                }
            } else if (node.style == ScalarStyle::Folded && node.scalarValue.find('\n') != std::string::npos) {
                os << ind << ">" << std::endl;
                std::istringstream iss(node.scalarValue);
                std::string subline;
                int subindent = indent + 1;
                std::string subInd(subindent * 2, ' ');
                while (std::getline(iss, subline)) {
                    os << subInd << subline << std::endl;
                }
            } else {
                os << ind << node.scalarValue;
            }
        } break;
        case YamlNodeType::Sequence:
        {
            bool allScalar = std::all_of(node.sequence.begin(), node.sequence.end(), [](const YamlNode &it) {
                return it.type == YamlNodeType::Scalar && it.style == ScalarStyle::Plain;
            });
            if (allScalar && node.sequence.size() <= 5) {
                os << ind << "[";
                for (size_t i = 0; i < node.sequence.size(); ++i) {
                    if (i > 0)
                        os << ", ";
                    os << node.sequence[i].scalarValue;
                }
                os << "]" << "\n";
            } else {
                for (const auto &item : node.sequence) {
                    os << ind << "- ";
                    if (item.type == YamlNodeType::Scalar && item.style == ScalarStyle::Plain) {
                        os << item.scalarValue << "\n";
                    } else {
                        os << "\n";
                        emitYaml(item, os, indent + 1);
                    }
                }
            }
        } break;
        case YamlNodeType::Mapping:
        {
            bool allScalar = std::all_of(node.mapping.begin(), node.mapping.end(), [](const auto &kv) {
                return kv.second.type == YamlNodeType::Scalar && kv.second.style == ScalarStyle::Plain;
            });
            if (allScalar && node.mapping.size() <= 5) {
                os << ind << "{";
                bool first = true;
                for (const auto &kv : node.mapping) {
                    if (!first)
                        os << ", ";
                    os << kv.first << ": " << kv.second.scalarValue;
                    first = false;
                }
                os << "}" << "\n";
            } else {
                for (const auto &kv : node.mapping) {
                    os << ind << kv.first << ": ";
                    if (kv.second.type == YamlNodeType::Scalar && kv.second.style == ScalarStyle::Plain) {
                        os << kv.second.scalarValue << "\n";
                    } else {
                        os << "\n";
                        emitYaml(kv.second, os, indent + 1);
                    }
                }
            }
        } break;
        }
    }
    static std::string toYamlString(const YamlNode &node)
    {
        std::ostringstream oss;
        emitYaml(node, oss, 0);
        return oss.str();
    }

    /**
     * @brief Convenience loader returning a Document view.
     */
    struct Document {
        YamlNode root;
        NodeView view() const { return NodeView{&root}; }
    };
    static Document loadFile(const std::string &filename) { return Document{parseFile(filename)}; }
    static Document loadString(const std::string &text) { return Document{parse(text)}; }

    /**
     * @brief Trim leading/trailing whitespace.
     */
    static std::string trim(const std::string &str)
    {
        size_t first = str.find_first_not_of(" \t");
        size_t last = str.find_last_not_of(" \t");
        if (first == std::string::npos)
            return "";
        return str.substr(first, last - first + 1);
    }

    /**
     * @brief Trim trailing whitespace.
     */
    static std::string rtrim(const std::string &str)
    {
        size_t last = str.find_last_not_of(" \t");
        if (last == std::string::npos)
            return "";
        return str.substr(0, last + 1);
    }

    /**
     * @brief Trim leading whitespace only.
     */
    static std::string ltrim(const std::string &str, size_t count)
    {
        if (str.size() <= count)
            return "";
        return str.substr(count);
    }

  private:
    /**
     * @brief Get indentation level, throwing on tabs.
     * @throws YamlError if tabs detected.
     */
    static int getIndent(const std::string &line, int lineNum)
    {
        int indent = 0;
        for (char c : line) {
            if (c == ' ') {
                ++indent;
            } else if (c == '\t') {
                throw YamlError("Tabs not allowed in YAML indentation", lineNum, indent + 1);
            } else {
                break;
            }
        }
        return indent;
    }

    /**
     * @brief Count leading spaces (no throw on tabs).
     */
    static int countLeadingSpaces(const std::string &line)
    {
        int indent = 0;
        for (char c : line) {
            if (c == ' ') {
                ++indent;
            } else {
                break;
            }
        }
        return indent;
    }

    /**
     * @brief Unescape quoted strings (basic: \n, \t, \\, \', \").
     */
    static std::string unescape(const std::string &s)
    {
        std::string result;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                switch (s[++i]) {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '\'':
                    result += '\'';
                    break;
                case '"':
                    result += '"';
                    break;
                default:
                    result += s[i];
                    break;
                }
            } else {
                result += s[i];
            }
        }
        return result;
    }

    /**
     * @brief Parse inline flow sequence [item1, item2].
     * @param flowContent Content inside [].
     * @return YamlNode as Sequence.
     */
    static YamlNode parseFlowSequence(const std::string &flowContent, int lineNum)
    {
        YamlNode seq(YamlNodeType::Sequence);
        std::istringstream iss(flowContent);
        std::string item;
        while (std::getline(iss, item, ',')) {
            std::string val = trim(item);
            if (!val.empty()) {
                if ((val[0] == '"' || val[0] == '\'') && val.size() > 1 && val.back() == val[0]) {
                    val = unescape(val.substr(1, val.size() - 2));
                }
                YamlNode node(YamlNodeType::Scalar);
                node.scalarValue = val;
                seq.sequence.push_back(node);
            }
        }
        return seq;
    }

    /**
     * @brief Parse inline flow mapping {key: val, ...}.
     * @param flowContent Content inside {}.
     * @return YamlNode as Mapping.
     * @throws YamlError on invalid format.
     */
    static YamlNode parseFlowMapping(const std::string &flowContent, int lineNum)
    {
        YamlNode map(YamlNodeType::Mapping);
        std::istringstream iss(flowContent);
        std::string pair;
        while (std::getline(iss, pair, ',')) {
            size_t colon = pair.find(':');
            if (colon == std::string::npos) {
                throw YamlError("Invalid flow mapping pair (missing colon): " + pair, lineNum);
            }
            std::string key = trim(pair.substr(0, colon));
            std::string val = trim(pair.substr(colon + 1));
            if (key.empty()) {
                throw YamlError("Empty key in flow mapping", lineNum);
            }
            if ((val[0] == '"' || val[0] == '\'') && val.size() > 1 && val.back() == val[0]) {
                val = unescape(val.substr(1, val.size() - 2));
            }
            YamlNode node(YamlNodeType::Scalar);
            node.scalarValue = val;
            map.mapping[key] = node;
        }
        return map;
    }

    /**
     * @brief Parse block scalar content (for | and >).
     * @param baseIndent The indent of the header line.
     * @param lineNum Current line number (updated).
     * @param input The input stream.
     * @return The raw scalar content (without chomp applied).
     */
    static std::string parseBlockScalar(int baseIndent, int &lineNum, std::istream &input)
    {
        std::vector<std::string> blockLines;
        std::string line;
        bool hasContent = false;
        int minDelta = INT_MAX;

        while (std::getline(input, line)) {
            ++lineNum;
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos)
                line.resize(commentPos);
            int nextIndent = getIndent(line, lineNum);
            std::string trimmed = trim(line);
            if (trimmed.empty()) {
                if (hasContent) {
                    blockLines.push_back(""); // Keep trailing blank for chomp
                }
                continue; // Skip for minDelta
            }

            if (nextIndent <= baseIndent) {
                // End of block, unconsume this line
                input.seekg(-(static_cast<std::streamoff>(line.size()) + 1), std::ios::cur);
                --lineNum;
                break;
            }

            int delta = nextIndent - baseIndent;
            if (delta < minDelta)
                minDelta = delta;
            hasContent = true;
            blockLines.push_back(line.substr(nextIndent));
        }

        if (!hasContent)
            minDelta = 1; // Arbitrary

        std::string content;
        for (const auto &ln : blockLines) {
            content += ln + "\n";
        }

        return content;
    }

    /**
     * @brief Apply chomp to block scalar content.
     * @param content The raw content.
     * @param chomp The chomp indicator ('-', '+', or default ' ' for clip).
     * @return Chomped content.
     */
    static std::string applyChomp(std::string content, char chomp)
    {
        if (content.empty())
            return content;
        size_t lastNonNewline = content.find_last_not_of('\n');
        if (lastNonNewline == std::string::npos) {
            return "";
        }
        std::string base = content.substr(0, lastNonNewline + 1);
        if (chomp == '-') {
            return base;
        } else if (chomp == '+') {
            return content;
        } else {
            // Clip: keep at most one newline
            return base + "\n";
        }
    }

    /**
     * @brief Fold block scalar content.
     * @param content The chomped content.
     * @return Folded string.
     */
    static std::string foldBlock(const std::string &content)
    {
        std::string folded;
        std::istringstream iss(content);
        std::string line;
        bool first = true;
        bool newParagraph = false;
        while (std::getline(iss, line)) {
            if (line.empty()) {
                newParagraph = true;
                continue;
            }
            if (!first) {
                if (newParagraph) {
                    folded += "\n";
                } else {
                    folded += " ";
                }
            }
            folded += rtrim(line);
            first = false;
            newParagraph = false;
        }
        return folded;
    }

    /**
     * @brief Core parser from input stream.
     */
    static YamlNode parseStream(std::istream &input)
    {
        YamlNode root(YamlNodeType::Mapping);
        std::string line;
        std::vector<std::pair<YamlNode *, int>> stack = {{&root, -1}};
        YamlNode *lastScalarNode = nullptr;
        int linecount{0};

        while (std::getline(input, line)) {
            ++linecount;
            if (size_t commentPos = line.find('#'); commentPos != std::string::npos)
                line.resize(commentPos);
            if (line.empty() || trim(line).empty())
                continue;

            int indent = getIndent(line, linecount);
            std::string content = trim(line.substr(indent));

            while (stack.size() > 1 && indent <= stack.back().second) {
                stack.pop_back();
            }
            if (stack.empty()) {
                throw YamlError("Invalid indentation or structure near: " + content, linecount);
            }

            YamlNode *currentNode = stack.back().first;

            if (indent > stack.back().second && lastScalarNode) {
                throw YamlError("Unexpected indentation after scalar value", linecount);
            }

            if (content[0] == '-') {
                if (currentNode->type != YamlNodeType::Sequence) {
                    currentNode->type = YamlNodeType::Sequence;
                    currentNode->sequence.clear();
                }

                std::string itemValue = trim(content.substr(1));
                YamlNode item;

                if (!itemValue.empty()) {
                    size_t colonPos = itemValue.find(':');
                    if (colonPos != std::string::npos) {
                        std::string key = trim(itemValue.substr(0, colonPos));
                        std::string val = trim(itemValue.substr(colonPos + 1));
                        item.type = YamlNodeType::Mapping;
                        if (!key.empty()) {
                            item.mapping[key] = YamlNode(YamlNodeType::Scalar);
                            item.mapping[key].scalarValue = val;
                        }
                    } else {
                        item.type = YamlNodeType::Scalar;
                        item.scalarValue = itemValue;
                    }
                } else {
                    item.type = YamlNodeType::Mapping;
                }

                currentNode->sequence.push_back(item);
                if (item.type == YamlNodeType::Mapping && itemValue.empty()) {
                    stack.push_back({&currentNode->sequence.back(), indent});
                }
            } else {
                size_t colonPos = content.find(':');
                if (colonPos == std::string::npos) {
                    if (lastScalarNode) {
                        lastScalarNode->scalarValue += "\n" + trim(line);
                        continue;
                    } else {
                        throw YamlError("Invalid mapping format (missing colon) on line " + std::to_string(linecount) +
                                            ": " + content,
                                        linecount);
                    }
                }

                std::string key = trim(content.substr(0, colonPos));
                std::string value = trim(content.substr(colonPos + 1));

                if (key.empty()) {
                    throw YamlError("Invalid mapping format (empty key): " + content, linecount);
                }

                if (currentNode->type != YamlNodeType::Mapping) {
                    currentNode->type = YamlNodeType::Mapping;
                    currentNode->sequence.clear();
                }

                YamlNode newNode;
                if (!value.empty()) {
                    // Handle block scalar
                    char indicator = 0;
                    char chomp = ' ';
                    if (value[0] == '|' || value[0] == '>') {
                        indicator = value[0];
                        if (value.size() > 1 && (value[1] == '-' || value[1] == '+')) {
                            chomp = value[1];
                            value = value.substr(2);
                        } else {
                            value = value.substr(1);
                        }
                        std::string block = parseBlockScalar(indent, linecount, input);
                        newNode.type = YamlNodeType::Scalar;
                        newNode.style = (indicator == '|' ? ScalarStyle::Literal : ScalarStyle::Folded);
                        if (newNode.style == ScalarStyle::Folded) {
                            block = foldBlock(block);
                        }
                        newNode.scalarValue = applyChomp(block, chomp);
                        currentNode->mapping[key] = newNode;
                        lastScalarNode = nullptr;
                        continue;
                    }

                    // Handle quoted scalars
                    char quoteType = '\0';
                    bool wasQuoted = false;
                    if ((value[0] == '"' || value[0] == '\'') && value.size() > 1) {
                        quoteType = value[0];
                        if (value.back() == quoteType) {
                            value = unescape(value.substr(1, value.size() - 2));
                            wasQuoted = true;
                        }
                    }

                    // Check for ambiguous colon in unquoted value
                    if (!wasQuoted && value.find(": ") != std::string::npos) {
                        throw YamlError("Unquoted value contains ': ' - use quotes to avoid ambiguity", linecount);
                    }

                    // Handle flow styles (after potential quote strip)
                    value = trim(value); // Re-trim after unescape
                    if (value[0] == '[' && value.back() == ']') {
                        newNode = parseFlowSequence(value.substr(1, value.size() - 2), linecount);
                    } else if (value[0] == '{' && value.back() == '}') {
                        newNode = parseFlowMapping(value.substr(1, value.size() - 2), linecount);
                    } else {
                        newNode.type = YamlNodeType::Scalar;
                        newNode.scalarValue = value;
                    }
                    currentNode->mapping[key] = newNode;
                    lastScalarNode = (newNode.type == YamlNodeType::Scalar) ? &currentNode->mapping[key] : nullptr;
                } else {
                    std::streampos pos = input.tellg();
                    std::string peekLine;
                    YamlNodeType inferredType = YamlNodeType::Mapping;
                    if (std::getline(input, peekLine)) {
                        int peekIndent = getIndent(peekLine, linecount + 1);
                        std::string peekContent = trim(peekLine.substr(peekIndent));
                        if (!peekContent.empty() && peekContent[0] == '-') {
                            inferredType = YamlNodeType::Sequence;
                        }
                        input.seekg(pos);
                    }
                    newNode.type = inferredType;
                    currentNode->mapping[key] = newNode;
                    stack.push_back({&currentNode->mapping[key], indent});
                    lastScalarNode = nullptr;
                }
            }
        }

        return root;
    }
};
