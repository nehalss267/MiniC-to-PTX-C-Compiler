cmake_minimum_required(VERSION 3.16)
project(minic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(MINIC_SOURCES
  src/lexer.cpp
  src/parser.cpp
  src/symtab.cpp
  src/sema.cpp
  src/irbuilder.cpp
  src/opt.cpp
  src/ptxgen.cpp
  src/astdump.cpp
  src/irdump.cpp
  src/main.cpp
)

add_executable(minic ${MINIC_SOURCES})
target_include_directories(minic PRIVATE src)

if(MSVC)
  target_compile_options(minic PRIVATE /W4)
else()
  target_compile_options(minic PRIVATE -Wall -Wextra)
endif()
