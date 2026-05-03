#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;



static bool read_file_binary(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}



int main(int argc, char** argv) {
    const fs::path out_dir = (argc > 1) ? fs::path(argv[1]) : fs::path("out");

    // Cases of path errors
    if (!fs::exists(out_dir)) {
        std::cerr << "[checker] directory does not exist: " << out_dir << '\n';
        return 2;
    }
    if (!fs::is_directory(out_dir)) {
        std::cerr << "[checker] path is not a directory: " << out_dir << '\n';
        return 2;
    }

    // Get the files inside the directory
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        std::cerr << "[checker] no files found in: " << out_dir << '\n';
        return 2;
    }

    // Save the content of the first file
    std::string reference_content;
    if (!read_file_binary(files.front(), reference_content)) {
        std::cerr << "[checker] failed to read reference file: " << files.front() << '\n';
        return 2;
    }

    // Check contents
    bool all_equal = true;
    for (std::size_t i = 1; i < files.size(); ++i) {
        std::string current_content;
        if (!read_file_binary(files[i], current_content)) {
            std::cerr << "[checker] failed to read file: " << files[i] << '\n';
            return 2;
        }

        if (current_content != reference_content) {
            all_equal = false;
            std::cerr << "[checker] mismatch: " << files[i].filename().string() << " differs from " << files.front().filename().string() << '\n';
        }
    }

    // Result
    if (!all_equal) {
        std::cout << "[checker] NO --> output files are NOT identical\n";
    } else {
        std::cout << "[checker] OK --> all " << files.size() << " output files have the same content\n";
    }
    return 0;
}
