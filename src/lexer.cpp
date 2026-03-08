#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace obsidian {

namespace {

bool is_ident_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_rest(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

TokenType keyword_from_text(const std::string& text) {
  if (text == "SELECT") return TokenType::Select;
  if (text == "FROM") return TokenType::From;
  if (text == "WHERE") return TokenType::Where;
  if (text == "CREATE") return TokenType::Create;
  if (text == "TABLE") return TokenType::Table;
  if (text == "INSERT") return TokenType::Insert;
  if (text == "INTO") return TokenType::Into;
  if (text == "VALUES") return TokenType::Values;
  if (text == "INT") return TokenType::Int;
  if (text == "FLOAT") return TokenType::Float;
  if (text == "STRING") return TokenType::String;
  if (text == "AND") return TokenType::And;
  if (text == "OR") return TokenType::Or;
  if (text == "NOT") return TokenType::Not;
  return TokenType::Identifier;
}

std::string to_upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

std::vector<Token> lex(const std::string& source) {
  std::vector<Token> tokens;
  const char* p = source.data();
  const char* end = source.data() + source.size();
  uint32_t line = 1;
  uint32_t column = 1;

  auto start_line = line;
  auto start_col = column;
  auto start = [&]() { start_line = line; start_col = column; };
  auto emit = [&](TokenType kind, const std::string& text) {
    tokens.push_back(Token{kind, text, start_line, start_col});
  };

  while (p < end) {
    start();
    char cur = *p;

    if (cur == ' ' || cur == '\t') {
      ++p; ++column;
    } else if (cur == '\n') {
      ++p; ++line; column = 1;
    } else if (cur == '\r') {
      ++p; if (p < end && *p == '\n') ++p; ++line; column = 1;
    } else if (cur == ',') { emit(TokenType::Comma, ","); ++p; ++column; }
    else if (cur == ';') { emit(TokenType::Semicolon, ";"); ++p; ++column; }
    else if (cur == '(') { emit(TokenType::LParen, "("); ++p; ++column; }
    else if (cur == ')') { emit(TokenType::RParen, ")"); ++p; ++column; }
    else if (cur == '*') { emit(TokenType::Star, "*"); ++p; ++column; }
    else if (cur == '=') { emit(TokenType::Eq, "="); ++p; ++column; }
    else if (cur == '<') {
      if (p + 1 < end && p[1] == '=') { emit(TokenType::Le, "<="); p += 2; column += 2; }
      else if (p + 1 < end && p[1] == '>') { emit(TokenType::Ne, "<>"); p += 2; column += 2; }
      else { emit(TokenType::Lt, "<"); ++p; ++column; }
    } else if (cur == '>') {
      if (p + 1 < end && p[1] == '=') { emit(TokenType::Ge, ">="); p += 2; column += 2; }
      else { emit(TokenType::Gt, ">"); ++p; ++column; }
    } else if (cur == '\"') {
      ++p; ++column;
      std::string value;
      while (p < end && *p != '\x22') {
        if (*p == '\\') {
          ++p; ++column;
          if (p >= end) throw std::runtime_error("Unterminated escape in string");
          if (*p == '\x22') value += '\x22';
          else if (*p == '\\') value += '\\';
          else if (*p == 'n') value += '\n';
          else if (*p == 't') value += '\t';
          else throw std::runtime_error("Invalid escape in string");
          ++p; ++column;
        } else {
          value += *p++;
          if (!value.empty() && value.back() == '\n') { ++line; column = 1; } else { ++column; }
        }
      }
      if (p >= end) throw std::runtime_error("Unterminated string");
      ++p; ++column;
      emit(TokenType::StringLiteral, value);
    } else if (std::isdigit(static_cast<unsigned char>(cur)) || (cur == '-' && p + 1 < end && std::isdigit(static_cast<unsigned char>(p[1])))) {
      bool neg = (cur == '-');
      if (neg) { ++p; ++column; }
      if (p >= end || !std::isdigit(static_cast<unsigned char>(*p))) throw std::runtime_error("Invalid number");
      std::string num; if (neg) num += '-';
      while (p < end && std::isdigit(static_cast<unsigned char>(*p))) { num += *p++; ++column; }
      if (p < end && *p == '.') {
        num += *p++; ++column;
        while (p < end && std::isdigit(static_cast<unsigned char>(*p))) { num += *p++; ++column; }
        emit(TokenType::FloatLiteral, num);
      } else {
        emit(TokenType::IntLiteral, num);
      }
    } else if (is_ident_start(cur)) {
      std::string id;
      while (p < end && is_ident_rest(*p)) { id += *p++; ++column; }
      TokenType k = keyword_from_text(to_upper(id));
      emit(k, id);
    } else {
      throw std::runtime_error("Unexpected character at " + std::to_string(line) + ":" + std::to_string(column));
    }
  }

  start();
  emit(TokenType::EndOfInput, "");
  return tokens;
}

}  // namespace obsidian
