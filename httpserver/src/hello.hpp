#pragma once
#include "common/common_defs.hpp"
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
};

} // namespace redfish
