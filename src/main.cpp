#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cpr/cpr.h>
#include <fmt/color.h>
#include <fmt/printf.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <utility>

using json = nlohmann::json;
using namespace simdjson; // optional

struct Credentials
{
    std::string username = "";
    std::string password = "";
    Credentials(const fs::path &filename_path)
    {
        std::ifstream file(filename_path);
        std::getline(file, username, ':');
        std::getline(file, password);
        file.close();
    }
};

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

auto login(const Credentials &creds)
{
    cpr::Session session;
    session.SetHttpVersion(cpr::HttpVersion(cpr::HttpVersionCode::VERSION_2_0));
    session.SetUrl(cpr::Url{"https://upel.agh.edu.pl/login/index.php"});
    const auto r = session.Get();
    std::string html = r.text;
    auto start = r.text.find("https://upel.agh.edu.pl/auth/saml2/login.php");
    auto length = r.text.find('"', start + 1) - start;
    std::string next_url = html.substr(start, length);
    boost::replace_all(next_url, "amp;", "");

    session.SetUrl(cpr::Url{next_url});
    const auto r2 = session.Get();
    //    std::cout << "r2 redirects : " << r2.redirect_count << '\n';
    //    std::cout << "r2 status code : " << r2.status_code << '\n';
    //    std::cout << r2.url.str() << "\n";
    //    std::cout << r2.text << "\n";
    auto url_str = find_str_after(R"(<input type="hidden" name="url" value=")", r2.text);
    auto skin_str = find_str_after(R"(<input type="hidden" name="skin" value=")", r2.text);
    auto token_str = find_str_after(R"(<input id="token" type="hidden" name="token" value=")",
                                    r2.text);
    std::cout << "URL : " << url_str << '\n';
    std::cout << "SKIN : " << skin_str << '\n';
    std::cout << "TOKEN : " << token_str << '\n';

    session.SetUrl(r2.url);
    session.SetParameters(cpr::Parameters{{"url", url_str},
                                          {"timezone", "2"},
                                          {"skin", skin_str},
                                          {"token", token_str},
                                          {"user", creds.username},
                                          {"password", creds.password}});
    auto p = session.Post();
    std::cout << "post responded with " << p.status_code << '\n';

    auto relay_state = find_str_after(R"(<input type="hidden" name="RelayState" value=")", p.text);
    auto saml_response = find_str_after(R"(<input type="hidden" name="SAMLResponse" value=")",
                                        p.text);

    boost::replace_all(relay_state, "amp;", "");
    boost::replace_all(saml_response, "amp;", "");
    session.SetUrl(cpr::Url("https://upel.agh.edu.pl/auth/saml2/sp/saml2-acs.php/upel.agh.edu.pl"));
    cpr::Parameters param = {{"RelayState", relay_state}, {"SAMLResponse", saml_response}};
    session.SetParameters(cpr::Parameters{}); // Reset parameters
    auto content = param.GetContent(*session.GetCurlHolder().get());
    session.SetBody(cpr::Body(content));
    auto p2 = session.Post();
    std::cout << "Status code : " << p2.status_code << '\n';
    std::cout << "New url : " << p2.url.str() << '\n';
    std::cout << "Took (s) : " << p2.elapsed << '\n';

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
    if (attendance.size() > 1) {
        // TODO:
    }
    fmt::print("Using attendance {}\n", attendance[0].name);
    std::string attendance_url{attendance[0].url.data(), attendance[0].url.size()};
    return attendance_url;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fmt::print(stderr, "Usage: {} <credentials filepath>\n", argv[0]);
        return -1;
    }
    //    spdlog::info("Welcome to spdlog!");

    const fs::path credentials_path{argv[1]};
    Credentials creds(credentials_path);
    ondemand::parser json_parser{};
    auto [session, sesskey] = login(creds);
    auto courses = get_courses(session, sesskey, json_parser);
    auto cpp_it = std::find_if(courses.begin(), courses.end(), [](auto pair) {
        return pair.first.find("C++") != std::string::npos;
    });
    assert(cpp_it != courses.end());
    auto cpp_id = cpp_it->second;
    get_course_items(session, cpp_id, sesskey, json_parser);

    return 0;
}
