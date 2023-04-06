#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cpr/cpr.h>
#include <cstdio>
#include <fmt/color.h>
#include <fmt/printf.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <utility>

#include "state.hpp"

using namespace simdjson; // optional


std::string find_str_after(const std::string &start, const std::string &html, char delimiter = '"')
{
    auto start_pattern = html.find(start);
    auto start_str     = start_pattern + start.size();
    auto end_str       = html.find(delimiter, start_str);
    return html.substr(start_str, end_str - start_str);
}
std::string get_sesskey(cpr::Session &session)
{
    session.SetUrl(cpr::Url{"https://upel.agh.edu.pl/my/courses.php"});
    auto courses = session.Get();
    std::cout << "Got url : " << courses.url.str() << '\n';
    //    std::cout << "Text: " << courses.text << '\n';
    //    auto full_username = find_str_after(R"(title="Zobacz profil">)", courses.text, '<');
    //    boost::algorithm::trim(full_username);
    //    fmt::print(fg(fmt::color::yellow), "Upel says your name is : {}\n", full_username);

    auto sesskey = find_str_after(R"(a href="https://upel.agh.edu.pl/login/logout.php?sesskey=)",
                                  courses.text);
    return sesskey;
}

std::pair<cpr::Session, std::string> login(const Credentials &creds)
{
    cpr::Session session;
    session.SetHttpVersion(cpr::HttpVersion(cpr::HttpVersionCode::VERSION_2_0));
    const cpr::Url upel_login_page_url = "https://upel.agh.edu.pl/login/index.php";
    spdlog::info("Visiting {}", upel_login_page_url.c_str());
    session.SetUrl(upel_login_page_url);
    const auto upel_login_page_request = session.Get();
    std::string html = upel_login_page_request.text;

    const auto escape_ampersands = [](std::string &str){
        boost::replace_all(str, "amp;", "");
    };

    const auto upel_saml2_url_begin = "https://upel.agh.edu.pl/auth/saml2/login.php";
    auto start = upel_login_page_request.text.find(upel_saml2_url_begin);
    auto length = upel_login_page_request.text.find('"', start + 1) - start;
    std::string next_url = html.substr(start, length);
    escape_ampersands(next_url);


    
    spdlog::info("Visiting {}", next_url.c_str());
    session.SetUrl(cpr::Url{next_url});
    const auto r2 = session.Get();
    spdlog::info("Got response from: {}", r2.url.c_str());
    //    std::cout << "r2 redirects : " << r2.redirect_count << '\n';
    //    std::cout << "r2 status code : " << r2.status_code << '\n';
    //    std::cout << r2.url.str() << "\n";
    //    std::cout << r2.text << "\n";
    auto url_str = find_str_after(R"(<input type="hidden" name="url" value=")", r2.text);
    auto skin_str = find_str_after(R"(<input type="hidden" name="skin" value=")", r2.text);
    auto token_str = find_str_after(R"(<input id="token" type="hidden" name="token" value=")",
                                    r2.text);
    // std::cout << "URL : " << url_str << '\n';
    // std::cout << "SKIN : " << skin_str << '\n';
    // std::cout << "TOKEN : " << token_str << '\n';

    spdlog::info("Visiting {}", r2.url.c_str());
    session.SetUrl(r2.url);
    session.SetParameters(cpr::Parameters{{"url", url_str},
                                          {"timezone", "2"},
                                          {"skin", skin_str},
                                          {"token", token_str},
                                          {"user", creds.email},
                                          {"password", creds.password}});
    auto p = session.Post();
    // std::cout << "post responded with " << p.status_code << '\n';

    auto relay_state = find_str_after(R"(<input type="hidden" name="RelayState" value=")", p.text);
    auto saml_response = find_str_after(R"(<input type="hidden" name="SAMLResponse" value=")",
                                        p.text);

    escape_ampersands(relay_state);
    escape_ampersands(saml_response);
    session.SetUrl(cpr::Url("https://upel.agh.edu.pl/auth/saml2/sp/saml2-acs.php/upel.agh.edu.pl"));
    cpr::Parameters param = {{"RelayState", relay_state}, {"SAMLResponse", saml_response}};
    session.SetParameters(cpr::Parameters{}); // Reset parameters
    auto content = param.GetContent(*session.GetCurlHolder().get());
    session.SetBody(cpr::Body(content));
    auto p2 = session.Post();
    switch(p2.status_code){
        case 200:
            break;
        case 404:
            fmt::print(fg(fmt::color::red), "Got 404: Invalid email/password");
        default:
            exit(1);
    }
    // std::cout << "Status code : " << p2.status_code << '\n';
    // std::cout << "New url : " << p2.url.str() << '\n';
    // std::cout << "Took (s) : " << p2.elapsed << '\n';
    fmt::print("Login requests took {}s\n", upel_login_page_request.elapsed + r2.elapsed + p.elapsed + p2.elapsed);

    session.SetUrl(p2.url);
    auto courses = session.Get();
    std::cout << "Got url : " << courses.url.str() << '\n';
    auto full_username = find_str_after(R"(title="Zobacz profil">)", courses.text, '<');
    boost::algorithm::trim(full_username);
    fmt::print(fg(fmt::color::yellow), "Upel says your name is : {}\n", full_username);

    auto sesskey = find_str_after(R"(a href="https://upel.agh.edu.pl/login/logout.php?sesskey=)",
                                  courses.text);
    return std::pair<cpr::Session, std::string>(std::move(session), std::move(sesskey));
}

