#include "command_line_parser.hpp"
#include "file_sync_session.hpp"

#include <iostream>

using namespace reactor;
int main(int argc, const char* argv[])
{
    auto [port, file, interval, server] = getArgs(parseCommandline(argc, argv),
                                                  "-p", "-f", "-i", "-s");

    net::io_context io_context;
    Session session(io_context, server, port, file,
                    interval.size() ? atoi(interval.data()) : 10);

    session.run();

    return 0;
}
