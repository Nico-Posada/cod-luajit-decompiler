cmake_minimum_required(VERSION 3.20)
project(luajit_decompiler_v2 LANGUAGES C CXX)

add_executable(luajit-decompiler-v2
    main.cpp
    hash/hash_resolver.cpp
    ast/ast.cpp
    bytecode/bytecode.cpp
    bytecode/prototype.cpp
    lua/lua.cpp
    third_party/lz4/lz4.c
)

target_compile_features(luajit-decompiler-v2 PRIVATE c_std_99 cxx_std_23)
target_include_directories(luajit-decompiler-v2 PRIVATE third_party/lz4)
set_target_properties(luajit-decompiler-v2 PROPERTIES C_EXTENSIONS OFF CXX_EXTENSIONS OFF)

if(MSVC)
    target_link_options(luajit-decompiler-v2 PRIVATE /STACK:268435456)
endif()

include(GNUInstallDirs)
install(TARGETS luajit-decompiler-v2 RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
