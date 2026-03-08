#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace obsidian {

std::vector<Token> lex(const std::string& source);

}  // namespace obsidian
