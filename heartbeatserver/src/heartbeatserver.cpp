#include "client/udp/udp_client.hpp"
#include "common/command_line_parser.hpp"
#include "nlohmann/json.hpp"
#include "server/udp/udp_server.hpp"

#include <fstream>
using namespace reactor;
struct HeartBeatServer
{
    UdpServer<HeartBeatServer> server;
    HeartBeatServer(std::string_view port) : server(port, *this) {}
    void start()
    {
        server.start();
    }
    net::io_context& getIoContext()
    {
        return server.getIoContext();
    }
    void handleRead(const error_code& ec, std::string_view data,
                    const udp::endpoint& ep, net::yield_context y, auto&& cont)
    {
        if (ec)
        {
            REACTOR_LOG_ERROR("Error receiving data: {}", ec.message());
            return;
        }

        REACTOR_LOG_DEBUG(
            "Received From {}, data: {}",
            ep.address().to_string() + ":" + std::to_string(ep.port()), data);
        // cont(data);
    }
};
struct HeatBeatClient
{
    using Targets = std::vector<std::pair<std::string, std::string>>;
    HeatBeatClient(net::io_context& ioc, Targets&& tars, int interval = 5) :
        ioc(ioc), timer(ioc), targets(std::move(tars)), interval(interval)
    {}
    void start()
    {
        net::spawn(ioc, [this](net::yield_context y) {
            timer.expires_after(std::chrono::seconds(interval));
            error_code ec{};
            timer.async_wait(y[ec]);
            if (!ec)
            {
                std::string_view data = "ping";
                for (const auto& [host, port] : targets)
                {
                    UdpClient<1024>::send_to(ioc, y, host, port,
                                             net::buffer(data));
                }
            }
            start();
        });
    }
    net::io_context& ioc;
    net::steady_timer timer;
    Targets targets;
    int interval;
};
int main(int argc, const char* argv[])
{
    auto [conf] = getArgs(parseCommandline(argc, argv), "-conf");
    if (conf.empty())
    {
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR("eg: heartbeatserver -c config.json");
        return 0;
    }
    try
    {
        std::ifstream file(conf);
        if (!file.is_open())
        {
            REACTOR_LOG_ERROR("File not found: {}", conf);
            return 0;
        }
        std::string fileContent((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        nlohmann::json j = nlohmann::json::parse(fileContent);
        reactor::getLogger().setLogLevel(LogLevel::DEBUG);

        HeartBeatServer server(j["port"].get<std::string>());

        HeatBeatClient client(server.getIoContext(),
                              j["targets"].get<HeatBeatClient::Targets>(),
                              j["interval"].get<int>());
        client.start();
        server.start();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }
}
