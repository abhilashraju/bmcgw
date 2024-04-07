#include "command_line_parser.hpp"
#include "ssl_utils.hpp"
#include "tcp_client.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
using namespace reactor;
int main(int argc, const char* argv[])
{
    auto [server, p, user, ps, certdir] = getArgs(
        parseCommandline(argc, argv), "-s", "-p", "-u", "-ps", "-certdir");
    reactor::getLogger().setLogLevel(LogLevel::ERROR);
    net::io_context io_context;
    ssl::context ssl_context =
        ensuressl::loadCertificate(boost::asio::ssl::context::tls_client,
                                   {certdir.data(), certdir.size()});

    TcpClient client(io_context, ssl_context);

    net::spawn(&io_context,
               [&client, &server, &p, &user, &ps](net::yield_context yield) {
        if (!client.connect(server, p, yield))
        {
            return;
        }
        nlohmann::json j;
        j["username"] = user;
        j["password"] = ps;
        std::string request = std::format("Authentication: \r\n\r\n{}",
                                          j.dump());
        client.send(request, yield);
    });

    io_context.run();
    return 0;
}
