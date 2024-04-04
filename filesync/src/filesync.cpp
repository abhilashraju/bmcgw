#include "asyncserver.hpp"
#include "command_line_parser.hpp"
#include "file_handler.hpp"
#include "file_sync_session.hpp"
#include "sync.hpp"

#include <utilities.hpp>

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
    auto [port, cert, conf] = getArgs(parseCommandline(argc, argv), "-p",
                                      "-certdir", "-c");
    if (port.empty() || cert.empty() || conf.empty())
    {
        std::cout << "Invalid arguments\n";
        std::cout << "eg: bmcfilesync -p port -certdir dirpath -c configfile\n";
        return 0;
    }
    try
    {
        SyncHandler handler;
#ifdef SSL_ON
        AsyncSslServer<decltype(handler)> server(handler, port, cert.data());
#else

#endif
        std::ifstream file(std::string{conf.data(), conf.size()});
        if (!file.is_open())
        {
            std::cout << "Invalid config file\n";
            return 0;
        }
        std::string confStr((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        nlohmann::json j = nlohmann::json::parse(confStr);

        FileSyncSession session(server.getIoContext(), j);
        server.start();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << '\n';
    }

    return 0;
}
