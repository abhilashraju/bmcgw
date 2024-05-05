#include "client/udp/udp_client.hpp"
#include "common/command_line_parser.hpp"
#include "common/utilities.hpp"
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
    bool checkSelf(const udp::endpoint& ep)
    {
        udp::resolver resolver(getIoContext());
        udp::resolver::query query(boost::asio::ip::host_name(), "");
        udp::endpoint sep = *resolver.resolve(query);

        return ep.address() != sep.address();
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
        if (data.starts_with("Connect:") && !checkSelf(ep))
        {
            cont(std::string_view("Connected"));
        }
    }
};
struct HeatBeatClient
{
    using Targets = std::vector<std::pair<std::string, std::string>>;
    HeatBeatClient(net::io_context& ioc, const std::string& sp, Targets&& tars,
                   int interval = 5) :
        ioc(ioc),
        timer(ioc), targets(std::move(tars)), servPort(sp), interval(interval)
    {}
    void findPeers(net::yield_context y)
    {
        UdpClient<1024>::broadcast(
            ioc, y, servPort, net::buffer("Connect:"),
            [this](const error_code& ec, const auto& ep,
                   std::string_view data) {
            if (ec)
            {
                REACTOR_LOG_ERROR("Error sending data: {}", ec.message());
                return;
            }
            REACTOR_LOG_DEBUG("Received: {}", data);
            if (data.starts_with("Connected"))
            {
                targets.push_back(std::make_pair(ep.address().to_string(),
                                                 std::to_string(ep.port())));
                sendBeats();
            }
                },
            true, std::chrono::seconds(15));
    }
    void sendBeats()
    {
        if (!sending)
        {
            net::spawn(ioc, [this](net::yield_context y) {
                timer.expires_after(std::chrono::seconds(interval));
                error_code ec{};
                sending = true;
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
                sending = false;
                sendBeats();
            });
        }
    }
    void start()
    {
        if (targets.empty())
        {
            net::spawn(ioc, std::bind_front(&HeatBeatClient::findPeers, this));
            return;
        }
        sendBeats();
    }
    net::io_context& ioc;
    net::steady_timer timer;
    Targets targets;
    std::string servPort;
    int interval;
    bool sending{false};
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
        std::ifstream file({conf.data(), conf.length()});
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
        auto targets = j["targets"].get<HeatBeatClient::Targets>();
        HeatBeatClient client(server.getIoContext(),
                              j["port"].get<std::string>(), std::move(targets),
                              j["interval"].get<int>());
        client.start();
        server.start();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }
}
