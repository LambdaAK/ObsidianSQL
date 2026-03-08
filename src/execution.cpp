#include "execution.hpp"
#include "ast.hpp"
#include "colors.hpp"
#include <iostream>
#include <variant>
#include <unordered_map>
#include <algorithm>

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

InsertValue eval_value(const Expr& e, const Row& row,
                      const std::unordered_map<std::string, std::size_t>& col_to_idx) {
  return std::visit(
    [&](const auto& x) -> InsertValue {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, LiteralExpr>) return x.value;
      else if constexpr (std::is_same_v<T, ColumnRefExpr>) {
        auto it = col_to_idx.find(x.column_name);
        if (it == col_to_idx.end())
          throw std::runtime_error("Column not found in WHERE: " + x.column_name);
        return row[it->second];
      }
      else throw std::runtime_error("Invalid expression in WHERE (expected column or literal)");
    },
    e.expr
  );
}

bool eval_bool(const Expr& e, const Row& row,
               const std::unordered_map<std::string, std::size_t>& col_to_idx) {
  return std::visit(
    [&](const auto& x) -> bool {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, BinaryExpr>) {
        if (x.op == Op::And) return eval_bool(*x.left, row, col_to_idx) && eval_bool(*x.right, row, col_to_idx);
        if (x.op == Op::Or) return eval_bool(*x.left, row, col_to_idx) || eval_bool(*x.right, row, col_to_idx);
        InsertValue a = eval_value(*x.left, row, col_to_idx);
        InsertValue b = eval_value(*x.right, row, col_to_idx);
        auto cmp = [&]() {
          if (std::holds_alternative<std::int64_t>(a) && std::holds_alternative<std::int64_t>(b)) {
            int64_t va = std::get<std::int64_t>(a), vb = std::get<std::int64_t>(b);
            if (va < vb) return -1; if (va > vb) return 1; return 0;
          }
          if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
            double va = std::get<double>(a), vb = std::get<double>(b);
            if (va < vb) return -1; if (va > vb) return 1; return 0;
          }
          if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
            int c = std::get<std::string>(a).compare(std::get<std::string>(b));
            return c < 0 ? -1 : (c > 0 ? 1 : 0);
          }
          throw std::runtime_error("Type mismatch in WHERE comparison");
        };
        int c = cmp();
        switch (x.op) {
          case Op::Eq: return c == 0;
          case Op::Ne: return c != 0;
          case Op::Lt: return c < 0;
          case Op::Le: return c <= 0;
          case Op::Gt: return c > 0;
          case Op::Ge: return c >= 0;
          default: return false;
        }
      }
      else if constexpr (std::is_same_v<T, UnaryExpr>) {
        if (x.op == Op::Not) return !eval_bool(*x.operand, row, col_to_idx);
        return false;
      }
      else return false;
    },
    e.expr
  );
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

  // Build column name -> index for WHERE and ORDER BY
  std::unordered_map<std::string, std::size_t> col_to_idx;
  for (std::size_t i = 0; i < table.columns.size(); ++i)
    col_to_idx[table.columns[i].first] = i;

  // Collect rows (filter by WHERE if present)
  std::vector<Row> rows;
  for (const Row& row : table.rows) {
    if (stmt.where_clause && !eval_bool(*stmt.where_clause.value(), row, col_to_idx))
      continue;
    rows.push_back(row);
  }

  // Sort by ORDER BY if present
  if (!stmt.order_by.empty()) {
    auto compare = [&](const InsertValue& a, const InsertValue& b) -> int {
      if (std::holds_alternative<std::int64_t>(a) && std::holds_alternative<std::int64_t>(b)) {
        int64_t va = std::get<std::int64_t>(a), vb = std::get<std::int64_t>(b);
        if (va < vb) return -1; if (va > vb) return 1; return 0;
      }
      if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
        double va = std::get<double>(a), vb = std::get<double>(b);
        if (va < vb) return -1; if (va > vb) return 1; return 0;
      }
      if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
        int c = std::get<std::string>(a).compare(std::get<std::string>(b));
        return c < 0 ? -1 : (c > 0 ? 1 : 0);
      }
      throw std::runtime_error("Type mismatch in ORDER BY comparison");
    };
    std::sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) {
      for (const auto& [col_name, asc] : stmt.order_by) {
        auto it = col_to_idx.find(col_name);
        if (it == col_to_idx.end())
          throw std::runtime_error("Column not found in ORDER BY: " + col_name);
        std::size_t idx = it->second;
        int c = compare(a[idx], b[idx]);
        if (c != 0) return asc ? c < 0 : c > 0;
      }
      return false;
    });
  }

  // Print rows
  for (const Row& row : rows) {
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
