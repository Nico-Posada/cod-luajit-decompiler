cmake_minimum_required(VERSION 3.20)
project(cod_luajit_decompiler LANGUAGES C CXX)
find_package(Threads REQUIRED)

add_executable(cod-luajit-decompiler
    main.cpp
    hash/hash_resolver.cpp
    ast/ast.cpp
    bytecode/bytecode.cpp
    bytecode/prototype.cpp
    lua/lua.cpp
    third_party/lz4/lz4.c
)

target_compile_features(cod-luajit-decompiler PRIVATE c_std_99 cxx_std_23)
target_include_directories(cod-luajit-decompiler PRIVATE third_party/lz4)
# p-ranav/argparse v3.2 (https://github.com/p-ranav/argparse, commit 3eda91b2e1ce7d569f84ba295507c4cd8fd96910)
target_include_directories(cod-luajit-decompiler SYSTEM PRIVATE third_party/argparse/include)
# p-ranav/indicators v2.3 (https://github.com/p-ranav/indicators, commit 26d39ad8fb438f5f50fb16427f554045d8431030)
target_include_directories(cod-luajit-decompiler SYSTEM PRIVATE third_party/indicators/include)
target_link_libraries(cod-luajit-decompiler PRIVATE Threads::Threads)
set_target_properties(cod-luajit-decompiler PROPERTIES C_EXTENSIONS OFF CXX_EXTENSIONS OFF)

if(MSVC)
    target_link_options(cod-luajit-decompiler PRIVATE /STACK:268435456)
endif()

include(GNUInstallDirs)
install(TARGETS cod-luajit-decompiler RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
