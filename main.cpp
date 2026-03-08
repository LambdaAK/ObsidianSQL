#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "execution.hpp"
#include "catalog.hpp"
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

static std::string trim_right(const std::string& s) {
  std::size_t i = s.size();
  while (i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '\r' || s[i - 1] == '\n'))
    --i;
  return s.substr(0, i);
}

void run_repl(obsidian::Catalog& catalog) {
  const char* prompt = "obsidian> ";
  std::string buffer;
  for (;;) {
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {
      std::cout << "\n";
      break;  // EOF (Ctrl-D)
    }
    buffer += line;
    buffer += '\n';

    std::string trimmed = trim_right(buffer);
    if (trimmed.empty()) {
      buffer.clear();
      continue;
    }
    if (trimmed == "exit" || trimmed == ".exit" || trimmed == ".quit") {
      break;
    }
    if (trimmed.back() != ';') {
      continue;  // keep reading for multi-line statement
    }

    try {
      auto tokens = obsidian::lex(buffer);
      std::size_t index = 0;
      int executed = 0;
      while (index < tokens.size() && tokens[index].kind != obsidian::TokenType::EndOfInput) {
        obsidian::Statement stmt = obsidian::parse_statement(tokens, index);
        obsidian::execute(stmt, catalog);
        ++executed;
      }
      if (executed == 0 && !tokens.empty() && tokens[0].kind != obsidian::TokenType::EndOfInput) {
        std::cerr << "Error: no complete statement (missing ';'?)\n";
      }
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << '\n';
    }
    buffer.clear();
  }
}

void run_batch(const std::string& input, bool tree_view) {
  auto tokens = obsidian::lex(input);
  if (tokens.empty() || (tokens.size() == 1 && tokens[0].kind == obsidian::TokenType::EndOfInput)) {
    std::cout << "No statements.\n";
    return;
  }
  obsidian::Catalog catalog;
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
      obsidian::execute(stmt, catalog);
    }
    ++stmt_num;
  }
  if (!tree_view && stmt_num > 0) {
    std::cout << "(" << stmt_num << " statement(s) executed)\n";
  }
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
                << "  With no file: start console (REPL). Type SQL, end with ; to run.\n"
                << "  With file: run all statements in the file.\n"
                << "  --tree  (with file) print AST tree instead of executing.\n"
                << "  In console: type 'exit' or .exit to quit, Ctrl-D to exit.\n";
      return 0;
    } else {
      path = arg;
    }
  }

  try {
    if (path.empty()) {
      std::cout << "ObsidianSQL console. Type SQL statements; end with ; to execute. 'exit' to quit.\n";
      obsidian::Catalog catalog;
      run_repl(catalog);
    } else {
      std::string input = read_file(path);
      run_batch(input, tree_view);
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
