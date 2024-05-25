#include "client/http/web_client.hpp"
#include "common/command_line_parser.hpp"
#include "common/utilities.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

constexpr const char* x509Comment = "Generated from OpenBMC service";
using namespace reactor;
constexpr const char* trustStorePath =
    "/Users/abhilashraju/work/cpp/bmcgw/build/cert_generator/";
ssl::context getContext()
{
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_verify_mode(ssl::verify_peer);
    ctx.load_verify_file(std::string(trustStorePath) + "ca-cer.pem");
    ctx.use_certificate_chain_file(std::string(trustStorePath) +
                                   "client-cert.pem");
    ctx.use_private_key_file(std::string(trustStorePath) + "client-key.pem",
                             ssl::context::pem);
    return ctx;
}
int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [ip, port, cert] = getArgs(parseCommandline(argc, argv), "-ip", "-p",
                                    "-certdir");
    reactor::getLogger().setLogLevel(LogLevel::ERROR);
    if (port.empty() || cert.empty() || ip.empty())
    {
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR(
            "eg: httpclient -ip ipaddress -p port -certdir dirpath");
        return 0;
    }
    try
    {
        net::io_context ioc;
        using Client = WebClient<CoroSslStream, http::string_body>;
        auto ctx = getContext();
        auto mono =
            Client::builder()
                .withSession(ioc.get_executor(), ctx)
                .withEndpoint(std::format("https://{}:{}/hello/me", ip, port))
                .create()
                .get()
                .toMono();
        mono->subscribe([](auto& v) {
            if (v.isError())
            {
                REACTOR_LOG_ERROR("Error: {}", v.error().message());
                return;
            }
            REACTOR_LOG_ERROR("Response: {}", v.response().body());
        });
        ioc.run();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }

    return 0;
}
