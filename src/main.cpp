#include <boost/algorithm/string/replace.hpp>
#include <cpr/cpr.h>
#include <fmt/printf.h>
#include <iostream>

struct Credentials
{
    std::string username;
    std::string password;
    Credentials(const fs::path &filename_path)
    {
        std::ifstream file(filename_path);
        std::getline(file, username, ':');
        std::getline(file, password);
        file.close();
    }
};

std::string find_str_after(const std::string &start, const std::string &html)
{
    auto start_pattern = html.find(start);
    auto start_str     = start_pattern + start.size();
    auto end_str       = html.find('"', start_str);
    return html.substr(start_str, end_str - start_str);
}

cpr::Session login(const Credentials &creds)
{
    cpr::Session session;
    session.SetUrl(cpr::Url{"https://upel.agh.edu.pl/login/index.php"});
    const auto r = session.Get();
    //    std::cout << r.text << std::endl;
    std::string  html     = r.text;
    auto         start    = r.text.find("https://upel.agh.edu.pl/auth/saml2/login.php");
    auto         length   = r.text.find('"', start + 1) - start;
    std::string  next_url = html.substr(start, length);
    boost::replace_all(next_url, "amp;", "");
    //    std::cout << next_url << "\n";

    session.SetUrl(cpr::Url{next_url});
    const auto r2 = session.Get();
    //    std::cout << "r2 redirects : " << r2.redirect_count << '\n';
    //    std::cout << "r2 status code : " << r2.status_code << '\n';
    std::cout << r2.url.str() << "\n";
    //    std::cout << r2.text << "\n";
    auto url_str   = find_str_after(R"(<input type="hidden" name="url" value=")", r2.text);
    auto skin_str  = find_str_after(R"(<input type="hidden" name="skin" value=")", r2.text);
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
    session.SetParameters(cpr::Parameters{});
    auto content = param.GetContent(*session.GetCurlHolder().get());
    session.SetBody(cpr::Body(content));
    //    session.SetHttpVersion(cpr::HttpVersion(cpr::HttpVersionCode::VERSION_1_1));
    auto p2 = session.Post();
    std::cout << "Status code : " << p2.status_code << '\n';
    std::cout << "New url : " << p2.url.str() << '\n';
    std::cout << "Took (s) : " << p2.elapsed << '\n';

    session.SetUrl(p2.url);
    auto courses = session.Get();
    std::cout << "Got url : " << courses.url.str() << '\n';
    std::cout << "Text: " << courses.text << '\n';

    return session;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fmt::print(stderr, "Usage: {} <credentials filepath>\n", argv[0]);
        return -1;
    }
    const auto  credentials_path = argv[1];
    Credentials creds(credentials_path);
    auto        session = login(creds);
    return 0;
}
