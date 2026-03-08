#pragma once

#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <stdexcept>

namespace obsidian {

// Parses a single statement. Consumes tokens up to and including the semicolon.
// Throws std::runtime_error on syntax errors.
Statement parse_statement(const std::vector<Token>& tokens, std::size_t& index);

// Parses one statement from the token stream. Index is updated to past the statement.
// Convenience: parse from start of vector.
Statement parse_statement(const std::vector<Token>& tokens);

}  // namespace obsidian
