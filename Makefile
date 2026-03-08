.PHONY: build run

build:
	@mkdir -p build && cd build && cmake .. && cmake --build .

run: build
	./build/obsidian_sql_main
