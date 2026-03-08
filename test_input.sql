CREATE TABLE users (id INT, name STRING);
INSERT INTO users VALUES (1, "alice");
INSERT INTO users VALUES (2, "bob");
SELECT * FROM users;
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = "alice";
