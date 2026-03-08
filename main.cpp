#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

namespace {

const char* column_type_name(obsidian::ColumnType t) {
  switch (t) {
    case obsidian::ColumnType::Int: return "INT";
    case obsidian::ColumnType::Float: return "FLOAT";
    case obsidian::ColumnType::String: return "STRING";
  }
  return "?";
}

void print_ast_oneline(const obsidian::Statement& stmt) {
  std::visit(
    [](const auto& s) {
      using T = std::decay_t<decltype(s)>;
      if constexpr (std::is_same_v<T, obsidian::CreateTableStmt>) {
        std::cout << "CREATE TABLE " << s.table_name << " (";
        for (std::size_t i = 0; i < s.columns.size(); ++i) {
          if (i) std::cout << ", ";
          std::cout << s.columns[i].first << " " << column_type_name(s.columns[i].second);
        }
        std::cout << ");\n";
      } else if constexpr (std::is_same_v<T, obsidian::InsertStmt>) {
        std::cout << "INSERT INTO " << s.table_name << " VALUES (";
        for (std::size_t i = 0; i < s.values.size(); ++i) {
          if (i) std::cout << ", ";
          std::visit(
            [](const auto& v) {
              using V = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<V, std::int64_t>) std::cout << v;
              else if constexpr (std::is_same_v<V, double>) std::cout << v;
              else std::cout << '"' << v << '"';
            },
            s.values[i]
          );
        }
        std::cout << ");\n";
      } else if constexpr (std::is_same_v<T, obsidian::SelectStmt>) {
        std::cout << "SELECT ";
        if (s.select_all) std::cout << "*";
        else {
          for (std::size_t i = 0; i < s.columns.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << s.columns[i];
          }
        }
        std::cout << " FROM " << s.table_name << ";\n";
      }
    },
    stmt
  );
}

void print_ast_tree(const obsidian::Statement& stmt, const std::string& indent = "") {
  const std::string sub = indent + "  ";
  std::visit(
    [&](const auto& s) {
      using T = std::decay_t<decltype(s)>;
      if constexpr (std::is_same_v<T, obsidian::CreateTableStmt>) {
        std::cout << indent << "CreateTableStmt\n";
        std::cout << sub << "table_name: \"" << s.table_name << "\"\n";
        std::cout << sub << "columns:\n";
        for (const auto& col : s.columns) {
          std::cout << sub << "  - " << col.first << " : " << column_type_name(col.second) << "\n";
        }
      } else if constexpr (std::is_same_v<T, obsidian::InsertStmt>) {
        std::cout << indent << "InsertStmt\n";
        std::cout << sub << "table_name: \"" << s.table_name << "\"\n";
        std::cout << sub << "values:\n";
        for (const auto& v : s.values) {
          std::visit(
            [&](const auto& x) {
              using V = std::decay_t<decltype(x)>;
              if constexpr (std::is_same_v<V, std::int64_t>) std::cout << sub << "  - " << x << " (int)\n";
              else if constexpr (std::is_same_v<V, double>) std::cout << sub << "  - " << x << " (float)\n";
              else std::cout << sub << "  - \"" << x << "\" (string)\n";
            },
            v
          );
        }
      } else if constexpr (std::is_same_v<T, obsidian::SelectStmt>) {
        std::cout << indent << "SelectStmt\n";
        std::cout << sub << "table_name: \"" << s.table_name << "\"\n";
        std::cout << sub << "select_all: " << (s.select_all ? "true" : "false") << "\n";
        if (!s.select_all) {
          std::cout << sub << "columns:\n";
          for (const auto& c : s.columns) std::cout << sub << "  - \"" << c << "\"\n";
        }
      }
    },
    stmt
  );
}

std::string read_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) {
    std::ostringstream msg;
    msg << "Cannot open file: " << path;
    throw std::runtime_error(msg.str());
  }
  std::stringstream buf;
  buf << f.rdbuf();
  return buf.str();
}

std::string read_stdin() {
  std::string input;
  std::string line;
  while (std::getline(std::cin, line)) {
    input += line;
    input += '\n';
  }
  return input;
}

}  // namespace

int main(int argc, char* argv[]) {
  bool tree_view = false;
  std::string path;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--tree" || arg == "-t") {
      tree_view = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << (argv[0] ? argv[0] : "obsidian_sql_main") << " [--tree|-t] [file.sql]\n"
                << "  If file.sql is given, parse that file; otherwise read from stdin.\n"
                << "  --tree  Print AST as an indented tree instead of one-line summary.\n";
      return 0;
    } else {
      path = arg;
    }
  }

  std::string input;
  if (path.empty()) {
    std::cout << "ObsidianSQL (lex + parse). Enter statements, then Ctrl-D to run.\n";
    input = read_stdin();
  } else {
    input = read_file(path);
  }
  if (input.empty()) {
    std::cout << "No input.\n";
    return 0;
  }

  try {
    auto tokens = obsidian::lex(input);
    if (tokens.empty() || (tokens.size() == 1 && tokens[0].kind == obsidian::TokenType::EndOfInput)) {
      std::cout << "No statements (only whitespace or empty).\n";
      return 0;
    }
    std::size_t index = 0;
    int stmt_num = 0;
    while (index < tokens.size() && tokens[index].kind != obsidian::TokenType::EndOfInput) {
      obsidian::Statement stmt = obsidian::parse_statement(tokens, index);
      if (tree_view) {
        if (stmt_num) std::cout << "\n";
        std::cout << "--- Statement " << (stmt_num + 1) << " ---\n";
        print_ast_tree(stmt);
        std::cout << "\n";
      } else {
        std::cout << "Parsed: ";
        print_ast_oneline(stmt);
      }
      ++stmt_num;
    }
    if (!tree_view && stmt_num > 0) {
      std::cout << "(" << stmt_num << " statement(s) parsed successfully)\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
