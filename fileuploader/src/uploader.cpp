#include "command_line_parser.hpp"
#include "file_sync_session.hpp"

#include <iostream>

using namespace reactor;
int main(int argc, const char* argv[])
{
    auto [port, conf] = getArgs(parseCommandline(argc, argv), "-p", "-c");

    net::io_context io_context;
    std::ifstream file(std::string{conf.data(), conf.size()});
    if (!file.is_open())
    {
        std::cout << "Invalid config file\n";
        return 0;
    }
    std::string confStr((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    nlohmann::json j = nlohmann::json::parse(confStr);
    FileSyncSession session(io_context, j);

    session.run();

    return 0;
}
