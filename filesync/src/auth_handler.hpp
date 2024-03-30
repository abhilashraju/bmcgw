#pragma once
#include "parser.hpp"

#include <nlohmann/json.hpp>
namespace bmcgw
{
struct AuthParser : Parser
{
    std::string username;
    std::string password;
    AuthParser() {}
    bool handleHeader(std::string_view header) override
    {
        auto pos = header.find(": ");
        if (pos == std::string_view::npos)
        {
            return false; // no type found
        }
        return true;
    }
    bool handleBody(std::string_view data) override
    {
        nlohmann::json j = nlohmann::json::parse(data);
        std::cout << j.dump(4) << "\n";
        return true;
    }
};
} // namespace bmcgw
