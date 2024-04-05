#pragma once
#include "beast_defs.hpp"
#include "syncdb.hpp"
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
        std::vector<std::string> paths;
        std::ranges::transform(
            j["paths"], std::back_inserter(paths),
            [](const auto& path) { return std::string{path}; });
        SyncDb::globalSyncDb().addPaths(paths);
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
            REACTOR_LOG_DEBUG("Timer error: {}", ec.message());
            return;
        }
        SyncDb::globalSyncDb().checkChanges([this](auto path) {
            net::spawn(&io_context, [this, path = std::move(path)](
                                        net::yield_context yield) {
                executeGuarded(std::bind_front(&FileSyncSession::upload_file,
                                               this, std::move(path)),
                               yield);
            });
        });

        net::spawn(&io_context, [this](net::yield_context yield) {
            executeGuarded(
                std::bind_front(&FileSyncSession::monitorFiles, this), yield);
        });
    }
    void run()
    {
        io_context.run();
    }

    void upload_file(const std::string& file_path, net::yield_context yield)
    {
        if (std::filesystem::is_directory(file_path))
        {
            SyncDb::globalSyncDb().applyDirectoryChanges(file_path);
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file)
        {
            REACTOR_LOG_DEBUG("Failed to open file: {}", file_path);
            return;
        }
        ssl::context ssl_context(ssl::context::sslv23_client);
        TcpClient client(io_context, ssl_context);
        if (!client.connect(server, port, yield))
        {
            REACTOR_LOG_DEBUG("Failed to connect {}:{}", server, port);
            return;
        }
        std::string request = std::format("Filename: {}\r\n\r\n", file_path);
        if (!client.send(request, yield))
        {
            REACTOR_LOG_DEBUG("Failed to send {}", file_path);
            return;
        }
        char buffer[1024];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
        {
            if (!client.send(std::string_view(buffer, file.gcount()), yield))
            {
                return;
            }
        }
        REACTOR_LOG_DEBUG("File {} uploaded successfully", file_path);
    }
};
} // namespace reactor
