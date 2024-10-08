#pragma once
#include "forwarder.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <unordered_map>
namespace reverseproxy
{
using FORWARD_MAKER = std::unique_ptr<AbstractForwarder> (*)(
    const std::string&, const std::string&);
using MAKER_FACTORY = std::map<std::string, FORWARD_MAKER>;

inline std::unique_ptr<AbstractForwarder>
    generic_forwarder(const std::string& ip, const std::string& port)
{
    return std::make_unique<HttpForwarder>(ip, port);
}
#ifdef SSL_ON
inline std::unique_ptr<AbstractForwarder>
    secure_generic_forwarder(const std::string& ip, const std::string& port)
{
    return std::make_unique<SecureHttpForwarder>(ip, port);
}
#endif
static MAKER_FACTORY& forwardMaker()
{
    static MAKER_FACTORY gmakerFactory;
    if (gmakerFactory.empty())
    {
        gmakerFactory["generic_forwarder"] = generic_forwarder;

#ifdef SSL_ON
        gmakerFactory["secure_generic_forwarder"] = secure_generic_forwarder;
#endif
    }
    return gmakerFactory;
}
struct RequestForwarder
{
    using Map =
        std::unordered_map<std::string, std::unique_ptr<AbstractForwarder>>;
    Map table;
    std::unique_ptr<AbstractForwarder> defaultForwarder;
    std::optional<std::reference_wrapper<net::io_context>> ioc;
    RequestForwarder(const std::string& config)
    {
        std::ifstream i(config);
        nlohmann::json routdef;
        i >> routdef;
        for (auto& r : routdef)
        {
#ifdef SSL_ON
            auto forwarderType = r["type"].is_null()
                                     ? std::string("secure_generic_forwarder")
                                     : r["type"].get<std::string>();
#else
            auto forwarderType = r["type"].is_null()
                                     ? std::string("generic_forwarder")
                                     : r["type"].get<std::string>();
#endif
            auto makeForwader = forwardMaker()[forwarderType];
            if (makeForwader)
            {
                std::cout << r["path"].get<std::string>();
                addRoute(r["path"].get<std::string>(),
                         std::move(makeForwader(r["ip"].get<std::string>(),
                                                r["port"].get<std::string>())));
                if (!r["default"].is_null() && r["default"].get<bool>())
                {
                    defaultForwarder.reset(
                        makeForwader(r["ip"].get<std::string>(),
                                     r["port"].get<std::string>())
                            .release());
                }
            }
        }
    }
    RequestForwarder(const RequestForwarder&) = delete;
    RequestForwarder(RequestForwarder&&) = delete;
    void addRoute(const std::string& path,
                  std::unique_ptr<AbstractForwarder> route)
    {
        table[path] = std::move(route);
    }
    void setIoContext(std::reference_wrapper<net::io_context> ioc)
    {
        this->ioc = ioc;
        for (auto& [k, v] : table)
        {
            v->setIoContext(ioc);
        }
        defaultForwarder->setIoContext(ioc);
    }
    AbstractForwarder* wildCardMatch(const std::string& path) const
    {
        std::string_view pathview(path);
        auto wildcarkeys = table | std::views::keys |
                           std::views::filter([](auto& a) {
            return std::string_view(a).ends_with("*");
        }) | std::views::transform([](auto& a) {
            return std::string_view(a).substr(0, a.length() - 1);
        });
        auto iter = std::ranges::find_if(
            wildcarkeys, [&](auto a) { return pathview.starts_with(a); });
        if (iter != wildcarkeys.end())
        {
            return table.at((*iter).data()).get();
        }
        return nullptr;
    }
    AbstractForwarder* getForwarder(const std::string& path) const
    {
        if (auto matched = wildCardMatch(path); matched != nullptr)
        {
            std::cout << "found matching wildcard for " << path;
            return matched;
        }
        if (const auto& iter = table.find(path); iter != end(table))
        {
            return iter->second.get();
        }
        return defaultForwarder.get();
    }
};
} // namespace reverseproxy
