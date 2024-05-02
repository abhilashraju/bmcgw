#include "client/udp/udp_client.hpp"
#include "common/command_line_parser.hpp"
#include "server/udp/udp_server.hpp"
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
                    net::yield_context y, auto&& cont)
    {
        if (ec)
        {
            REACTOR_LOG_ERROR("Error receiving data: {}", ec.message());
            return;
        }

        REACTOR_LOG_DEBUG("Received: {}", data);
        cont(data);
    }
};
struct HeatBeatClient
{
    HeatBeatClient(net::io_context& ioc, std::string_view target,
                   std::string_view port) :
        ioc(ioc),
        timer(ioc), host(target.data(), target.length()),
        port(port.data(), port.length())
    {}
    void start()
    {
        net::spawn(ioc, [this](net::yield_context y) {
            timer.expires_after(std::chrono::seconds(1));
            error_code ec{};
            timer.async_wait(y[ec]);
            if (!ec)
            {
                std::string_view data = "ping";
                UdpClient<1024>::send_to(
                    ioc, y, host, port, net::buffer(data),
                    [this](const auto& ec, auto&& res) {
                    REACTOR_LOG_DEBUG("Received Client: {}", res);
                },
                    true);
            }
            start();
        });
    }
    net::io_context& ioc;
    net::steady_timer timer;
    std::string host;
    std::string port;
};
int main(int argc, const char* argv[])
{
    auto [rip, rp, port] = getArgs(parseCommandline(argc, argv), "-rip", "-rp",
                                   "-p");
    if (port.empty() || rip.empty() || rp.empty())
    {
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR(
            "eg: heartbeatserver -p port -rip remote_ip -rp remote_port");
        return 0;
    }
    try
    {
        reactor::getLogger().setLogLevel(LogLevel::DEBUG);

        HeartBeatServer server(port);
        HeatBeatClient client(server.getIoContext(), rip, rp);
        client.start();
        server.start();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }
}
