cmake_minimum_required(VERSION 3.18)

project(EDA093-lab1 LANGUAGES C)

add_executable(lsh parse.c lsh.c)
# termcap replaced with ncurses
target_link_libraries(lsh PRIVATE readline ncurses)
target_compile_options(lsh PRIVATE "-ggdb3" "-O0" "-Wall" "-Wextra")
