#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

std::vector<std::string> split(const std::string &string,
                               const std::string &delim);
std::vector<std::string> parseLine(const std::string &line);
bool fileExistsInDir(const std::string &dirPath, const std::string &fileName);
bool run(const std::string &path, std::vector<std::string> &cmd,
         bool background = false);

const std::string EXIT = "exit";

int main(int argc, char *argv[]) {
    std::vector<std::string> args(argc);
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }

    auto PATH = getenv("PATH");
    auto &&paths = split(PATH, ":");

    int returned = 0;

    std::string line;

    while (true) {
        std::cout << fs::current_path() << std::endl;
        std::getline(std::cin, line);
        std::vector<std::string> parsedLine = parseLine(line);

        std::string fileName = parsedLine[0];

        if (fileName == EXIT || fileName == "") {
            break;
        }
        for (auto path : paths) {
            // if (!fs::exists(path) || !fs::is_directory(path)) {
            //     continue;
            // }
            try {
                if (fileExistsInDir(path, fileName)) {
                    bool bg = parsedLine[parsedLine.size() - 1] == "&";
                    if (bg) {
                        parsedLine.pop_back();
                    }

                    run(path + "/" + fileName, parsedLine, !bg);
                    break;
                }
            } catch (const fs::filesystem_error &e) {
                continue;
            }
        }
    }

    return returned;
}

std::vector<std::string> split(const std::string &string,
                               const std::string &delim) {
    std::string substr = string;
    std::vector<std::string> strings;

    int next_delim;
    while ((next_delim = substr.find(delim)) > 0) {
        std::cout << substr.length() << std::endl;
        strings.push_back(substr.substr(0, next_delim));
        substr = substr.substr(next_delim + delim.length(), substr.length());
    }

    return strings;
}

std::vector<std::string> parseLine(const std::string &line) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    char quote_type = '\0';
    bool escape = false;

    int size = line.size();

    for (int i = 0; i < size; i++) {
        char c = line[i];

        if (escape) {
            current += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (quoted) {
            if (c == quote_type) {
                quoted = false;
            } else {
                current += c;
            }
        } else if (c == '"' || c == '\'') {
            quoted = true;
            quote_type = c;
        } else if (isspace(c)) {
            tokens.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    tokens.push_back(current);

    return tokens;
}

bool fileExistsInDir(const std::string &dirPath, const std::string &fileName) {
    for (const auto &entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().filename() == fileName) {
            return true;
        }
    }
    return false;
}

bool run(const std::string &path, std::vector<std::string> &cmd,
         bool background) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "error forking" << std::endl;
    } else if (pid == 0) {
        // exec
        std::vector<char *> c_args;

        for (const auto &arg : cmd) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr); // execv requires null-terminated array

        execv(path.c_str(), c_args.data());
        exit(1);
    } else if (!background) {
        // wait
        std::cerr << "foreground wait unimplemented" << std::endl;
    }
    return 0;
}
