# ObsidianSQL

A small in-memory SQL database in C++. No persistence yet—everything lives in memory for the session.

## Build & Run

```bash
make
make run
```

Or run a script: `./build/obsidian_sql_main script.sql`

## SQL

**CREATE TABLE** – Define tables with INT, FLOAT, or STRING columns.

```sql
CREATE TABLE users (id INT, name STRING);
```

**INSERT** – Values must match column order.

```sql
INSERT INTO users VALUES (1, "alice");
```

**SELECT** – With optional WHERE and ORDER BY.

```sql
SELECT * FROM users;
SELECT id, name FROM users WHERE id > 0;
SELECT * FROM users ORDER BY name DESC;
```

WHERE supports `=`, `<>`, `<`, `<=`, `>`, `>=`, and `AND`, `OR`, `NOT`. ORDER BY takes `ASC` (default) or `DESC`.

## Console Commands

- `.tables` – List tables
- `.schema <name>` – Show a table’s columns and types
- `.help` – List these
- `.exit` or `exit` – Quit
