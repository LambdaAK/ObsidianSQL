#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <optional>
#include <cstdint>

namespace obsidian {

enum class ColumnType { Int, Float, String };

// Value of a literal in INSERT (matches INT, FLOAT, STRING).
using InsertValue = std::variant<std::int64_t, double, std::string>;

// Expression types for WHERE clause.
enum class Op { Eq, Ne, Lt, Le, Gt, Ge, And, Or, Not };

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr {
  InsertValue value;
};

struct ColumnRefExpr {
  std::string column_name;
};

struct BinaryExpr {
  Op op;
  ExprPtr left;
  ExprPtr right;
};

struct UnaryExpr {
  Op op;  // Only Not for now
  ExprPtr operand;
};

struct Expr {
  std::variant<LiteralExpr, ColumnRefExpr, BinaryExpr, UnaryExpr> expr;
  template <typename T>
  explicit Expr(T&& e) : expr(std::forward<T>(e)) {}
};

// CREATE TABLE name ( col TYPE, ... );
struct CreateTableStmt {
  std::string table_name;
  std::vector<std::pair<std::string, ColumnType>> columns;
};

// INSERT INTO name VALUES ( v1, v2, ... );
struct InsertStmt {
  std::string table_name;
  std::vector<InsertValue> values;
};

// SELECT col1, col2, ... FROM name [WHERE expr];  or  SELECT * FROM name [WHERE expr];
struct SelectStmt {
  bool select_all = false;   // true for SELECT *
  std::vector<std::string> columns;
  std::string table_name;
  std::optional<ExprPtr> where_clause;
};

using Statement = std::variant<CreateTableStmt, InsertStmt, SelectStmt>;

// A program is a sequence of statements (for future use; parser can return one or many).
using Program = std::vector<Statement>;

}  // namespace obsidian
