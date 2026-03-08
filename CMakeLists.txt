cmake_minimum_required(VERSION 3.16)
project(ObsidianSQL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obsidian_sql
  src/lexer.cpp
  src/parser.cpp
)
target_include_directories(obsidian_sql PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(obsidian_sql_main main.cpp)
target_link_libraries(obsidian_sql_main PRIVATE obsidian_sql)
