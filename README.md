This branch supports Call of Duty bytecode versions `0x82`, `0x83` and `0x84`.

## CoD LuaJIT Decompiler

*CoD LuaJIT Decompiler* is a replacement tool for the old and now mostly defunct python decompiler.  
The project fixes all of the bugs and quirks the python decompiler had while also offering  
full support for gotos and stripped bytecode including locals and upvalues.

## Requirements

- Clang with a C++23 standard library that supports `<print>` (libstdc++ 14, libc++, or the MSVC STL)
- CMake 3.20 or newer

## Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix "$PWD/dist"
```

On Windows, configure with `-T ClangCL` instead of `-DCMAKE_CXX_COMPILER=clang++`.

The installed executable is `dist/bin/cod-luajit-decompiler` on Linux and
`dist/bin/cod-luajit-decompiler.exe` on Windows.

## Usage

```text
cod-luajit-decompiler INPUT_PATH [options]
```

For a directory input, output defaults to `<input-directory>/output` and retains
the input-relative directory tree. For a file input, output defaults to
`<input-file-parent>/output`. A relative `-o`/`--output` path is resolved from
the current working directory, and missing output directories are created.

Errors are written to standard error. Batch runs continue after per-file
failures, always print a final summary, and exit unsuccessfully if any file
fails. Existing output files are left untouched unless `-f`/`--force_overwrite`
is supplied. Windows Explorer drag-and-drop remains available because the
dropped path is passed as the first command-line argument.

Feel free to [report any issues](https://github.com/Nico-Posada/cod-luajit-decompiler/issues/new) you have.

## TODO

* bytecode big endian support
* improved decompilation logic for conditional assignments

---

This project uses an boolean expression decompilation algorithm that is based on this paper:  
[www.cse.iitd.ac.in/~sak/reports/isec2016-paper.pdf](https://www.cse.iitd.ac.in/~sak/reports/isec2016-paper.pdf)
