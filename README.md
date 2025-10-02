# BasicYamlParser.hpp

Single-include YAML parser.

Doesn't do everything, but I'm working on it.

For stuff it doesn't do, see the disabled tests. :)


## Basic Parsing from a String

```
#include "BasicYamlParser.hpp"
#include <iostream>

int main() {
    auto doc = YamlParser::loadString(R"(
name: Alice
age: 30
hobbies:
  - reading
  - coding
scores: [85, 92, 78]
    )");

    YamlParser::NodeView view = doc.view();
    std::cout << "Name: " << view["name"].as_str() << std::endl;
    std::cout << "Age: " << *view["age"].to_int() << std::endl;

    auto hobbies = view["hobbies"];
    if (hobbies.is_seq()) {
        std::cout << "Hobbies:" << std::endl;
        for (size_t i = 0; i < hobbies.as_seq().size(); ++i) {
            std::cout << "- " << hobbies[i].as_str() << std::endl;
        }
    }

    return 0;
}
```

## Parsing from File and Path Lookup

```
#include "BasicYamlParser.hpp"
#include <iostream>

int main() {
    try {
        auto doc = YamlParser::loadFile("config.yaml");
        YamlParser::NodeView view = doc.view();
        std::cout << "Second score: " << *view.at_path("scores[1]").to_int() << std::endl;
        std::cout << "First hobby: " << view.value<std::string>("hobbies[0]", "none") << std::endl;
    } catch (const YamlError& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
```

## Building and Serializing YAML

```
#include "BasicYamlParser.hpp"
#include <iostream>

int main() {
    YamlNode root(YamlNodeType::Mapping);
    root.mapping["name"] = YamlNode(YamlNodeType::Scalar);
    root.mapping["name"].scalarValue = "Bob";

    YamlNode hobbies(YamlNodeType::Sequence);
    hobbies.sequence.emplace_back(YamlNodeType::Scalar);
    hobbies.sequence.back().scalarValue = "gaming";
    hobbies.sequence.emplace_back(YamlNodeType::Scalar);
    hobbies.sequence.back().scalarValue = "music";
    root.mapping["hobbies"] = hobbies;

    std::string yaml = YamlParser::toYamlString(root);
    std::cout << yaml << std::endl;

    return 0;
}
```

## Handling Block Scalars

```
#include "BasicYamlParser.hpp"
#include <iostream>

int main() {
    auto doc = YamlParser::loadString(R"(
description: |
  Multi-line
  literal text.
  Preserves newlines.
    )");

    YamlParser::NodeView view = doc.view();
    std::cout << "Description:\n" << view["description"].as_str() << std::endl;

    return 0;
}
```

## Error Handling

```
#include "BasicYamlParser.hpp"
#include <iostream>

int main() {
    try {
        YamlParser::loadString("key: value without quotes containing: colon");
    } catch (const YamlError& e) {
        std::cerr << "Caught error: " << e.what() << std::endl;
    }
    return 0;
}
```


