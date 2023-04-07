#include "state.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <functional>
#include <stdexcept>

void Credentials::verify_credentials() {
  auto yellow_alert = [](auto msg) { fmt::print(fg(fmt::color::yellow), msg); };
  verify_email(yellow_alert);
  verify_password(yellow_alert);
}

void Credentials::verify_password(std::function<void(const std::string &)> report_function) {
  auto min_characters = 8u;
  if (password.size() < min_characters)
    report_function(fmt::format("Password is shorter than {} characters", min_characters));
}
void Credentials::verify_email(std::function<void(const std::string &)> report_function) {
  if (email.find('@') == std::string::npos)
    report_function("Alert: email does not contain a '@' character\n");
}

State::State(const Credentials &_creds) : creds{_creds} {
}
