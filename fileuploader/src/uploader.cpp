#include "command_line_parser.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>

#include <fstream>
#include <iostream>
namespace net = boost::asio;
namespace beast = boost::beast;
using net::awaitable;
using net::co_spawn;
using net::detached;
using net::use_awaitable;
using net::ip::tcp;
namespace ssl = net::ssl;
using ssl::stream;
struct Session
{
    net::io_context io_context;
    std::chrono::seconds interval{10};
    net::steady_timer timer{io_context, interval};
    std::string port;

    struct MonitorConfigs
    {
        std::filesystem::file_time_type lastWriteTime;
        std::filesystem::path filePath;
    };
    std::vector<MonitorConfigs> monitorConfigs;
    void addConfig(std::filesystem::path filePath)
    {
        if (std::filesystem::is_directory(filePath))
        {
            std::filesystem::directory_iterator dir(filePath);
            for (auto& file : dir)
            {
                addConfig(file.path());
            }
            return;
        }
        monitorConfigs.push_back(
            {std::filesystem::last_write_time(filePath), filePath});
    }
    Session(std::string_view p, std::string_view f) : port(p.data(), p.size())
    {
        addConfig(std::filesystem::path(f));
        net::spawn(&io_context,
                   [this](net::yield_context yield) { monitorFiles(yield); });
    }
    void monitorFiles(net::yield_context yield)
    {
        timer.expires_after(interval);
        beast::error_code ec{};
        timer.async_wait(yield[ec]);
        if (ec)
        {
            std::cout << ec.message() << "\n";
            return;
        }
        for (auto& config : monitorConfigs)
        {
            auto currentWriteTime =
                std::filesystem::last_write_time(config.filePath);
            if (currentWriteTime != config.lastWriteTime)
            {
                config.lastWriteTime = currentWriteTime;
                net::spawn(&io_context,
                           [this, config](net::yield_context yield) {
                    upload_file(config.filePath, yield);
                });
            }
        }

        net::spawn(&io_context,
                   [this](net::yield_context yield) { monitorFiles(yield); });
    }
    void run()
    {
        io_context.run();
    }
    bool checkFailed(beast::error_code& ec)
    {
        if (ec)
        {
            std::cout << ec.message() << "\n";
            return true;
        }
        return false;
    }
    void upload_file(const std::string& file_path, net::yield_context yield)
    {
        try
        {
            tcp::resolver resolver(io_context);
            ssl::context ssl_context(ssl::context::sslv23_client);

            stream<tcp::socket> socket(io_context, ssl_context);
            beast::error_code ec{};
            auto endpoints = resolver.async_resolve("localhost", port,
                                                    yield[ec]);
            if (checkFailed(ec))
            {
                return;
            }
            net::async_connect(socket.next_layer(), endpoints, yield[ec]);
            if (checkFailed(ec))
            {
                return;
            }
            socket.async_handshake(ssl::stream_base::client, yield[ec]);
            if (checkFailed(ec))
            {
                return;
            }
            std::ifstream file(file_path, std::ios::binary);
            if (!file)
            {
                std::cerr << "Failed to open file: " << file_path << std::endl;
                return;
            }

            std::string request = std::format("filename: {}\r\n\r\n",
                                              file_path);
            net::async_write(socket, net::buffer(request), yield[ec]);
            if (checkFailed(ec))
            {
                return;
            }
            char buffer[1024];
            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
            {
                net::async_write(socket, net::buffer(buffer, file.gcount()),
                                 yield[ec]);
                std::cout << "Bytes sent: " << file.gcount() << "\n";
                if (checkFailed(ec))
                {
                    return;
                }
            }

            std::cout << "File uploaded successfully.\n";
        }
        catch (std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << "\n";
        }
    }
};

int main(int argc, const char* argv[])
{
    auto [port, file] = bmcgw::getArgs(bmcgw::parseCommandline(argc, argv),
                                       "-p", "-f");

    Session session(port, file);

    session.run();

    return 0;
}
