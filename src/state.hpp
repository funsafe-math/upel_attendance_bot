#pragma once
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

struct Credentials {
  std::string email    = "";
  std::string password = "";
  Credentials(const fs::path &filename_path) {
    // Format is "email:password"
    std::ifstream file(filename_path);
    std::getline(file, email, ':');
    std::getline(file, password);
    file.close();
    verify_credentials();
  }
  Credentials(const std::string &_email, const std::string &_password) : email{_email}, password{_password} {
    verify_credentials();
  }
  Credentials() = default;

  void verify_credentials();
  void verify_password(std::function<void(const std::string &)> report_function);
  void verify_email(std::function<void(const std::string &)> report_function);
};

class State {
public:
  Credentials creds;
  State(const Credentials &creds);
};
