#include <cctype>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

std::vector<std::string> split(const std::string &string,
                               const std::string &delim);
std::vector<std::string> parseLine(const std::string &line);
bool fileExistsInDir(const std::string &dirPath, const std::string &fileName);
bool myexec(const std::string &path, std::vector<std::string> &cmd,
            bool background, std::istream *in, std::ostream *out);

// Basically looks for the file in path
std::string which(const std::string &fileName);
inline bool cd(const std::vector<std::string> &parsedLine);

void run(std::vector<std::string> &command, bool bg);

const std::string EXIT = "exit";

int main(int argc, char *argv[]) {
    std::vector<std::string> args(argc);
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }

    std::string line;

    while (true) {
        std::cout << fs::current_path() << std::endl;
        std::getline(std::cin, line);
        std::vector<std::string> parsedLine = parseLine(line);

        std::string fileName = parsedLine[0];

        if (fileName == EXIT || fileName == "") {
            break;
        }
        bool bg = parsedLine[parsedLine.size() - 1] == "&";
        if (bg) {
            parsedLine.pop_back();
        }

        if (fileName == "cd") {
            cd(parsedLine);
            continue;
        }
        run(parsedLine, bg);
    }

    return 0;
}

std::vector<std::string> split(const std::string &string,
                               const std::string &delim) {
    std::string substr = string;
    std::vector<std::string> strings;

    int next_delim;
    while ((next_delim = substr.find(delim)) > 0) {
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
    bool dollar = false;
    bool open_bracket = false;

    int size = line.size();

    for (int i = 0; i < size; i++) {
        char c = line[i];

        if (dollar) {
            if (open_bracket == true) {
                if (c == '}') {
                    const char *str = getenv(current.c_str());
                    if (str != nullptr) {
                        tokens.push_back(str);
                    }
                    current.clear();
                    dollar = false;
                    continue;
                }
            } else if (c == '{' && escape) {
                open_bracket = true;
                escape = false;
                continue;
            } else if (isspace(c)) {
                const char *str = getenv(current.c_str());
                if (str != nullptr) {
                    tokens.push_back(str);
                }
                dollar = false;
                current.clear();
                continue;
            }
            current += c;
            escape = false;
        } else if (escape) {
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
            if (current.length() > 0) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (c == '$') {
            dollar = true;
            escape = true;
        } else {
            current += c;
        }
    }

    if (dollar) {
        const char *tmp = getenv(current.c_str());
        if (tmp == nullptr) {
            current = "";
        } else {
            current = tmp;
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

void replaceIn(const std::string &in) {
    int fd;
    if (close(STDIN_FILENO) < 0) {
        throw std::runtime_error("Error close()");
    }
    if ((fd = open(in.c_str(), O_RDONLY, S_IWUSR | S_IRUSR)) < 0) {
        throw std::runtime_error("Error open()");
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
        throw std::runtime_error("Error dup2()");
    }
}

void replaceOut(const std::string &in) {
    int fd;
    if (close(STDIN_FILENO) < 0) {
        throw std::runtime_error("Error close()");
    }
    if ((fd = open(in.c_str(), O_WRONLY, S_IWUSR | S_IRUSR)) < 0) {
        throw std::runtime_error("Error open()");
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        throw std::runtime_error("Error dup2()");
    }
}

bool myexec(const std::string &path, std::vector<std::string> &cmd,
            bool background, const std::string &in, const std::string &out) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "error forking" << std::endl;
    } else if (pid == 0) {
        // exec
        if (in != "") {
            cmd.pop_back();
            cmd.pop_back();
            replaceIn(in);
        }
        if (out != "") {
            cmd.pop_back();
            cmd.pop_back();
            replaceOut(in);
        }

        std::vector<char *> c_args;

        for (const auto &arg : cmd) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr); // execv requires null-terminated array

        execv(path.c_str(), c_args.data());
        std::cout << path << " doesn't exist" << std::endl;
        exit(1);
    } else if (!background) {
        // wait
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
        } else {
            std::cout << "[" << WEXITSTATUS(status) << "]";
        }
    }
    return 0;
}

std::string which(const std::string &fileName) {
    std::string PATH = getenv("PATH");
    std::vector<std::string> &&paths = split(PATH, ":");

    for (auto path : paths) {
        try {
            if (fileExistsInDir(path, fileName)) {
                return path + "/" + fileName;
            }
        } catch (const fs::filesystem_error &e) {
        }
    }

    return "";
}

inline bool cd(const std::vector<std::string> &parsedLine) {
    try {
        setenv("OLDPWD", fs::current_path().c_str(), 1);
        fs::current_path(parsedLine[1]);
    } catch (std::exception e) {
        std::cout << e.what() << std::endl;
        return false;
    }
    return true;
}

std::string getInput(std::vector<std::string> &command) {
    int i;
    bool flag = false;
    for (; i < command.size(); i++) {
        if (command[i] == "<") {
            flag = true;
            break;
        }
    }
    i++;
    if (flag) {
        if (i >= command.size()) {
            throw std::runtime_error(
                "syntax incorrect, no filename found after <");
        }
        return command[i];
    }
    return "";
}

std::string getOutput(std::vector<std::string> &command) {
    int i;
    bool flag = false;
    for (; i < command.size(); i++) {
        if (command[i] == ">") {
            flag = true;
            break;
        }
    }
    i++;
    if (flag) {
        if (i >= command.size()) {
            throw std::runtime_error(
                "syntax incorrect, no filename found after >");
        }
        return command[i];
    }
    return "";
}

void run(std::vector<std::string> &command, bool bg) {
    std::string fileName = command[0];
    std::string &&input = getInput(command);
    std::string &&output = getOutput(command);
    std::cout << "input=" << input << std::endl
              << "output=" << output << std::endl;

    if (fileName[0] != '/') {
        fileName = which(fileName);
    }
    try {
        myexec(fileName, command, bg, input, output);
    } catch (std::runtime_error e) {
        std::cout << e.what() << std::endl;
    }
}
