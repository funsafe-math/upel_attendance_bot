#pragma once
#include <iostream>
#include <vector>
#include <fmt/core.h>
#include <fmt/color.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

struct Credentials
{
    std::string email = "";
    std::string password = "";
    Credentials(const fs::path &filename_path)
    {
        // Format is "email:password"
        std::ifstream file(filename_path);
        std::getline(file, email, ':');
        std::getline(file, password);
        file.close();
    }
    Credentials(const std::string &_email, const std::string &_password)
        : email{_email}
        , password{_password}
    {}
    Credentials() = default;
};

class State {
    public:
        Credentials creds;
        State(int argc, const char *argv[])
        {
        if (argc < 2) {
            fmt::print(stderr, "Usage: {} <credentials filepath>\n", argv[0]);
            fmt::print("Email: ");
            std::string email;
            std::cin >> email;
            if (email.find('@') == std::string::npos)
                fmt::print(fg(fmt::color::yellow), "Alert: email does not contain a '@' character\n");
            fmt::print("");
//            fmt::print("Password: ");
            std::string password{getpass("Password: ")};
            creds = Credentials{email, password};
        } else {
            const fs::path credentials_path{argv[1]};
            creds = Credentials{credentials_path};
        }
        }
};
