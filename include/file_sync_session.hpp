#pragma once
#include "beast_defs.hpp"
#include "tcp_client.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
namespace reactor
{
struct FileSyncSession
{
    net::io_context& io_context;
    std::chrono::seconds interval{10};
    net::steady_timer timer{io_context, interval};
    std::string port;
    std::string server{"localhost"};

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
    void executeGuarded(auto&& func, net::yield_context yield)
    {
        try
        {
            func(yield);
        }

        catch (std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << "\n";
            net::spawn(&io_context, [this, func = std::move(func)](
                                        net::yield_context yield) {
                executeGuarded(std::move(func), yield);
            });
        }
    }
    FileSyncSession(net::io_context& ctx, const nlohmann::json& j) :
        io_context(ctx)
    {
        interval = std::chrono::seconds{j["interval"]};
        server = j["server"];
        port = j["port"];
        for (auto& file : j["paths"])
        {
            addConfig(file);
        }
        net::spawn(&io_context, [this](net::yield_context yield) {
            executeGuarded(
                std::bind_front(&FileSyncSession::monitorFiles, this), yield);
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
                    executeGuarded(
                        std::bind_front(&FileSyncSession::upload_file, this,
                                        config.filePath),
                        yield);
                });
            }
        }

        net::spawn(&io_context, [this](net::yield_context yield) {
            executeGuarded(
                std::bind_front(&FileSyncSession::monitorFiles, this), yield);
        });
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
        if (!client.connect(server, port, yield))
        {
            std::cerr << std::format("Failed to connect {}:{}", server, port);
            return;
        }
        std::string request = std::format("Filename: {}\r\n\r\n", file_path);
        if (!client.send(request, yield))
        {
            std::cerr << std::format("Failed to send {}", file_path);
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
} // namespace reactor
