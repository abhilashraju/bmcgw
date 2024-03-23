#include "bmcgw.hpp"

#include "asyncserver.hpp"
#include "command_line_parser.hpp"

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
    auto [port, root] = getArgs(parseCommandline(argc, argv), "-p", "-r");
    if (port.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout << "eg: bmcgw -p port\n";
        return 0;
    }
    try
    {
        Aggregator aggregator;
#ifdef SSL_ON
        AsyncSslServer<Aggregator> server(
            aggregator, port,
            "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
            "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
            "/etc/ssl/certs/authority");
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
