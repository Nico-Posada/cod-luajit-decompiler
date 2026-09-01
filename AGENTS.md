# Project summary

COD LuaJIT Decompiler converts Call of Duty LuaJIT bytecode into readable Lua source. It is a C++23 command-line program built around the existing `Bytecode -> Ast -> Lua` pipeline and supports the Call of Duty bytecode versions `0x82`, `0x83`, and `0x84`.

# Project goals

- Prioritize Call of Duty bytecode formats and real game fixtures over generic LuaJIT compatibility.
- Preserve game-specific behavior such as typed hashes and BO6 `HGGET`/`HGSET` instructions.
- Produce deterministic output on Linux and Windows from one portable CMake target.
- Keep the parser, AST reconstruction, and Lua writer direct and maintainable; prefer the C++ standard library over platform wrappers or new dependencies.
- Validate format or decompiler changes against the available Call of Duty corpus, including repeat runs that exclude generated output from traversal.

# Scope guidance

Do not broaden behavior for stock LuaJIT or unrelated games unless it is also required by a supported Call of Duty format. Preserve serialized field widths, bytecode-version distinctions, hash formatting, and existing generated-Lua conventions unless a real Call of Duty fixture proves a change is needed.

Build with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build --config Release --parallel
```

When starting changes, don't create a new branch unless it's a large enough to warrant a new branch. When committing changes, use conventional commit messages.

If you ever make changes to the CLI arg parsing - whether that be a new flag/description change - make sure to update the readme with the correct -h output.
