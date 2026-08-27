#include "main.h"

struct Error {
    const std::string message;
    const std::string filePath;
    const std::string function;
    const std::string source;
    const std::string line;
};

static bool isProgressBarActive = false;

static struct {
    bool showHelp = false;
    bool forceOverwrite = false;
    bool ignoreDebugInfo = false;
    bool minimizeDiffs = false;
    bool unrestrictedAscii = false;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::string extensionFilter;
} arguments;

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

static bool decompile_file(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
    std::filesystem::create_directories(outputFile.parent_path());
    Bytecode bytecode(inputFile.string());
    Ast ast(bytecode, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
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

static const char* parse_arguments(const int argc, char* const argv[]) {
    for (int i = 1; i < argc; i++) {
        const std::string argument = argv[i];

        if (argument == "-h" || argument == "-?" || argument == "--help") {
            arguments.showHelp = true;
        } else if (argument == "-f" || argument == "--force_overwrite") {
            arguments.forceOverwrite = true;
        } else if (argument == "-i" || argument == "--ignore_debug_info") {
            arguments.ignoreDebugInfo = true;
        } else if (argument == "-m" || argument == "--minimize_diffs") {
            arguments.minimizeDiffs = true;
        } else if (argument == "-u" || argument == "--unrestricted_ascii") {
            arguments.unrestrictedAscii = true;
        } else if (argument == "-e" || argument == "--extension") {
            if (++i >= argc)
                return argv[i - 1];
            arguments.extensionFilter = argv[i];
        } else if (argument == "-o" || argument == "--output") {
            if (++i >= argc)
                return argv[i - 1];
            arguments.outputPath = argv[i];
        } else if (!argument.empty() && argument.front() == '-') {
            return argv[i];
        } else if (arguments.inputPath.empty()) {
            arguments.inputPath = argv[i];
        } else {
            return argv[i];
        }
    }

    return nullptr;
}

int main(int argc, char* argv[]) try {
    if (const char* invalid = parse_arguments(argc, argv)) {
        std::println(stderr, "Invalid argument: {}\nUse -? to show usage and options.", invalid);
        return EXIT_FAILURE;
    }

    if (arguments.showHelp) {
        print(
            "Usage: luajit-decompiler-v2 INPUT_PATH [options]\n"
            "\n"
            "Available options:\n"
            "  -h, -?, --help\t\tShow this message\n"
            "  -o, --output OUTPUT_PATH\tOverride default output directory\n"
            "  -e, --extension EXTENSION\tOnly decompile files with the specified extension\n"
            "  -f, --force_overwrite\t\tAlways overwrite existing files\n"
            "  -i, --ignore_debug_info\tIgnore bytecode debug info\n"
            "  -m, --minimize_diffs\t\tOptimize output formatting to help minimize diffs\n"
            "  -u, --unrestricted_ascii\tDisable default UTF-8 encoding and string restrictions"
        );
        return EXIT_SUCCESS;
    }

    if (arguments.inputPath.empty()) {
        std::println(stderr, "No input path specified!");
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

    std::size_t filesSkipped = 0;

    for (const std::filesystem::path& inputFile : inputFiles) {
        std::filesystem::path outputFile;

        if (inputIsDirectory) {
            outputFile = inputFile.lexically_relative(inputPath);
        } else {
            outputFile = inputFile.filename();
        }

        outputFile.replace_extension(".lua");
        if (!decompile_file(inputFile, outputPath / outputFile))
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
