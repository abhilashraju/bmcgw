### Http Server

This is an examples implementation of an Http Server using the [Reactor](https://github.com/abhilashraju/reactor) framework

A greeting server example

```
#include "server/http_server.hpp"
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
                                   const reactor::http_function& httpfunc)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);

        res.body() = "Hello, " + httpfunc["name"];
        res.prepare_payload();
        return res;
    }
    reactor::VariantResponse helloId(const reactor::StringbodyRequest& req,
                                     const reactor::http_function& httpfunc)
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

int main(int argc, const char* argv[])
{
    // Create a server endpoint
    auto [port, cert] = getArgs(parseCommandline(argc, argv), "-p", "-certdir");
    if (port.empty() || cert.empty())
    {
        REACTOR_LOG_ERROR("Invalid arguments");
        REACTOR_LOG_ERROR("eg: httpserver -p port -certdir dirpath");
        return 0;
    }
    try
    {
        reactor::getLogger().setLogLevel(LogLevel::ERROR);

        HttpsServer server(port, cert);
        redfish::Hello hello(server.router());
        server.start();
    }
    catch (const std::exception& e)
    {
        REACTOR_LOG_ERROR("Exception: {}", e.what());
    }

    return 0;
}

```