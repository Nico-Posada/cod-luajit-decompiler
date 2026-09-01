#include "main.h"
#include <argparse/argparse.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/termcolor.hpp>
#include <iostream>
#include <utility>
#ifdef assert
#undef assert
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

struct Error {
    const std::string message;
    const std::string filePath;
    const std::string function;
    const std::string source;
    const std::string line;
};

static struct {
    bool forceOverwrite = false;
    bool ignoreDebugInfo = false;
    bool minimizeDiffs = false;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::string extensionFilter;
} arguments;

template <typename... Args>
static void print_above(indicators::ProgressBar& progressBar, std::format_string<Args...> format, Args&&... args) {
    indicators::erase_line();
    std::cout << termcolor::reset << std::flush;
    std::println(format, std::forward<Args>(args)...);
    std::fflush(stdout);
    progressBar.print_progress();
}

template <typename... Args>
static void print_above(
    indicators::ProgressBar& progressBar, std::FILE* stream, std::format_string<Args...> format, Args&&... args
) {
    indicators::erase_line();
    std::cout << termcolor::reset << std::flush;
    std::println(stream, format, std::forward<Args>(args)...);
    std::fflush(stream);
    progressBar.print_progress();
}

static std::filesystem::path get_executable_directory(const char* executableArgument) {
#ifdef __linux__
    std::error_code error;
    const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !executablePath.empty())
        return executablePath.lexically_normal().parent_path();
#elif defined(_WIN32)
    std::wstring buffer(256, L'\0');

    while (buffer.size() <= 32768) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length)
            break;
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer).lexically_normal().parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#endif

    return std::filesystem::absolute(executableArgument).lexically_normal().parent_path();
}

static std::string string_to_lowercase(const std::string& string) {
    std::string lowercaseString = string;

    for (std::size_t i = lowercaseString.size(); i--;) {
        if (lowercaseString[i] < 'A' || lowercaseString[i] > 'Z')
            continue;
        lowercaseString[i] += 'a' - 'A';
    }

    return lowercaseString;
}

static std::vector<std::filesystem::path>
find_input_files(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath) {
    std::vector<std::filesystem::path> inputFiles;

    if (std::filesystem::is_regular_file(inputPath)) {
        inputFiles.emplace_back(inputPath);
        return inputFiles;
    }

    for (std::filesystem::recursive_directory_iterator entry(inputPath), end; entry != end; ++entry) {
        const std::filesystem::path entryPath = std::filesystem::absolute(entry->path()).lexically_normal();

        if (entry->is_directory() && entryPath == outputPath) {
            entry.disable_recursion_pending();
            continue;
        }

        if (entry->is_regular_file() &&
            (arguments.extensionFilter.empty() ||
             arguments.extensionFilter == string_to_lowercase(entryPath.extension().string()))) {
            inputFiles.emplace_back(entryPath);
        }
    }

    return inputFiles;
}

