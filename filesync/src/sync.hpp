#pragma once
#include "auth_handler.hpp"
#include "parser_collections.hpp"
namespace bmcgw
{
class SyncHandler
{
    template <typename StreamReader>
    struct SyncSession
    {
        StreamReader streamReader;
        char data_[1024];
        static constexpr size_t max_length = sizeof(data_);
        std::unique_ptr<Parser> parser;
        ~SyncSession() {}
        void setParser(std::unique_ptr<Parser> p)
        {
            parser = std::move(p);
        }
        SyncSession(StreamReader&& streamReader) :
            streamReader(std::move(streamReader))
        {}
        void doRead(net::yield_context yield)
        {
            beast::error_code ec{};
            auto length = streamReader.stream().async_read_some(
                net::buffer(data_, max_length), yield[ec]);
            if (ec)
            {
                std::cout << ec.message() << "\n";
                return;
            }
            doWrite(length, yield);
        }
        void doWrite(size_t length, net::yield_context yield)
        {
            if (!parser->parse(std::string_view(data_, length)))
            {
                return;
            }
            doRead(yield);
        }
    };

  public:
    SyncHandler() {}
    void setIoContext(std::reference_wrapper<net::io_context> ioc) {}
    void handleRead(auto&& streamReader, net::yield_context yield)
    {
        auto session = std::make_shared<SyncSession<decltype(streamReader)>>(
            std::move(streamReader));
        auto parsers = std::make_unique<ParserCollections>();
        parsers->addParser("Filename", std::make_unique<FileParser>());
        parsers->addParser("Authentication", std::make_unique<AuthParser>());
        session->setParser(std::move(parsers));
        session->doRead(yield);
    }
};
} // namespace bmcgw
