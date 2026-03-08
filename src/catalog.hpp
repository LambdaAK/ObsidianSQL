#pragma once

#include "ast.hpp"
#include <unordered_map>
#include <vector>

namespace obsidian {

// One row: one value per column in schema order.
using Row = std::vector<InsertValue>;

struct Table {
  std::string name;
  std::vector<std::pair<std::string, ColumnType>> columns;
  std::vector<Row> rows;
};

// Table name -> table.
using Catalog = std::unordered_map<std::string, Table>;

}  // namespace obsidian
