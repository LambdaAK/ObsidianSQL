#pragma once

#include <string>
#include <cstdint>

namespace obsidian {

enum class TokenType {
  // Literals
  IntLiteral,
  FloatLiteral,
  StringLiteral,

  // Identifier (table/column names)
  Identifier,

  // Keywords
  Select,
  From,
  Where,
  Create,
  Table,
  Insert,
  Into,
  Values,
  Int,
  Float,
  String,
  And,
  Or,
  Not,

  // Comparison operators
  Eq,   // =
  Ne,   // <>
  Lt,   // <
  Le,   // <=
  Gt,   // >
  Ge,   // >=

  // Punctuation
  Comma,
  Semicolon,
  LParen,
  RParen,
  Star,

  // Special
  EndOfInput,
};

struct Token {
  TokenType kind;
  std::string text;
  uint32_t line = 0;
  uint32_t column = 0;

  bool is(TokenType k) const { return kind == k; }
};

}  // namespace obsidian
