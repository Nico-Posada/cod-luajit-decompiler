cmake_minimum_required(VERSION 3.20)
project(luajit_decompiler_v2 LANGUAGES CXX)

add_executable(luajit-decompiler-v2
    main.cpp
    ast/ast.cpp
    bytecode/bytecode.cpp
    bytecode/prototype.cpp
    lua/lua.cpp
)

target_compile_features(luajit-decompiler-v2 PRIVATE cxx_std_23)
set_target_properties(luajit-decompiler-v2 PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_link_options(luajit-decompiler-v2 PRIVATE /STACK:268435456)
endif()

include(GNUInstallDirs)
install(TARGETS luajit-decompiler-v2 RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
