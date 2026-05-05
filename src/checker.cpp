#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
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

static std::optional<std::string> dataset_type_from_content(const std::string& content) {
    std::istringstream in(content);
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, separator);
        if (key == "dataset-type" || key == "dataset_type") {
            return line.substr(separator + 1);
        }
    }

    return std::nullopt;
}

struct OutputFile {
    fs::path path;
    std::string content;
};



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

    std::sort(files.begin(), files.end());

    std::map<std::string, std::vector<OutputFile>> files_by_dataset_type;
    for (const auto& file : files) {
        std::string content;
        if (!read_file_binary(file, content)) {
            std::cerr << "[checker] failed to read file: " << file << '\n';
            return 2;
        }

        const auto dataset_type = dataset_type_from_content(content);
        if (!dataset_type.has_value() || dataset_type->empty()) {
            std::cerr << "[checker] missing dataset-type in: " << file.filename().string() << '\n';
            return 2;
        }

        files_by_dataset_type[*dataset_type].push_back(OutputFile{file, std::move(content)});
    }

    bool all_equal = true;

    for (const auto& [dataset_type, dataset_files] : files_by_dataset_type) {
        const auto& reference = dataset_files.front();
        bool dataset_equal = true;

        for (std::size_t i = 1; i < dataset_files.size(); ++i) {
            const auto& current = dataset_files[i];
            if (current.content != reference.content) {
                dataset_equal = false;
                all_equal = false;
                std::cerr << "[checker] mismatch: dataset-type=" << dataset_type << ' '
                          << current.path.filename().string() << " differs from "
                          << reference.path.filename().string() << '\n';
            }
        }

        if (!dataset_equal) {
            std::cout << "[checker] NO --> dataset-type=" << dataset_type
                      << " output files are NOT identical\n";
        } else {
            std::cout << "[checker] OK --> dataset-type=" << dataset_type
                      << " all " << dataset_files.size()
                      << " output files have the same content\n";
        }
    }

    if (!all_equal) {
        std::cout << "[checker] NO --> at least one dataset-type group has non-identical output files\n";
    }
    return 0;
}