std::map<std::string, int64_t> get_courses(cpr::Session &session,
                                           const std::string &sesskey,
                                           ondemand::parser &parser)
{
    //    auto sesskey = get_sesskey(session);
    auto courses_url = fmt::format("https://upel.agh.edu.pl/lib/ajax/"
                                   "service.php?sesskey={}&info=core_course_get_enrolled_"
                                   "courses_by_timeline_classification",
                                   sesskey);

    session.SetUrl(cpr::Url{courses_url});
    session.SetBody(cpr::Body(
        R"([{"index":0,"methodname":"core_course_get_enrolled_courses_by_timeline_classification","args":{"offset":0,"limit":0,"classification":"all","sort":"fullname","customfieldname":"","customfieldvalue":""}}])"));
    auto r = session.Get();
    //    ondemand::parser parser;
    padded_string padded_text{r.text};
    auto doc = parser.iterate(padded_text);
    ondemand::array arr = doc.get_array();
    auto data = (*arr.begin())["data"]["courses"].get_array();
    fmt::print("Found courses :\n");
    std::map<std::string, int64_t> ret;
    for (ondemand::object it : data) {
        auto id = it.find_field("id").get_uint64().value();
        auto name = it.find_field("fullname").get_string().value();
        fmt::print("     {}\n", name);
        ret.emplace(name, id);
    }

    return ret;
}

std::optional<std::string> get_course_items(cpr::Session &session,
                                            long course_id,
                                            const std::string &session_key,
                                            ondemand::parser &parser)
{
    //    auto session_key = get_sesskey(session);
    auto url = fmt::format("https://upel.agh.edu.pl/lib/ajax/"
                           "service.php?sesskey={}&info=core_courseformat_get_state",
                           session_key);
    session.SetUrl(cpr::Url{url});
    auto body = R"([{"index":0,"methodname":"core_courseformat_get_state","args":{"courseid":)"
                + std::to_string(course_id) + " }}]";
    session.SetBody(cpr::Body{body});
    auto r = session.Post();
    //    ondemand::parser parser;
    padded_string padded_text{r.text};
    auto doc = parser.iterate(padded_text);
    ondemand::array arr = doc.get_array();
    auto data = (*arr.begin())["data"].get_string();
    padded_string data_str{data.value()};
    doc = parser.iterate(data_str);
    auto main_obj = doc.get_object();

    fmt::print("Detected sections:\n");

    struct NameUrl
    {
        std::string_view name;
        std::string_view url;
    };

    std::vector<NameUrl> attendance;
    for (ondemand::field field : main_obj) {
        std::cout << "\t## " << field.key() << " ##\n";
        auto arr = field.value().get_array();
        if (arr.error())
            continue;
        for (ondemand::object it : arr) {
            auto name = it.find_field_unordered("name").get_string();
            if (name.error()) {
                name = it.find_field_unordered("title").get_string();
                if (name.error())
                    continue;
            }
            auto url = it.find_field_unordered("url").get_string();
            bool is_attendance = !url.error()
                                 && url.value().find("attendance") != std::string::npos;
            if (is_attendance) {
                fmt::print(fg(fmt::color::yellow), "\t{}\n", name.value());
                attendance.push_back(NameUrl{name.value(), url.value()});
            } else {
                fmt::print("\t{}\n", name.value());
            }
        }
    }
    if (attendance.empty()) {
        fmt::print(stderr, "Could not find any attendance sites\n");
        exit(1);
    }
    auto used_url = std::find_if(attendance.begin(), attendance.end(), [](auto &name_url) {
        auto &str = name_url.name;
        return str.find("Test programu studenta") != std::string::npos;
    });
    if (used_url == attendance.end()) {
        fmt::print(stderr, "Could not find attendance containing specified parameters, exiting");
        exit(2);
    }
    if (attendance.size() > 1) {
        // TODO:
    }
    fmt::print("Using attendance: {} ({})\n", used_url->name, used_url->url);
    //    std::string attendance_url{attendance[0].url.data(), attendance[0].url.size()};
    return std::string(used_url->url);
}

void register_attendance(cpr::Session &session,
                         const std::string &attendance_url,
                         const std::string &session_key)
{
    session.SetUrl(cpr::Url{attendance_url});
    auto attendance_page = session.Get();
    const auto &html = attendance_page.text;
    auto start = html.find(R"(href="https://upel.agh.edu.pl/mod/attendance/attendance.php?sessid=)");
    if (start == std::string::npos) {
        fmt::print(stderr, "Could not find attendance link, exiting :(\n");
        return;
    }
    start = html.find('"', start + 1);
    auto end = html.find('"', start + 1);
    auto url = html.substr(start, end - start);
    boost::replace_all(url, "amp;", "");
    fmt::print("Found url {}\n", url);
    session.SetUrl(cpr::Url(url));
    auto r = session.Get();
    fmt::print("Visited url, got back {} status code\n", r.status_code);
}

int main(int argc, const char *argv[])
{
    State state(argc, argv);
    // if (argc < 2) {
    //     fmt::print(stderr, "Usage: {} <credentials filepath>\n", argv[0]);
    //     return -1;
    // }
    //    spdlog::info("Welcome to spdlog!");

    // const fs::path credentials_path{argv[1]};
    // Credentials creds(credentials_path);
    auto &creds = state.creds;
    ondemand::parser json_parser{};
    auto [session, sesskey] = login(creds);
    auto courses = get_courses(session, sesskey, json_parser);
    auto cpp_it = std::find_if(courses.begin(), courses.end(), [](auto pair) {
        return pair.first.find("C++") != std::string::npos;
    });
    assert(cpp_it != courses.end());
    auto cpp_id = cpp_it->second;
    auto attendence_url = get_course_items(session, cpp_id, sesskey, json_parser).value();
    register_attendance(session, attendence_url, sesskey);

    return 0;
}
