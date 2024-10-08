#include "common/command_line_parser.hpp"
#include "common/utilities.hpp"
#include "file_handler.hpp"
#include "file_sync_session.hpp"
#include "server/tcp/asyncserver.hpp"
#include "sync.hpp"

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
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR(
            "eg: bmcfilesync -p port -certdir dirpath -c configfile");
        return 0;
    }
    try
    {
        net::io_context ioc;
        reactor::getLogger().setLogLevel(LogLevel::ERROR);
        SyncHandler handler;
#ifdef SSL_ON
        AsyncSslServer<decltype(handler), MtlsStreamMaker> server(
            ioc, handler, port, cert.data());
#else

#endif

        std::ifstream file(std::string{conf.data(), conf.size()});
        if (!file.is_open())
        {
            REACTOR_LOG_ERROR("Invalid config file");
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
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }

    return 0;
}
