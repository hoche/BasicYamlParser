#include <iostream>
#include <sstream>

#include <gtest/gtest.h>
#include "BasicYamlParser.hpp"

// Helper to compare YAML strings (ignoring minor whitespace diffs)
std::string normalizeYaml(const std::string& yaml) {
    std::istringstream iss(yaml);
    std::ostringstream oss;
    std::string line;
    while (std::getline(iss, line)) {
        // Trim and skip empty lines for comparison
        std::string trimmed = YamlParser::trim(line);
        if (!trimmed.empty()) {
            oss << trimmed << "\n";
        }
    }
    return oss.str();
}

TEST(YamlParserBasic, SimpleScalar) {
    auto doc = YamlParser::loadString("value: hello");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["value"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.as_str(), "hello");
}

TEST(YamlParserBasic, IntegerScalar) {
    auto doc = YamlParser::loadString("count: 42");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["count"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.to_int(), 42LL);
}

TEST(YamlParserBasic, DoubleScalar) {
    auto doc = YamlParser::loadString("pi: 3.14");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["pi"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.to_double(), 3.14);
}

TEST(YamlParserBasic, BooleanScalar) {
    auto doc = YamlParser::loadString("active: true");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["active"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.to_bool(), true);

    auto doc_false = YamlParser::loadString("inactive: False");
    auto val_false = doc_false.view()["inactive"];
    EXPECT_EQ(val_false.to_bool(), false);
}

