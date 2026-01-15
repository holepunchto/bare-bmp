cmake_minimum_required(VERSION 3.31)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)

project(bare_bmp C)

add_bare_module(bare_bmp)

target_sources(
  ${bare_bmp}
  PRIVATE
    binding.c
)
