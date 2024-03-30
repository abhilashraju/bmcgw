#pragma once
namespace bmcgw
{
struct ParserCollections : Parser
{
    std::map<std::string, std::unique_ptr<Parser>> parsers;
    Parser* currentParser{nullptr};
    ~ParserCollections() {}
    void addParser(const std::string& type, std::unique_ptr<Parser> parser)
    {
        parsers[type] = std::move(parser);
    }
    bool handleHeader(std::string_view header) override
    {
        auto pos = header.find(": ");
        if (pos == std::string_view::npos)
        {
            return false; // no type found
        }
        auto type = header.substr(0, pos);
        auto iter = parsers.find(std::string{type.data(), type.size()});
        if (iter != parsers.end())
        {
            currentParser = iter->second.get();
            return currentParser->handleHeader(header);
        }
        return false;
    }
    bool handleBody(std::string_view data) override
    {
        return currentParser->handleBody(data);
    }
};

} // namespace bmcgw
