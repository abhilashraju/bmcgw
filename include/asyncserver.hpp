#include "streams.hpp"
namespace bmcgw
{

template <typename StreamMaker, typename Router>
struct AsyncServer
{
    net::io_context ioc_;
    Router& router_;
    tcp::acceptor acceptor_;
    StreamMaker streamMaker;

    AsyncServer(Router& rout, std::string_view port,
                StreamMaker&& streamMaker) :
        router_(rout),
        acceptor_(ioc_, tcp::endpoint(tcp::v4(), std::atoi(port.data()))),
        streamMaker(std::move(streamMaker))
    {}
    void start()
    {
        acceptor_.listen(net::socket_base::max_listen_connections);
        waitForAsyncConnection();
        ioc_.run();
    }
    void waitForAsyncConnection()
    {
        auto asyncWork = [this](auto streamReader, net::yield_context yield) {
            handleRead(ioc_, std::move(streamReader), router_, yield);
        };
        streamMaker.acceptAsyncConnection(ioc_, acceptor_,
                                          std::move(asyncWork));
    }

    static inline void handleRead(auto& ioc, auto&& streamReader, auto& router,
                                  net::yield_context yield)
    {
        beast::flat_buffer buffer;
        StringbodyRequest request;
        beast::error_code ec{};
        http::async_read(streamReader.stream(), buffer, request, yield[ec]);
        if (ec)
        {
            std::cout << ec.message() << "\n";
            return;
        }
        handleRequest(ioc, std::move(request), std::move(streamReader), router,
                      yield);
    }
    static inline void handleRequest(auto& ioc, auto&& request,
                                     auto&& streamReader, auto& router,
                                     net::yield_context yield)
    {
        auto forwarder = router.getForwarder(request.target());
        if (forwarder)
        {
            auto&& responseHander = [&ioc, yield,
                                     streamReader = std::move(streamReader)](
                                        auto&& response) mutable {
                sendResponse(std::move(streamReader), std::move(response),
                             yield);
            };
            (*forwarder)(std::move(request), ioc, std::move(responseHander));
        }
    }
    static inline void sendResponse(auto&& streamReader, auto&& response,
                                    net::yield_context yield)
    {
        std::visit(
            [&streamReader, &yield](auto&& resp) {
            beast::error_code ec{};
            http::async_write(streamReader.stream(), resp, yield[ec]);
            if (ec)
            {
                std::cout << ec.message() << "\n";
            }
            streamReader.close(ec);
        },
            response);
    }
};

template <typename Router>
struct AsyncSslServer : AsyncServer<SslStreamMaker, Router>
{
    using Base = AsyncServer<SslStreamMaker, Router>;
    AsyncSslServer(Router& router, std::string_view port,
                   const char* serverCert, const char* serverPkey,
                   const char* trustStore) :
        Base(router, port, SslStreamMaker(serverCert, serverPkey, trustStore))
    {}
};
} // namespace bmcgw
