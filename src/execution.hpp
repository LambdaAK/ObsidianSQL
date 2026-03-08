#pragma once

#include "ast.hpp"
#include "catalog.hpp"
#include <stdexcept>

namespace obsidian {

// Execute one statement. Modifies catalog for CREATE/INSERT; prints result for SELECT.
// Throws std::runtime_error on schema/execution errors (table exists, not found, wrong value count).
void execute(const Statement& stmt, Catalog& catalog);

}  // namespace obsidian
