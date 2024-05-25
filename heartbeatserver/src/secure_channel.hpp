#include "client/http/web_client.hpp"
#include "server/http/http_server.hpp"
#include "ssl/aes.hpp"
using namespace reactor;
using Targets = std::vector<std::pair<std::string, std::string>>;
struct SecureChannel
{
    struct Client
    {
        using WebClient = WebClient<CoroSslStream, http::string_body>;
        Client(net::io_context& ioc, const std::string& host,
               const std::string& port, const std::string& certPath) :
            ioc(ioc), certPath(certPath), host(host), port(port)

        {}
        ssl::context& getContext()
        {
            ctx.set_verify_mode(ssl::verify_peer);
            ctx.set_verify_callback(ensuressl::tlsVerifyCallback);
            return ctx;
        }
        void getKey(auto&& cont)
        {
            auto mono = WebClient::builder()
                            .withSession(ioc.get_executor(), getContext())
                            .withEndpoint(std::format("https://{}:{}/{}", host,
                                                      port, "key"))
                            .create()
                            .withMethod(http::verb::get)
                            .toMono();
            mono->asJson([this, mono, cont = std::move(cont)](auto& v) {
                if (v.isError())
                {
                    REACTOR_LOG_ERROR("Error: {}", v.error().message());
                    return;
                }
                REACTOR_LOG_ERROR("Error: {}", v.response().data().dump(4));
                cont(v.response().data());
            });
            return;
        }
        net::io_context& ioc;
        std::string certPath;
        std::string host;
        std::string port;
        ssl::context ctx{
            ensuressl::loadCertificate(boost::asio::ssl::context::tls_client,
                                       {certPath.data(), certPath.size()})};
    };

    struct Server
    {
        HttpsServer server;
        std::vector<tcp::endpoint> endpoints;
        std::string key;
        Server(net::io_context& ioc, std::string_view port,
               std::string_view certPath) : server(ioc, port, certPath)
        {
            server.router().add_get_handler(
                "/key", [this](const reactor::StringbodyRequest& req,
                               const reactor::http_function& httpfunc,
                               reactor::net::yield_context yield) {
                return getKey(req, httpfunc, yield);
            });
        }
        reactor::VariantResponse getKey(const reactor::StringbodyRequest& req,
                                        const reactor::http_function& httpfunc,
                                        reactor::net::yield_context yield)
        {
            reactor::http::response<reactor::http::string_body> res{
                http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            if (key.empty())
            {
                key = AESCipher::generateKey();
            }
            nlohmann::json j;
            j["key"] = key;
            res.body() = j.dump();
            res.prepare_payload();
            return res;
        }
        void listen()
        {
            server.listen();
        }
        std::string getKey()
        {
            return key;
        }
        std::string decrypt(std::string_view ciphertext)
        {
            return AESCipher(getKey().data()).decrypt(ciphertext);
        }
        std::string encrypt(std::string_view plainttext)
        {
            return AESCipher(getKey().data()).encrypt(plainttext);
        }
    };
};
