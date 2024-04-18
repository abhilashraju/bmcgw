#pragma once
#include "common/beast_defs.hpp"
#include "common/common_defs.hpp"
namespace reverseproxy
{
using namespace reactor;

struct AbstractForwarder
{
    using ResponseHandler = std::function<void(VariantResponse&&)>;
    std::optional<std::reference_wrapper<net::io_context>> ioc;
    void setIoContext(std::reference_wrapper<net::io_context> ioc)
    {
        this->ioc = ioc;
    }
    virtual void operator()(VariantRequest&&, ResponseHandler&&) const = 0;
    virtual ~AbstractForwarder(){};
};
template <typename Requester>
struct CommonForwarderImpl : AbstractForwarder
{
    std::string target;
    std::string port;

    ~CommonForwarderImpl() {}
    CommonForwarderImpl(const std::string& t, const std::string& p) :
        target(t), port(p)
    {}

    void operator()(VariantRequest&& requestVar,
                    ResponseHandler&& cont) const override
    {
        StringbodyRequest* request =
            std::get_if<StringbodyRequest>(&requestVar);
        if (request == nullptr)
        {
            cont(http::response<http::string_body>(
                http::status::internal_server_error, 11));
            return;
        }
        net::spawn(ioc->get().get_executor(),
                   [this, req = std::move(*request),
                    cont = std::move(cont)](net::yield_context yield) mutable {
            Requester requester;
            requester.forward(ioc->get(), target, port, std::move(req),
                              std::move(cont), yield);
        });
        // Receive the response from the target server
    }
};

struct Forwarder
{
    ssl::context sslctx{ssl::context::tlsv12_client};
    using Continuation = AbstractForwarder::ResponseHandler;
    Continuation cont;
    void forward(net::io_context& ioContext, std::string_view target,
                 std::string_view port, StringbodyRequest&& req,
                 Continuation&& cTok, net::yield_context yield)
    {
        this->cont = std::move(cTok);
        ssl::stream<tcp::socket> forwardingSocket(ioContext, sslctx);
        tcp::resolver resolver(ioContext);
        beast::error_code ec{};
        auto results = resolver.async_resolve(target.data(), port.data(),
                                              yield[ec]);
        if (checkFail(ec))
        {
            return;
        }
        net::async_connect(forwardingSocket.next_layer(), results, yield[ec]);
        if (checkFail(ec))
        {
            return;
        }
        forwardingSocket.async_handshake(ssl::stream_base::client, yield[ec]);
        if (checkFail(ec))
        {
            return;
        }
        http::async_write(forwardingSocket, req, yield[ec]);
        if (checkFail(ec))
        {
            return;
        }
        beast::flat_buffer buffer;
        StringbodyResponse res;
        http::async_read(forwardingSocket, buffer, res, yield[ec]);

        if (!checkFail(ec))
        {
            cont(std::move(res));
        }
        forwardingSocket.async_shutdown(yield[ec]);
        forwardingSocket.next_layer().close();
    }
    bool checkFail(beast::error_code& ec) const
    {
        if (ec)
        {
            cont(http::response<http::string_body>(
                http::status::internal_server_error, 11));
            return true;
        }
        return false;
    };
    // VariantResponse readFromUrl(STREAM_TYPE& stream, beast::flat_buffer&
    // buffer,
    //                             http::response_parser<http::string_body>&
    //                             res0, auto& ec) const
    // {
    //     http::file_body::value_type body;
    //     std::string url = res0.get()["fileurl"];
    //     body.open(url.data(), chai::beast::file_mode::scan, ec);
    //     checkFail(ec);
    //     const auto size = body.size();
    //     http::response_parser<chai::http::file_body> parser{std::move(res0)};
    //     parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
    //     parser.get().body() = std::move(body);
    //     parser.get().content_length(size);
    //     parser.get().prepare_payload();
    //     return parser.release();
    // }
    // VariantResponse readAsFile(STREAM_TYPE& stream, beast::flat_buffer&
    // buffer,
    //                            http::response_parser<http::string_body>&
    //                            res0, auto& ec) const
    // {
    //     http::response_parser<http::file_body> res{std::move(res0)};
    //     res.body_limit((std::numeric_limits<std::uint64_t>::max)());
    //     http::file_body::value_type file;
    //     auto tempFileName = getLocalTime();
    //     file.open(tempFileName.c_str(), beast::file_mode::write, ec);
    //     checkFail(ec);
    //     res.get().body() = std::move(file);
    //     http::read(stream, buffer, res);
    //     res.get().body().seek(0, ec);
    //     res.get().prepare_payload();
    //     checkFail(ec);
    //     return res.release();
    // }
    // VariantResponse read(STREAM_TYPE& stream, beast::flat_buffer& buffer,
    //                      beast::error_code& ec) const

    // {
    //     // Start with an empty_body parser
    //     http::response_parser<http::string_body> res0;
    //     // Read just the header. Otherwise, the empty_body
    //     // would generate an error if body octets were received.
    //     // res0.skip(true);
    //     http::read_header(stream, buffer, res0, ec);
    //     // checkFail(ec);

    //     constexpr int Threshold = 8000;
    //     if (res0.content_length())
    //     {
    //         if (!res0.get()["fileurl"].empty())
    //         {
    //             return readFromUrl(stream, buffer, res0, ec);
    //         }
    //         if (res0.content_length().value() > Threshold)
    //         {
    //             return readAsFile(stream, buffer, res0, ec);
    //         }
    //         http::response_parser<http::dynamic_body> res{std::move(res0)};
    //         // Finish reading the message
    //         http::read(stream, buffer, res, ec);
    //         res.get().prepare_payload();
    //         checkFail(ec);
    //         return res.release();
    //     }
    //     return res0.release();
    // }
};
using HttpForwarder = CommonForwarderImpl<Forwarder>;
using SecureHttpForwarder = CommonForwarderImpl<Forwarder>;
} // namespace reverseproxy
