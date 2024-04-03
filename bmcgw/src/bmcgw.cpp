#include "bmcgw.hpp"

#include "asyncserver.hpp"
#include "command_line_parser.hpp"
#include "http_handler.hpp"

#include <utilities.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
constexpr const char* trustStorePath = "/etc/ssl/certs/authority";
constexpr const char* x509Comment = "Generated from OpenBMC service";
using namespace bmcgw;

int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [port, conffile, certDir] = getArgs(parseCommandline(argc, argv), "-p",
                                             "-c", "-certdir");
    if (port.empty() || conffile.empty() || certDir.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout
            << "eg: bmcgw -p port -c config path , -certdir dir-of-ss-cirt\n";
        return 0;
    }
    try
    {
        std::ifstream file(std::string{conffile.data(), conffile.size()});
        if (!file.is_open())
        {
            std::cout << "Invalid config file\n";
            return 0;
        }
        std::string conf((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        Aggregator aggregator(conf);
        HttpHandler handler(aggregator);
#ifdef SSL_ON
        AsyncSslServer<decltype(handler)> server(handler, port, certDir);
#else

#endif

        server.start();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << '\n';
    }

    return 0;
}