TEST(YamlParserBasic, NullScalar) {
    auto doc = YamlParser::loadString("null_val: null");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["null_val"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_FALSE(val.to_int().has_value());
    EXPECT_FALSE(val.to_bool().has_value());
    EXPECT_FALSE(val.to_double().has_value());

    auto doc_tilde = YamlParser::loadString("tilde_val: ~");
    auto val_tilde = doc_tilde.view()["tilde_val"];
    EXPECT_FALSE(val_tilde.to_int().has_value());

    auto doc_empty = YamlParser::loadString("empty_val: ");
    auto val_empty = doc_empty.view()["empty_val"];
    EXPECT_FALSE(val_empty.to_int().has_value());
}

TEST(YamlParserBasic, SimpleSequence) {
    auto doc = YamlParser::loadString(R"(
items:
  - apple
  - banana
  - cherry
    )");
    ASSERT_TRUE(doc.view().is_map());
    auto seq = doc.view()["items"];
    ASSERT_TRUE(seq.is_seq());
    EXPECT_EQ(seq.as_seq().size(), 3);
    EXPECT_EQ(seq[0].as_str(), "apple");
    EXPECT_EQ(seq[1].as_str(), "banana");
    EXPECT_EQ(seq[2].as_str(), "cherry");
}

TEST(YamlParserBasic, DISABLED_NestedMapping) {
    auto doc = YamlParser::loadString(R"(
person:
  name: Alice
  age: 30
  address:
    city: Wonderland
    zip: 12345
    )");
    ASSERT_TRUE(doc.view().is_map());
    auto person = doc.view()["person"];
    ASSERT_TRUE(person.is_map());
    EXPECT_EQ(person["name"].as_str(), "Alice");
    EXPECT_EQ(person["age"].to_int(), 30LL);

    auto addr = person["address"];
    ASSERT_TRUE(addr.is_map());
    EXPECT_EQ(addr["city"].as_str(), "Wonderland");
    EXPECT_EQ(addr["zip"].to_int(), 12345LL);
}

TEST(YamlParserBasic, PathLookup) {
    auto doc = YamlParser::loadString(R"(
a:
  b:
    - 1
    - 2
  c: three
    )");
    YamlParser::NodeView view = doc.view();
    EXPECT_EQ(view.value<int>("a.b[1]", -1), 2);
    EXPECT_EQ(view.value<std::string>("a.c", "default"), "three");
    EXPECT_EQ(view.value<int>("a.d", 99), 99);  // Missing path defaults
}

TEST(YamlParserFlow, InlineSequence) {
    auto doc = YamlParser::loadString("scores: [85, 92.5, 78]");
    ASSERT_TRUE(doc.view().is_map());
    auto seq = doc.view()["scores"];
    ASSERT_TRUE(seq.is_seq());
    EXPECT_EQ(seq.as_seq().size(), 3);
    EXPECT_EQ(seq[0].to_int(), 85LL);
    EXPECT_EQ(seq[1].to_double(), 92.5);
    EXPECT_EQ(seq[2].to_int(), 78LL);
}

TEST(YamlParserFlow, DISABLED_InlineMapping) {
    auto doc = YamlParser::loadString("config: {debug: true, level: 1, name: 'test'}");
    ASSERT_TRUE(doc.view().is_map());
    auto cfg = doc.view()["config"];
    ASSERT_TRUE(cfg.is_map());
    EXPECT_EQ(cfg["debug"].to_bool(), true);
    EXPECT_EQ(cfg["level"].to_int(), 1LL);
    EXPECT_EQ(cfg["name"].as_str(), "test");
}

TEST(YamlParserFlow, DISABLED_NestedFlow) {
    auto doc = YamlParser::loadString("data: {list: [1, {x: 10, y: 20}], flag: false}");
    ASSERT_TRUE(doc.view().is_map());
    auto data = doc.view()["data"];
    ASSERT_TRUE(data.is_map());
    auto lst = data["list"];
    ASSERT_TRUE(lst.is_seq());
    EXPECT_EQ(lst.as_seq().size(), 2);
    EXPECT_EQ(lst[0].to_int(), 1LL);
    auto nested_map = lst[1];
    ASSERT_TRUE(nested_map.is_map());
    EXPECT_EQ(nested_map["x"].to_int(), 10LL);
    EXPECT_EQ(nested_map["y"].to_int(), 20LL);
    EXPECT_EQ(data["flag"].to_bool(), false);
}

TEST(YamlParserQuoted, SimpleQuoted) {
    auto doc = YamlParser::loadString(R"(key: "hello: world")");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["key"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.as_str(), "hello: world");  // Colon preserved
}

TEST(YamlParserQuoted, EscapedQuoted) {
    auto doc = YamlParser::loadString(R"(msg: "Line one\nLine two")");
    ASSERT_TRUE(doc.view().is_map());
    auto val = doc.view()["msg"];
    ASSERT_TRUE(val.is_scalar());
    EXPECT_EQ(val.as_str(), "Line one\nLine two");  // Escape handled

    auto doc_single = YamlParser::loadString(R"(single: 'It\'s quoted')");
    auto val_single = doc_single.view()["single"];
    EXPECT_EQ(val_single.as_str(), "It's quoted");
}

TEST(YamlParserQuoted, UnquotedWithColon) {
    // Should fail without quotes
    EXPECT_THROW(YamlParser::loadString("key: hello: world"), YamlError);
}

TEST(YamlParserBlock, LiteralScalar) {
    auto doc = YamlParser::loadString(R"(
description: |
  This is a multi-line
  description with indentation.
  It preserves newlines.
  Last line.
    )");
    ASSERT_TRUE(doc.view().is_map());
    auto desc = doc.view()["description"];
    ASSERT_TRUE(desc.is_scalar());
    EXPECT_EQ(desc.as_str(), "This is a multi-line\ndescription with indentation.\nIt preserves newlines.\nLast line.\n");
}

TEST(YamlParserBlock, LiteralWithEmptyLines) {
    auto doc = YamlParser::loadString(R"(
notes: |
  First line.

  Third line after empty.

  Last line.
    )");
    auto notes = doc.view()["notes"];
    EXPECT_EQ(notes.as_str(), "First line.\n\nThird line after empty.\n\nLast line.\n");
}

TEST(YamlParserBlock, DISABLED_FoldedScalarBasic) {
    auto doc = YamlParser::loadString(R"(
summary: >
  This is a folded scalar.
  It folds newlines into spaces.
  Multiple spaces are preserved where indented.
    )");
    ASSERT_TRUE(doc.view().is_map());
    auto summary = doc.view()["summary"];
    ASSERT_TRUE(summary.is_scalar());
    // Expect: "This is a folded scalar. It folds newlines into spaces. Multiple spaces are preserved where indented."
    EXPECT_EQ(summary.as_str(), "This is a folded scalar. It folds newlines into spaces. Multiple spaces are preserved where indented. ");
}

TEST(YamlParserBlock, DISABLED_FoldedScalarWithParagraphs) {
    auto doc = YamlParser::loadString(R"(
description: >
  First paragraph.

  Second paragraph with more
  text that folds.

  Final line.
    )");
    ASSERT_TRUE(doc.view().is_map());
    auto desc = doc.view()["description"];
    ASSERT_TRUE(desc.is_scalar());
    // Expect: "First paragraph.\n\nSecond paragraph with more text that folds.\n\nFinal line. "
    EXPECT_EQ(desc.as_str(), "First paragraph.\n\nSecond paragraph with more text that folds.\n\nFinal line. ");
}

TEST(YamlParserBlock, FoldedScalarChompStrip) {
    auto doc = YamlParser::loadString(R"(
notes: >-
  Note with trailing newline stripped.
  Last line without trailing space.
    )");
    auto notes = doc.view()["notes"];
    // Assuming >- strips final newline; adjust based on implementation
    EXPECT_EQ(notes.as_str(), "Note with trailing newline stripped. Last line without trailing space.");
}

TEST(YamlParserBlock, FoldedScalarErrorNoIndent) {
    // Expect throw if continuation not indented
    EXPECT_THROW(YamlParser::loadString(R"(
err: >
non-indented line
    )"), YamlError);
}

TEST(YamlParserError, TabsInIndent) {
    EXPECT_THROW(YamlParser::loadString("key:\n\tvalue: 1"), YamlError);
}

TEST(YamlParserError, InvalidIndent) {
    EXPECT_THROW(YamlParser::loadString(R"(
a: 1
  b: 2
c: 3
    )"), YamlError);  // c at wrong indent
}

TEST(YamlParserError, MissingColon) {
    EXPECT_THROW(YamlParser::loadString("key value"), YamlError);
}

TEST(YamlParserError, EmptyKey) {
    EXPECT_THROW(YamlParser::loadString(": value"), YamlError);
}

TEST(YamlParserError, InvalidFlow) {
    EXPECT_THROW(YamlParser::loadString("map: {a: 1, b}"), YamlError);  // Missing value
}

TEST(YamlParserEmission, DISABLED_RoundTrip) {
    std::string original = R"(
name: Alice
items:
  - one
  - two
config:
  debug: true
  scores: [10, 20]
    )";
    auto doc = YamlParser::loadString(original);
    std::string emitted = YamlParser::toYamlString(doc.root);
    // Normalize and compare
    EXPECT_EQ(normalizeYaml(emitted), normalizeYaml(original));
}

TEST(YamlParserEmission, DISABLED_BlockLiteral) {
    auto doc = YamlParser::loadString(R"(
desc: |
  Multi
  line
    )");
    std::string emitted = YamlParser::toYamlString(doc.root);
    EXPECT_TRUE(emitted.find("desc: |") != std::string::npos);
    EXPECT_TRUE(emitted.find("Multi") != std::string::npos);
    EXPECT_TRUE(emitted.find("line") != std::string::npos);
}

TEST(YamlParserEmission, DISABLED_FoldedScalar) {
    auto doc = YamlParser::loadString(R"(
sum: >
  Folded
  content
    )");
    std::string emitted = YamlParser::toYamlString(doc.root);
    // Assuming emission uses > for folded
    EXPECT_TRUE(emitted.find("sum: >") != std::string::npos);
    EXPECT_TRUE(emitted.find("Folded") != std::string::npos);
    EXPECT_TRUE(emitted.find("content") != std::string::npos);
}

TEST(YamlParserConvenience, ValueWithDefault) {
    auto doc = YamlParser::loadString("present: 42");
    YamlParser::NodeView view = doc.view();
    EXPECT_EQ(view.value<int>("present", 0), 42);
    EXPECT_EQ(view.value<std::string>("missing", "def"), "def");
    EXPECT_EQ(view.value<bool>("missing", true), true);
}

TEST(YamlParserDeduceType, Variants) {
    EXPECT_TRUE(std::holds_alternative<long>(YamlParser::deduceType("123")));
    EXPECT_TRUE(std::holds_alternative<double>(YamlParser::deduceType("3.14")));
    EXPECT_TRUE(std::holds_alternative<bool>(YamlParser::deduceType("True")));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(YamlParser::deduceType("null")));
    EXPECT_TRUE(std::holds_alternative<std::string>(YamlParser::deduceType("hello")));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

