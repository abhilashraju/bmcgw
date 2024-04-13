#pragma once
#include "client/http/web_client.hpp"
using namespace reactor;

namespace bmcgw
{
struct Requester
{
    using Client = WebClient<CoroSslStream, http::string_body>;
    using Request = Client::Request;
    using Response = Client::Response;
    std::string name;
    std::string token;
    std::string machine;
    std::string port{"443"};
    ssl::context ctx{ssl::context::tlsv12_client};
    net::io_context& ioc;
    std::string usename;
    std::string password_;
    Requester(net::io_context& ioc) : ioc(ioc) {}
    Requester& withCredentials(std::string_view username, std::string_view pass)
    {
        usename = std::string(username.data(), username.size());
        password_ = std::string(pass.data(), pass.size());
        token = std::string();
        return *this;
    }
    Requester& withMachine(std::string machine)
    {
        this->machine = std::move(machine);
        return *this;
    }
    Requester& withName(std::string nm)
    {
        this->name = std::move(nm);
        return *this;
    }
    Requester& withPort(std::string port)
    {
        this->port = std::move(port);
        return *this;
    }

    std::string user()
    {
        return usename;
    }
    std::string password()
    {
        return password_;
    }
    ssl::context& getContext()
    {
        ctx.set_verify_mode(ssl::verify_none);
        return ctx;
    }
    void getToken()
    {
        getToken([]() {});
    }
    template <typename Contiuation>
    void getToken(Contiuation cont)
    {
        if (token.empty())
        {
            auto mono =
                Client::builder()
                    .withSession(ioc.get_executor(), getContext())
                    .withEndpoint(std::format(
                        "https://{}:{}/redfish/v1/SessionService/Sessions",
                        machine, port))
                    .create()
                    .post()
                    .withBody(nlohmann::json{{"UserName", user()},
                                             {"Password", password()}})
                    .toMono();
            mono->asJson([this, mono, cont = std::move(cont)](auto& v) {
                if (v.isError())
                {
                    REACTOR_LOG_ERROR("Error: {}", v.error().message());
                    return;
                }
                REACTOR_LOG_ERROR("Error: {}", v.response().data().dump(4));
                token = v.response().getHeaders()["X-Auth-Token"];
                cont();
            });
            return;
        }
        cont();
    }
    Client::Session::Request&& applyToken(Client::Session::Request&& req)
    {
        req.set("X-Auth-Token", token);
        return std::move(req);
    }
    template <typename Contiuation>
    void forward(Client::Session::Request req, Contiuation&& cont)
    {
        // CLIENT_LOG_INFO("Forwarding to: {} with token {}", machine,
        //                 req["X-Auth-Token"]);
        req.set(http::field::host, machine);
        auto mono =
            Client::builder()
                .withSession(ioc.get_executor(), getContext())
                .withEndpoint(std::format("https://{}:{}", machine, port))
                .create()
                .withRequest(req)
                .toMono();
        mono->subscribe([mono, cont = std::move(cont)](auto v) mutable {
            cont(std::move(v));
        });
    }
    template <typename Contiuation>
    void aggregate(Client::Session::Request req, Contiuation cont)
    {
        getToken([this, cont = std::move(cont), req]() {
            std::string ep = std::format("https://{}:{}", machine, port);
            auto mono = Client::builder()
                            .withSession(ioc.get_executor(), getContext())
                            .withEndpoint(ep)
                            .withTarget(req.target())
                            .create()
                            .withMethod(req.method())
                            .withHeaders(req.base())
                            .withBody(req.body())
                            .withHeader({"X-Auth-Token", token})
                            .toMono();
            mono->asJson([cont = std::move(cont), mono](auto v) { cont(v); });
        });
    }
};
} // namespace bmcgw
