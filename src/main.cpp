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

struct Command {
    std::string out = "";
    std::string in = "";
    std::string bin = "";
    std::vector<std::string> args;
    bool bg = false;
    bool append = false;

    void to_string() {
        std::cout << "out=" << out <<
                    "\nin=" << in <<
                    "\nbin=" << bin <<
                    "\narg1=" << args[1] <<
                    "\nbg=" << bg <<
                    "\nappend=" << append << std::endl;

    }
};

Command parse(const std::vector<std::string>& line);
std::vector<std::string> split(const std::string &string,
                               const std::string &delim);
std::vector<std::string> tokenize(const std::string &line);
bool fileExistsInDir(const std::string &dirPath, const std::string &fileName);
bool myexec(Command &cmd);

// Basically looks for the file in path
std::string which(const std::string &fileName);
inline bool cd(const std::vector<std::string> &parsedLine);

void run(Command &cmd);

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
        std::vector<std::string> parsedLine = tokenize(line);
        Command &&cmd = parse(parsedLine);

        cmd.to_string();

        std::string fileName = cmd.bin;

        if (fileName == EXIT) {
            break;
        }

        if (fileName == "cd") {
            cd(cmd.args);
            continue;
        }
        run(cmd);
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

Command parse(const std::vector<std::string>& line) {
    Command cmd;
    cmd.bin = line[0];
    for(int i = 1; i < line.size(); i++) {
        if (line[i] == "<") {
            cmd.in = line[++i];
        } else if (line[i] == ">") {
            cmd.out = line[++i];
            cmd.append = false;
        } else if (line[i] == ">>") {
            cmd.out = line[++i];
            cmd.append = true;
        } else if (line[i] == "&") {
            if (i + 1 < line.size()) {
                throw std::runtime_error("error parsing, & token is not at the end");
            }
            cmd.bg = true;
        } else {
            cmd.args.push_back(line[i]);
        }
    }
    return cmd;
}

std::vector<std::string> tokenize(const std::string &line) {
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

bool myexec(Command &cmd) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "error forking" << std::endl;
    } else if (pid == 0) {
        // exec
        if (cmd.in != "") {
            cmd.args.pop_back();
            cmd.args.pop_back();
            replaceIn(cmd.in);
        }
        if (cmd.out != "") {
            cmd.args.pop_back();
            cmd.args.pop_back();
            replaceOut(cmd.in);
        }

        std::vector<char *> c_args;

        for (const auto &arg : cmd.args) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr); // execv requires null-terminated array

        execv(cmd.bin.c_str(), c_args.data());
        std::cout << cmd.bin << " doesn't exist" << std::endl;
        exit(1);
    } else if (!cmd.bg) {
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

void run(Command &cmd) {
    const std::string &fileName = cmd.bin;
    if (fileName[0] == '/') {
        myexec(cmd);
    }
    std::string &&commandPath = which(fileName);
    myexec(cmd);
}
