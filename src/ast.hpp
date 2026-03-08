#pragma once

#include <string>
#include <vector>
#include <variant>
#include <cstdint>

namespace obsidian {

enum class ColumnType { Int, Float, String };

// Value of a literal in INSERT (matches INT, FLOAT, STRING).
using InsertValue = std::variant<std::int64_t, double, std::string>;

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

// SELECT col1, col2, ... FROM name;  or  SELECT * FROM name;
struct SelectStmt {
  bool select_all = false;   // true for SELECT *
  std::vector<std::string> columns;
  std::string table_name;
};

using Statement = std::variant<CreateTableStmt, InsertStmt, SelectStmt>;

// A program is a sequence of statements (for future use; parser can return one or many).
using Program = std::vector<Statement>;

}  // namespace obsidian
