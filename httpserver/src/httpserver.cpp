#include "common/command_line_parser.hpp"
#include "common/utilities.hpp"
#include "dump_uploader.hpp"
#include "hello.hpp"
#include "server/http_server.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

constexpr const char* x509Comment = "Generated from OpenBMC service";
using namespace reactor;

int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [port, cert] = getArgs(parseCommandline(argc, argv), "-p", "-certdir");
    if (port.empty() || cert.empty())
    {
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR("eg: httpserver -p port -certdir dirpath");
        return 0;
    }
    try
    {
        reactor::getLogger().setLogLevel(LogLevel::ERROR);

        HttpsServer server(port, cert);
        redfish::DumpUploader uploader(server.router(), "/tmp");
        redfish::Hello hello(server.router());
        server.start();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }

    return 0;
}