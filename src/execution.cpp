#include "execution.hpp"
#include "colors.hpp"
#include <iostream>
#include <variant>

namespace obsidian {

namespace {

bool value_matches_type(const InsertValue& v, ColumnType t) {
  switch (t) {
    case ColumnType::Int: return std::holds_alternative<std::int64_t>(v);
    case ColumnType::Float: return std::holds_alternative<double>(v);
    case ColumnType::String: return std::holds_alternative<std::string>(v);
  }
  return false;
}

void execute_create_table(const CreateTableStmt& stmt, Catalog& catalog) {
  if (catalog.count(stmt.table_name))
    throw std::runtime_error("Table already exists: " + stmt.table_name);
  Table table;
  table.name = stmt.table_name;
  table.columns = stmt.columns;
  catalog[stmt.table_name] = std::move(table);
  std::cout << color::green << "Created table " << stmt.table_name << "." << color::reset << "\n" << std::flush;
}

void execute_insert(const InsertStmt& stmt, Catalog& catalog) {
  auto it = catalog.find(stmt.table_name);
  if (it == catalog.end())
    throw std::runtime_error("Table not found: " + stmt.table_name);
  Table& table = it->second;
  if (stmt.values.size() != table.columns.size())
    throw std::runtime_error("INSERT has " + std::to_string(stmt.values.size()) +
                             " values but table has " + std::to_string(table.columns.size()) + " columns");
  for (std::size_t i = 0; i < stmt.values.size(); ++i) {
    if (!value_matches_type(stmt.values[i], table.columns[i].second))
      throw std::runtime_error("Type mismatch for column " + table.columns[i].first);
  }
  table.rows.push_back(stmt.values);
  std::cout << color::green << "Inserted 1 row(s)." << color::reset << "\n" << std::flush;
}

void print_value(const InsertValue& v) {
  std::visit(
    [](const auto& x) {
      using V = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<V, std::int64_t>) std::cout << x;
      else if constexpr (std::is_same_v<V, double>) std::cout << x;
      else std::cout << '"' << x << '"';
    },
    v
  );
}

void execute_select(const SelectStmt& stmt, Catalog& catalog) {
  auto it = catalog.find(stmt.table_name);
  if (it == catalog.end())
    throw std::runtime_error("Table not found: " + stmt.table_name);
  const Table& table = it->second;

  std::vector<std::size_t> col_indices;
  if (stmt.select_all) {
    for (std::size_t i = 0; i < table.columns.size(); ++i)
      col_indices.push_back(i);
  } else {
    for (const std::string& col_name : stmt.columns) {
      std::size_t i = 0;
      for (; i < table.columns.size(); ++i) {
        if (table.columns[i].first == col_name) break;
      }
      if (i >= table.columns.size())
        throw std::runtime_error("Column not found: " + col_name);
      col_indices.push_back(i);
    }
  }

  // Header
  std::cout << color::bold << color::cyan;
  for (std::size_t i = 0; i < col_indices.size(); ++i) {
    if (i) std::cout << " | ";
    std::cout << table.columns[col_indices[i]].first;
  }
  std::cout << color::reset << "\n" << color::dim;
  for (std::size_t i = 0; i < col_indices.size(); ++i) {
    if (i) std::cout << "-+-";
    std::cout << "---";
  }
  std::cout << color::reset << "\n";

  // Rows
  for (const Row& row : table.rows) {
    for (std::size_t i = 0; i < col_indices.size(); ++i) {
      if (i) std::cout << " | ";
      print_value(row[col_indices[i]]);
    }
    std::cout << "\n";
  }
}

}  // namespace

void execute(const Statement& stmt, Catalog& catalog) {
  std::visit(
    [&catalog](const auto& s) {
      using T = std::decay_t<decltype(s)>;
      if constexpr (std::is_same_v<T, CreateTableStmt>)
        execute_create_table(s, catalog);
      else if constexpr (std::is_same_v<T, InsertStmt>)
        execute_insert(s, catalog);
      else if constexpr (std::is_same_v<T, SelectStmt>)
        execute_select(s, catalog);
    },
    stmt
  );
}

}  // namespace obsidian
