#include "parser.hpp"
#include <sstream>
#include <cstdlib>
#include <memory>

namespace obsidian {

namespace {

const Token& at(const std::vector<Token>& tokens, std::size_t i) {
  if (i >= tokens.size()) return tokens.back();
  return tokens[i];
}

TokenType kind(const std::vector<Token>& tokens, std::size_t i) {
  return at(tokens, i).kind;
}

const char* token_type_name(TokenType k) {
  switch (k) {
    case TokenType::Create: return "CREATE";
    case TokenType::Table: return "TABLE";
    case TokenType::Insert: return "INSERT";
    case TokenType::Into: return "INTO";
    case TokenType::Select: return "SELECT";
    case TokenType::From: return "FROM";
    case TokenType::Values: return "VALUES";
    case TokenType::Int: return "INT";
    case TokenType::Float: return "FLOAT";
    case TokenType::String: return "STRING";
    case TokenType::Identifier: return "identifier";
    case TokenType::IntLiteral: return "integer";
    case TokenType::FloatLiteral: return "float";
    case TokenType::StringLiteral: return "string";
    case TokenType::Semicolon: return "';'";
    case TokenType::LParen: return "'('";
    case TokenType::RParen: return "')'";
    case TokenType::Comma: return "','";
    case TokenType::Star: return "'*'";
    case TokenType::Where: return "WHERE";
    case TokenType::And: return "AND";
    case TokenType::Or: return "OR";
    case TokenType::Not: return "NOT";
    case TokenType::Eq: return "'='";
    case TokenType::Ne: return "'<>'";
    case TokenType::Lt: return "'<'";
    case TokenType::Le: return "'<='";
    case TokenType::Gt: return "'>'";
    case TokenType::Ge: return "'>='";
    case TokenType::EndOfInput: return "end of input";
    default: return "?";
  }
}

void expect(const std::vector<Token>& tokens, std::size_t& index, TokenType k) {
  if (index >= tokens.size() || tokens[index].kind != k) {
    std::ostringstream msg;
    msg << "Expected " << token_type_name(k);
    if (index < tokens.size())
      msg << " but got " << token_type_name(tokens[index].kind) << " (\"" << tokens[index].text << "\")";
    else
      msg << " but got end of input";
    msg << " at " << (index < tokens.size() ? tokens[index].line : 0) << ":"
        << (index < tokens.size() ? tokens[index].column : 0);
    throw std::runtime_error(msg.str());
  }
}

std::string consume_identifier(const std::vector<Token>& tokens, std::size_t& index) {
  expect(tokens, index, TokenType::Identifier);
  std::string id = tokens[index].text;
  ++index;
  return id;
}

ExprPtr parse_or_expr(const std::vector<Token>& tokens, std::size_t& index);

ColumnType parse_column_type(const std::vector<Token>& tokens, std::size_t& index) {
  if (index >= tokens.size()) throw std::runtime_error("Expected type (INT, FLOAT, STRING)");
  TokenType k = tokens[index].kind;
  ++index;
  if (k == TokenType::Int) return ColumnType::Int;
  if (k == TokenType::Float) return ColumnType::Float;
  if (k == TokenType::String) return ColumnType::String;
  throw std::runtime_error("Expected INT, FLOAT, or STRING at " + std::to_string(tokens[index - 1].line) + ":" + std::to_string(tokens[index - 1].column));
}

InsertValue parse_value(const std::vector<Token>& tokens, std::size_t& index) {
  if (index >= tokens.size()) throw std::runtime_error("Expected value (int, float, or string)");
  const Token& t = tokens[index];
  ++index;
  switch (t.kind) {
    case TokenType::IntLiteral:
      return static_cast<std::int64_t>(std::strtoll(t.text.c_str(), nullptr, 10));
    case TokenType::FloatLiteral:
      return std::strtod(t.text.c_str(), nullptr);
    case TokenType::StringLiteral:
      return t.text;
    default:
      throw std::runtime_error("Expected int, float, or string literal at " + std::to_string(t.line) + ":" + std::to_string(t.column));
  }
}

ExprPtr parse_primary(const std::vector<Token>& tokens, std::size_t& index) {
  if (index >= tokens.size()) throw std::runtime_error("Expected expression");
  TokenType k = kind(tokens, index);
  if (k == TokenType::LParen) {
    ++index;
    ExprPtr e = parse_or_expr(tokens, index);
    expect(tokens, index, TokenType::RParen); ++index;
    return e;
  }
  if (k == TokenType::Identifier) {
    std::string col = consume_identifier(tokens, index);
    return std::make_unique<Expr>(ColumnRefExpr{std::move(col)});
  }
  if (k == TokenType::IntLiteral || k == TokenType::FloatLiteral || k == TokenType::StringLiteral) {
    InsertValue v = parse_value(tokens, index);
    return std::make_unique<Expr>(LiteralExpr{std::move(v)});
  }
  throw std::runtime_error("Expected column, literal, or (expression) at " + std::to_string(tokens[index].line) + ":" + std::to_string(tokens[index].column));
}

ExprPtr parse_comparison(const std::vector<Token>& tokens, std::size_t& index) {
  ExprPtr left = parse_primary(tokens, index);
  if (index >= tokens.size()) return left;
  TokenType k = kind(tokens, index);
  Op op;
  if (k == TokenType::Eq) op = Op::Eq;
  else if (k == TokenType::Ne) op = Op::Ne;
  else if (k == TokenType::Lt) op = Op::Lt;
  else if (k == TokenType::Le) op = Op::Le;
  else if (k == TokenType::Gt) op = Op::Gt;
  else if (k == TokenType::Ge) op = Op::Ge;
  else return left;
  ++index;
  ExprPtr right = parse_primary(tokens, index);
  return std::make_unique<Expr>(BinaryExpr{op, std::move(left), std::move(right)});
}

ExprPtr parse_not_expr(const std::vector<Token>& tokens, std::size_t& index) {
  if (index < tokens.size() && kind(tokens, index) == TokenType::Not) {
    ++index;
    ExprPtr operand = parse_not_expr(tokens, index);
    return std::make_unique<Expr>(UnaryExpr{Op::Not, std::move(operand)});
  }
  return parse_comparison(tokens, index);
}

ExprPtr parse_and_expr(const std::vector<Token>& tokens, std::size_t& index) {
  ExprPtr left = parse_not_expr(tokens, index);
  while (index < tokens.size() && kind(tokens, index) == TokenType::And) {
    ++index;
    ExprPtr right = parse_not_expr(tokens, index);
    left = std::make_unique<Expr>(BinaryExpr{Op::And, std::move(left), std::move(right)});
  }
  return left;
}

ExprPtr parse_or_expr(const std::vector<Token>& tokens, std::size_t& index) {
  ExprPtr left = parse_and_expr(tokens, index);
  while (index < tokens.size() && kind(tokens, index) == TokenType::Or) {
    ++index;
    ExprPtr right = parse_and_expr(tokens, index);
    left = std::make_unique<Expr>(BinaryExpr{Op::Or, std::move(left), std::move(right)});
  }
  return left;
}

CreateTableStmt parse_create_table(const std::vector<Token>& tokens, std::size_t& index) {
  expect(tokens, index, TokenType::Create); ++index;
  expect(tokens, index, TokenType::Table); ++index;
  std::string table_name = consume_identifier(tokens, index);
  expect(tokens, index, TokenType::LParen); ++index;

  CreateTableStmt stmt;
  stmt.table_name = std::move(table_name);

  if (kind(tokens, index) == TokenType::RParen)
    throw std::runtime_error("CREATE TABLE must have at least one column");

  for (;;) {
    std::string col_name = consume_identifier(tokens, index);
    ColumnType col_type = parse_column_type(tokens, index);
    stmt.columns.emplace_back(std::move(col_name), col_type);

    if (kind(tokens, index) == TokenType::RParen) break;
    expect(tokens, index, TokenType::Comma); ++index;
  }
  expect(tokens, index, TokenType::RParen); ++index;
  expect(tokens, index, TokenType::Semicolon); ++index;
  return stmt;
}

InsertStmt parse_insert(const std::vector<Token>& tokens, std::size_t& index) {
  expect(tokens, index, TokenType::Insert); ++index;
  expect(tokens, index, TokenType::Into); ++index;
  std::string table_name = consume_identifier(tokens, index);
  expect(tokens, index, TokenType::Values); ++index;
  expect(tokens, index, TokenType::LParen); ++index;

  InsertStmt stmt;
  stmt.table_name = std::move(table_name);

  if (kind(tokens, index) == TokenType::RParen)
    throw std::runtime_error("INSERT VALUES must have at least one value");

  for (;;) {
    stmt.values.push_back(parse_value(tokens, index));
    if (kind(tokens, index) == TokenType::RParen) break;
    expect(tokens, index, TokenType::Comma); ++index;
  }
  expect(tokens, index, TokenType::RParen); ++index;
  expect(tokens, index, TokenType::Semicolon); ++index;
  return stmt;
}

SelectStmt parse_select(const std::vector<Token>& tokens, std::size_t& index) {
  expect(tokens, index, TokenType::Select); ++index;

  SelectStmt stmt;
  if (kind(tokens, index) == TokenType::Star) {
    stmt.select_all = true;
    ++index;
  } else {
    if (kind(tokens, index) != TokenType::Identifier)
      throw std::runtime_error("Expected * or column list after SELECT");
    for (;;) {
      stmt.columns.push_back(consume_identifier(tokens, index));
      if (kind(tokens, index) != TokenType::Comma) break;
      ++index;
    }
  }

  expect(tokens, index, TokenType::From); ++index;
  stmt.table_name = consume_identifier(tokens, index);
  if (index < tokens.size() && kind(tokens, index) == TokenType::Where) {
    ++index;
    stmt.where_clause = parse_or_expr(tokens, index);
  }
  expect(tokens, index, TokenType::Semicolon); ++index;
  return stmt;
}

}  // namespace

Statement parse_statement(const std::vector<Token>& tokens, std::size_t& index) {
  if (index >= tokens.size() || kind(tokens, index) == TokenType::EndOfInput)
    throw std::runtime_error("Unexpected end of input");

  TokenType k = kind(tokens, index);
  if (k == TokenType::Create) return parse_create_table(tokens, index);
  if (k == TokenType::Insert) return parse_insert(tokens, index);
  if (k == TokenType::Select) return parse_select(tokens, index);

  throw std::runtime_error("Expected CREATE, INSERT, or SELECT at " +
    std::to_string(tokens[index].line) + ":" + std::to_string(tokens[index].column));
}

Statement parse_statement(const std::vector<Token>& tokens) {
  std::size_t index = 0;
  return parse_statement(tokens, index);
}

}  // namespace obsidian
