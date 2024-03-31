#include "command_line_parser.hpp"
#include "tcp_client.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
using namespace bmcgw;
int main(int argc, const char* argv[])
{
    auto [server, p, user, ps] = bmcgw::getArgs(
        bmcgw::parseCommandline(argc, argv), "-s", "-p", "-u", "-ps");

    net::io_context io_context;
    ssl::context ssl_context(ssl::context::sslv23_client);
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