static bool decompile_file(
    const std::filesystem::path& inputFile,
    const std::filesystem::path& outputFile,
    const HashResolver& hashResolver,
    indicators::ProgressBar& progressBar
) {
    std::filesystem::create_directories(outputFile.parent_path());
    Bytecode bytecode(inputFile.string());
    Ast ast(bytecode, hashResolver, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
    Lua lua(
        bytecode,
        ast,
        outputFile.string(),
        arguments.forceOverwrite,
        arguments.minimizeDiffs
    );

    try {
        bytecode();
        ast();
        lua();
        return true;
    } catch (const Error& error) {
        print_above(
            progressBar,
            stderr,
            "Error running {}\nSource: {}:{}\n\nFile: {}\n\n{}",
            error.function,
            error.source,
            error.line,
            error.filePath,
            error.message
        );
        return false;
    }
}

int main(int argc, char* argv[]) try {
    argparse::ArgumentParser program("cod-luajit-decompiler", "", argparse::default_arguments::help);
    program.add_description("Decompile Call of Duty LuaJIT bytecode into readable Lua source.");
    program.add_hidden_alias_for(program.at("-h"), "-?");

    program.add_argument("INPUT_PATH")
        .help("File or directory containing LuaJIT bytecode")
        .store_into(arguments.inputPath);
    program.add_argument("-o", "--output")
        .metavar("OUTPUT_PATH")
        .help("Override default output directory")
        .store_into(arguments.outputPath);
    program.add_argument("-e", "--extension")
        .metavar("EXTENSION")
        .help("Only decompile files with the specified extension")
        .default_value(std::string{".lua"})
        .store_into(arguments.extensionFilter);
    program.add_argument("-f", "--force_overwrite")
        .help("Always overwrite existing files")
        .flag()
        .store_into(arguments.forceOverwrite);
    program.add_argument("-i", "--ignore_debug_info")
        .help("Ignore bytecode debug info")
        .flag()
        .store_into(arguments.ignoreDebugInfo);
    program.add_argument("-m", "--minimize_diffs")
        .help("Optimize output formatting to help minimize diffs")
        .flag()
        .store_into(arguments.minimizeDiffs);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& error) {
        std::println(stderr, "{}\n\n{}", error.what(), program.help().str());
        return EXIT_FAILURE;
    }

    if (!arguments.extensionFilter.empty()) {
        if (arguments.extensionFilter.front() != '.')
            arguments.extensionFilter.insert(arguments.extensionFilter.begin(), '.');
        arguments.extensionFilter = string_to_lowercase(arguments.extensionFilter);
    }

    const std::filesystem::path inputPath = std::filesystem::absolute(arguments.inputPath).lexically_normal();

    if (!std::filesystem::exists(inputPath)) {
        std::println(stderr, "Failed to open input path: {}", inputPath.string());
        return EXIT_FAILURE;
    }

    const bool inputIsDirectory = std::filesystem::is_directory(inputPath);

    if (!inputIsDirectory && !std::filesystem::is_regular_file(inputPath)) {
        std::println(stderr, "Input path is not a file or directory: {}", inputPath.string());
        return EXIT_FAILURE;
    }

    const std::filesystem::path outputPath =
        std::filesystem::absolute(
            arguments.outputPath.empty() ? (inputIsDirectory ? inputPath : inputPath.parent_path()) / "output"
                                         : arguments.outputPath
        )
            .lexically_normal();

    if (std::filesystem::exists(outputPath) && !std::filesystem::is_directory(outputPath)) {
        std::println(stderr, "Output path is not a folder: {}", outputPath.string());
        return EXIT_FAILURE;
    }

    std::filesystem::create_directories(outputPath);
    const std::vector<std::filesystem::path> inputFiles = find_input_files(inputPath, outputPath);

    if (inputFiles.empty()) {
        std::println(
            stderr,
            "No files {}found in path: {}",
            arguments.extensionFilter.empty() ? "" : "with extension " + arguments.extensionFilter + " ",
            inputPath.string()
        );
        return EXIT_FAILURE;
    }

    const HashResolver hashResolver(get_executable_directory(argv[0]) / "PackageIndex");

    std::size_t filesSkipped = 0;

    {
        constexpr std::size_t PROGRESS_BAR_NON_BAR_WIDTH = 41;
        const std::size_t terminalWidth = indicators::terminal_width();
        const std::size_t barWidth =
            terminalWidth > PROGRESS_BAR_NON_BAR_WIDTH ? terminalWidth - PROGRESS_BAR_NON_BAR_WIDTH : 1;
        indicators::ProgressBar progressBar{
            indicators::option::BarWidth{barWidth},
            indicators::option::Start{"["},
            indicators::option::Fill{"="},
            indicators::option::Lead{">"},
            indicators::option::Remainder{" "},
            indicators::option::End{" ]"},
            indicators::option::ShowPercentage{true},
            indicators::option::ShowElapsedTime{true},
            indicators::option::ShowRemainingTime{true},
            indicators::option::PrefixText{"Decompiling "},
            indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}},
            indicators::option::MaxProgress{inputFiles.size()}};

        try {
            progressBar.set_progress(0);
            for (std::size_t i = 0; i < inputFiles.size(); i++) {
                const std::filesystem::path& inputFile = inputFiles[i];
                std::filesystem::path outputFile;

                if (inputIsDirectory) {
                    outputFile = inputFile.lexically_relative(inputPath);
                } else {
                    outputFile = inputFile.filename();
                }

                outputFile.replace_extension(".lua");
                if (!decompile_file(inputFile, outputPath / outputFile, hashResolver, progressBar))
                    filesSkipped++;
                progressBar.set_progress(i + 1);
            }
        } catch (...) {
            progressBar.mark_as_completed();
            throw;
        }
    }

    std::print("\n");
    if (filesSkipped)
        std::println("Failed to decompile {} file{}.", filesSkipped, filesSkipped > 1 ? "s" : "");
    std::println("Done! Decompiled {}/{} Files.", inputFiles.size() - filesSkipped, inputFiles.size());
    return filesSkipped ? EXIT_FAILURE : EXIT_SUCCESS;
} catch (const std::filesystem::filesystem_error& error) {
    std::println(stderr, "Filesystem error: {}", error.what());
    return EXIT_FAILURE;
}

void assert(
    const bool& assertion,
    const std::string& message,
    const std::string& filePath,
    const std::string& function,
    const std::string& source,
    const uint32_t& line
) {
    if (!assertion)
        throw Error{
            .message = message,
            .filePath = filePath,
            .function = function,
            .source = source,
            .line = std::to_string(line)};
}

std::string byte_to_string(const uint8_t& byte) {
    return std::format("0x{:02X}", static_cast<unsigned>(byte));
}
