#pragma once
#include "common/common_defs.hpp"
#include "logger/logger.hpp"
#include "server/http/http_errors.hpp"
#include "server/http/http_target_parser.hpp"

#include <filesystem>
#include <functional>
namespace http = reactor::http;
namespace beast = reactor::beast;
namespace redfish
{
class Hello
{
  public:
    Hello(auto& router)
    {
        router.add_get_handler("/hello/{name}",
                               std::bind_front(&Hello::hello, this));
        router.add_get_handler("/hello/{name}/{id}",
                               std::bind_front(&Hello::helloId, this));
        router.add_post_handler("/events",
                                std::bind_front(&Hello::events, this));
    }

    reactor::VariantResponse hello(const reactor::StringbodyRequest& req,
                                   const reactor::http_function& httpfunc,
                                   reactor::net::yield_context yield)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);

        res.body() = "Hello, " + httpfunc["name"];
        res.prepare_payload();
        return res;
    }
    reactor::VariantResponse helloId(const reactor::StringbodyRequest& req,
                                     const reactor::http_function& httpfunc,
                                     reactor::net::yield_context yield)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);

        res.body() = "Hello, " + httpfunc["name"] + " with id " +
                     httpfunc["id"];
        res.prepare_payload();
        return res;
    }
    reactor::VariantResponse events(const reactor::StringbodyRequest& req,
                                    const reactor::http_function& httpfunc,
                                    reactor::net::yield_context yield)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.body() = "Received event: " + req.body();
        REACTOR_LOG_DEBUG("Received event: {}", req.body());
        res.prepare_payload();
        return res;
    }
};

} // namespace redfish
