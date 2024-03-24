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
    {
        router_.setIoContext(std::ref(ioc_));
    }
    void start()
    {
        acceptor_.listen(net::socket_base::max_listen_connections);
        waitForAsyncConnection();
        ioc_.run();
    }
    void waitForAsyncConnection()
    {
        auto asyncWork = [this](auto streamReader, net::yield_context yield) {
            handleRead(std::move(streamReader), router_, yield);
        };
        streamMaker.acceptAsyncConnection(ioc_, acceptor_,
                                          std::move(asyncWork));
    }

    static inline void handleRead(auto&& streamReader, auto& router,
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
        handleRequest(std::move(request), std::move(streamReader), router,
                      yield);
    }
    static inline void handleRequest(auto&& request, auto&& streamReader,
                                     auto& router, net::yield_context yield)
    {
        auto forwarder = router.getForwarder(request.target());
        if (forwarder)
        {
            auto responseHander = [streamReader](auto&& response) mutable {
                net::spawn(streamReader.stream().get_executor(),
                           [streamReader, response = std::move(response)](
                               net::yield_context yield) {
                    sendResponse(streamReader, response, yield);
                });
            };
            (*forwarder)(std::move(request), std::move(responseHander));
        }
    }
    static inline void sendResponse(auto streamReader, auto& response,
                                    net::yield_context yield)
    {
        beast::error_code ec{};
        auto resp = std::get_if<StringbodyResponse>(&response);
        assert(resp);
        http::async_write(streamReader.stream(), *resp, yield[ec]);

        streamReader.stream().async_shutdown(yield[ec]);
        if (ec)
        {
            std::cout << ec.message() << "\n";
        }
        streamReader.stream().lowest_layer().close();
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
