#include "bmcgw.hpp"

#include "command_line_parser.hpp"
#include "http_server.hpp"

#include <exec/static_thread_pool.hpp>
#include <utilities.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace chai;
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
    exec::static_thread_pool threadPool;
    Aggregator aggregator;

#ifdef SSL_ON
    SSlServer server(
        port, aggregator,
        "/Users/abhilashraju/work/cpp/chai/certs/server-certificate.pem",
        "/Users/abhilashraju/work/cpp/chai/certs/server-private-key.pem",
        "/etc/ssl/certs/authority");
#else
    TCPServer server(port, aggregator);
#endif

    server.start(threadPool);
    return 0;
}
