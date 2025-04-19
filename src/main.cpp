#include <iostream>
#include <ostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argc);
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }

    std::string line;

    while(true) {
        std::cin >> line;

        const std::string EXIT = "exit";
        if (line == EXIT || line == "") {
            break;
        }
        std::cout << line << std::endl;
    }

    return 0;
}
