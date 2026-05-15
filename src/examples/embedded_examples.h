#pragma once

#include <cstddef>
#include <string>

namespace embedded_examples {

struct AssemblyExample {
    const char* id = "";
    const char* title = "";
    const char* description = "";
    const char* source = "";
};

const AssemblyExample* examples();
std::size_t exampleCount();
const AssemblyExample* exampleAt(std::size_t index);
const AssemblyExample* findExample(const std::string& id);

} // namespace embedded_examples
