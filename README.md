## CoD LuaJIT Decompiler

A fork of [marsinator358](https://github.com/marsinator358)'s [luajit-decompiler-v2](https://github.com/marsinator358/luajit-decompiler-v2) project geared specifically towards decompiling Call of Duty (CoD) lua bytecode.

The original project did have a branch dedicated to adding support for CoD (and this fork has it as a base), but the project still felt like a catch-all for luajit and was missing some specializations that could clean up the decomps.

## Simple Usage

If you're on Windows, simply download the executable from the Releases tab and drag-and-drop your bytecode files onto the executable to begin the decompilation process.

## CLI Usage

Use `-h` or `-?` to show the help menu

```bash
 > dist/bin/cod-luajit-decompiler -h
Usage: cod-luajit-decompiler [--help] [--output OUTPUT_PATH] [--extension EXTENSION] [--bit-length BIT_LENGTH] [--force_overwrite] [--ignore_debug_info] [--minimize_diffs] INPUT_PATH

Decompile Call of Duty LuaJIT bytecode into readable Lua source.

Positional arguments:
  INPUT_PATH                File or directory containing LuaJIT bytecode

Optional arguments:
  -h, --help                shows help message and exits
  -o, --output OUTPUT_PATH  Override default output directory
  -e, --extension           Only decompile files with the specified extension [nargs=0..1] [default: ".lua"]
  -b, --bit-length          Set package-index hash width in bits (0-64) [nargs=0..1] [default: 64]
  -f, --force_overwrite     Always overwrite existing files
  -i, --ignore_debug_info   Ignore bytecode debug info
  -m, --minimize_diffs      Optimize output formatting to help minimize diffs
```

For a directory input, output defaults to `<input-directory>/output` and keeps
the input-relative directory tree. For a file input, output defaults to an
`output` directory beside the file. Relative `--output` paths are resolved from
the current working directory, and missing output directories are created.

To resolve hashes, place WNI v1 (`.wni`) package-index files in a
`PackageIndex` directory beside the executable (see projects like [GreyhoundPackageIndex](https://github.com/Scobalula/GreyhoundPackageIndex) for example wnis). Subdirectories are scanned
recursively. `--bit-length` has no effect when this directory is absent.

If `PackageIndex/.bit_length` exists, its decimal value overrides
`--bit-length`. It must be between `0` and `64`. (This behavior is custom
to this project, but will not affect any other projects that use the
wni format)

Example:
```bash
 > cat PackageIndex/.bit_length
───────┬─────────────────────────────────────────────────────────────────────────
       │ File: PackageIndex/.bit_length
───────┼─────────────────────────────────────────────────────────────────────────
   1   │ 64
───────┴─────────────────────────────────────────────────────────────────────────
```

Batch runs continue after individual file failures and finish with a summary.
The command exits unsuccessfully if any file fails.

## Build Requirements

- Clang with a C++23 standard library that supports `<print>` (libstdc++ 14, libc++, or the MSVC STL)
- CMake 3.20 or newer

## Build and Install

#### Windows

```console
cmake -S . -B build -T ClangCL
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix dist
```

The executable is installed to `dist/bin/cod-luajit-decompiler.exe`.

#### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
cmake --install build --prefix dist
```

The executable is installed to `dist/bin/cod-luajit-decompiler`.

#### MacOS

Mac is unsupported right now. Feel free to modify this project to include support for MacOS and open a PR.

Feel free to [report any issues](https://github.com/Nico-Posada/cod-luajit-decompiler/issues/new) you have.

---

This project uses an boolean expression decompilation algorithm that is based on this paper:  
[www.cse.iitd.ac.in/~sak/reports/isec2016-paper.pdf](https://www.cse.iitd.ac.in/~sak/reports/isec2016-paper.pdf)
