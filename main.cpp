#include "main.h"
#include <argparse/argparse.hpp>

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

static bool isProgressBarActive = false;

static struct {
    bool forceOverwrite = false;
    bool ignoreDebugInfo = false;
    bool minimizeDiffs = false;
    bool unrestrictedAscii = false;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::string extensionFilter;
} arguments;

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
    const HashResolver& hashResolver
) {
    std::filesystem::create_directories(outputFile.parent_path());
    Bytecode bytecode(inputFile.string());
    Ast ast(bytecode, hashResolver, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
    Lua lua(
        bytecode,
        ast,
        outputFile.string(),
        arguments.forceOverwrite,
        arguments.minimizeDiffs,
        arguments.unrestrictedAscii
    );

    try {
        print("--------------------\nInput file: " + bytecode.filePath + "\nReading bytecode...");
        bytecode();
        print("Building ast...");
        ast();
        print("Writing lua source...");
        lua();
        print("Output file: " + lua.filePath);
        return true;
    } catch (const Error& error) {
        erase_progress_bar();
        std::println(
            stderr,
            "\nError running {}\nSource: {}:{}\n\nFile: {}\n\n{}",
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
    program.add_argument("-u", "--unrestricted_ascii")
        .help("Disable default UTF-8 encoding and string restrictions")
        .flag()
        .store_into(arguments.unrestrictedAscii);

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

    for (const std::filesystem::path& inputFile : inputFiles) {
        std::filesystem::path outputFile;

        if (inputIsDirectory) {
            outputFile = inputFile.lexically_relative(inputPath);
        } else {
            outputFile = inputFile.filename();
        }

        outputFile.replace_extension(".lua");
        if (!decompile_file(inputFile, outputPath / outputFile, hashResolver))
            filesSkipped++;
    }

    print(
        "--------------------\n" +
        (filesSkipped
             ? "Failed to decompile " + std::to_string(filesSkipped) + " file" + (filesSkipped > 1 ? "s" : "") + ".\n"
             : "") +
        "Done!"
    );
    return filesSkipped ? EXIT_FAILURE : EXIT_SUCCESS;
} catch (const std::filesystem::filesystem_error& error) {
    std::println(stderr, "Filesystem error: {}", error.what());
    return EXIT_FAILURE;
}

void print(const std::string& message) {
    std::println("{}", message);
    std::fflush(stdout);
}

void print_progress_bar(const double& progress, const double& total) {
    static char PROGRESS_BAR[] = "\r[====================]";
    const uint8_t threshold = std::round(20 / total * progress);

    for (uint8_t i = 20; i--;) {
        PROGRESS_BAR[i + 2] = i < threshold ? '=' : ' ';
    }

    std::print("{}", PROGRESS_BAR);
    std::fflush(stdout);
    isProgressBarActive = true;
}

void erase_progress_bar() {
    static constexpr char PROGRESS_BAR_ERASER[] = "\r                      \r";

    if (!isProgressBarActive)
        return;
    std::print("{}", PROGRESS_BAR_ERASER);
    std::fflush(stdout);
    isProgressBarActive = false;
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
