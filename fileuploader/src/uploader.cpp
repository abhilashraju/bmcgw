#include "command_line_parser.hpp"
#include "tcp_client.hpp"

#include <fstream>
#include <iostream>
namespace bmcgw
{
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
        auto iter = std::ranges::find_if(monitorConfigs, [&filePath](auto& v) {
            return v.filePath == filePath;
        });
        if (iter == std::end(monitorConfigs))
        {
            monitorConfigs.push_back(
                {std::filesystem::file_time_type{}, filePath});
        }

        if (std::filesystem::is_directory(filePath))
        {
            std::filesystem::directory_iterator dir(filePath);
            for (auto& file : dir)
            {
                addConfig(file.path());
            }
            return;
        }
    }
    void executeGuarded(auto&& func)
    {
        try
        {
            func();
        }

        catch (std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << "\n";
        }
    }
    Session(std::string_view p, std::string_view f, int interval) :
        interval(interval), port(p.data(), p.size())
    {
        addConfig(std::filesystem::path(f));
        net::spawn(&io_context, [this](net::yield_context yield) {
            executeGuarded(
                std::bind_front(&Session::monitorFiles, this, yield));
        });
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
                    executeGuarded(std::bind_front(&Session::upload_file, this,
                                                   config.filePath, yield));
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

    void applyDirectoryChanges(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
        {
            std::filesystem::directory_iterator dir(path);
            for (auto& file : dir)
            {
                addConfig(file.path());
            }
        }
    }
    void upload_file(const std::string& file_path, net::yield_context yield)
    {
        if (std::filesystem::is_directory(file_path))
        {
            applyDirectoryChanges(file_path);
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file)
        {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return;
        }
        ssl::context ssl_context(ssl::context::sslv23_client);
        TcpClient client(io_context, ssl_context);
        if (!client.connect("localhost", port, yield))
        {
            return;
        }
        std::string request = std::format("Filename: {}\r\n\r\n", file_path);
        if (!client.send(request, yield))
        {
            return;
        }
        char buffer[1024];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
        {
            if (!client.send(std::string_view(buffer, file.gcount()), yield))
            {
                return;
            }
            std::cout << "Bytes sent: " << file.gcount() << "\n";
        }

        std::cout << "File uploaded successfully.\n";
    }
};
} // namespace bmcgw
using namespace bmcgw;
int main(int argc, const char* argv[])
{
    auto [port, file, interval] =
        bmcgw::getArgs(bmcgw::parseCommandline(argc, argv), "-p", "-f", "-i");

    Session session(port, file, interval.size() ? atoi(interval.data()) : 10);

    session.run();

    return 0;
}
