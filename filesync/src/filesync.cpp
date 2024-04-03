#include "asyncserver.hpp"
#include "command_line_parser.hpp"
#include "file_handler.hpp"
#include "sync.hpp"

#include <utilities.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

constexpr const char* x509Comment = "Generated from OpenBMC service";
using namespace bmcgw;

int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [port, cert] = getArgs(parseCommandline(argc, argv), "-p", "-certdir");
    if (port.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout << "eg: bmcfilesync -p port \n";
        return 0;
    }
    try
    {
        SyncHandler handler;
#ifdef SSL_ON
        AsyncSslServer<decltype(handler)> server(handler, port, cert.data());
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
