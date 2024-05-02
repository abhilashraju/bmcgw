#include "common/command_line_parser.hpp"
#include "request_forwarder.hpp"
#include "server/http/http_handler.hpp"
#include "server/tcp/asyncserver.hpp"

#include <iostream>

using namespace reactor;
using namespace reverseproxy;
int main(int argc, const char* argv[])
{
    auto [port, conf, certDir] = getArgs(parseCommandline(argc, argv), "-p",
                                         "-c", "-certdir");

    net::io_context io_context;
    RequestForwarder router(std::string{conf.data(), conf.length()});

    HttpHandler handler(router);
    AsyncSslServer<decltype(handler)> server(handler, port, certDir);
    server.start();
    return 0;
}
