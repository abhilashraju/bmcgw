#pragma once
#include "common/common_defs.hpp"
// #include "http_errors.hpp"
#include "common/utilities.hpp"
#include "requester.hpp"

#include <filesystem>
#include <functional>
#include <ranges>

namespace bmcgw
{
struct Aggregator
{
    struct RequestBlock
    {
        size_t count{0};
        using ResultEntityType =
            ResponseEntity<nlohmann::json, Requester::Response>;
        using ResultType =
            std::pair<std::string, HttpExpected<ResultEntityType>>;
        std::vector<ResultType> results;
        std::function<void(std::vector<ResultType>)> onFinish;
        void finish()
        {
            onFinish(std::move(results));
        }
    };
    nlohmann::json routeConfig;
    std::optional<std::reference_wrapper<net::io_context>> ioc;
    std::vector<Requester> requesters;
    std::unordered_map<std::string, RequestBlock> blocks;
    std::optional<Requester> masterController;
    Requester makeRequester(nlohmann::json& v)
    {
        std::string name = v["Name"];
        std::string type = v["Type"];
        std::string hostname = v["Hostname"];
        std::string port = v["Port"];
        std::string authType = v["AuthType"];
        Requester r(ioc.value().get());
        r.withMachine(hostname).withPort(port).withName(std::move(name));
        if (authType != "None")
        {
            std::string username = v["authentication"]["Username"];
            std::string password = v["authentication"]["Password"];
            r.withCredentials(username, password);
        }
        return r;
    }
    void makeRequesters()
    {
        masterController.emplace(makeRequester(routeConfig["Master"]));
        auto array = routeConfig["Exposes"].get<std::vector<nlohmann::json>>();
        for (auto& v : array)
        {
            requesters.emplace_back(makeRequester(v));
        }
    }
    Aggregator(std::string_view conf) : routeConfig(nlohmann::json::parse(conf))
    {}
    auto aggregate(Requester::Client::Session::Request req, auto&& cont)
    {
        std::string target = req.target();
        blocks.emplace(target, RequestBlock{.count = requesters.size(),
                                            .onFinish = std::move(cont)});
        for (auto& r : requesters)
        {
            r.aggregate(req, [this, target, name = r.name](auto& v) {
                blocks[target].results.push_back(std::make_pair(name, v));
                if (--blocks[target].count == 0)
                {
                    blocks[target].finish();
                    blocks.erase(target);
                }
            });
        }
    }
    static void mergeMembers(auto& outer, nlohmann::json& member,
                             const std::string& name)
    {
        auto& innerMembers =
            member["Members"].get_ref<nlohmann::json::array_t&>();
        for (auto& member : innerMembers)
        {
            if (member != nullptr)
            {
                std::string id = member["@odata.id"];
                auto splitId = split(id, '/');
                splitId.back() = name + "_" + toString(splitId.back());
                member["@odata.id"] = join(splitId | std::views::drop(1), '/');
                outer.emplace_back(member);
            }
        }
    }
    auto mergeWithAggregated(StringbodyRequest req, StringbodyResponse&& res,
                             nlohmann::json&& body, auto&& cont)
    {
        aggregate(std::move(req), [body = std::move(body), res = std::move(res),
                                   cont = std::move(cont)](auto r) mutable {
            auto& members = body["Members"].get_ref<nlohmann::json::array_t&>();
            for (auto& v : r)
            {
                if (v.second.isError())
                {
                    CLIENT_LOG_ERROR("Error: {}", v.second.error().message());
                    continue;
                }
                auto j = v.second.response().data();
                mergeMembers(members, j, v.first);
                body["Members@odata.count"] = members.size();
            }
            res.body() = body.dump(2);
            CLIENT_LOG_INFO("Response: {}", body.dump(4));
            res.prepare_payload();
            cont(VariantResponse(res));
        });
    }
    auto findResourceRequester(std::string_view item)
    {
        return std::ranges::find_if(requesters, [&item](auto& v) {
            return item.starts_with(v.name + "_");
        });
    }
    auto&& stripSateliteInfo(StringbodyRequest&& req)
    {
        auto path = req.target();
        auto segments = split(path, '/');
        segments.back() = segments.back().substr(segments.back().find('_') + 1);
        req.target(join(segments | std::views::drop(1), '/'));
        return std::move(req);
    }
    auto forward(Requester::Client::Session::Request req, auto&& cont)
    {
        auto iter = findResourceRequester(split(req.target(), '/').back());
        if (iter != std::end(requesters))
        {
            iter->forward(iter->applyToken(stripSateliteInfo(std::move(req))),
                          std::move(cont));
            return;
        }
        masterController.value().forward(std::move(req), std::move(cont));
    }
    Aggregator* getForwarder(const std::string& path)
    {
        return this;
    }
    void setIoContext(std::reference_wrapper<net::io_context> ioc)
    {
        this->ioc = ioc;
        makeRequesters();
    }
    bool isAggregatedResource(std::string_view item)
    {
        return findResourceRequester(item) != std::end(requesters);
    }
    bool isAggregatedCollection(StringbodyRequest& req)
    {
        if (req.method() != http::verb::get)
        {
            return false;
        }
        auto path = req.target();
        const auto& segments = split(path, '/');
        if (isAggregatedResource(segments.back()))
        {
            return false;
        }
        static std::array<const char*, 3> aggregateResourcesPath = {
            "", "redfish", "v1"};
        auto iter = std::mismatch(std::begin(segments), std::end(segments),
                                  std::begin(aggregateResourcesPath),
                                  std::end(aggregateResourcesPath));
        if (iter.second != std::end(aggregateResourcesPath))
        {
            CLIENT_LOG_INFO("Not an aggregated resource");
            return false;
        }
        CLIENT_LOG_INFO("Aggregated resource");
        if (iter.first == std::end(segments))
        {
            CLIENT_LOG_INFO("Resource List Query");
            return true;
        }
        static std::array<const char*, 3> exclusionList = {"", "$metadata",
                                                           "JsonSchemas"};
        auto iter2 = std::find(std::begin(exclusionList),
                               std::end(exclusionList), *iter.first);
        if (iter2 != std::end(exclusionList))
        {
            CLIENT_LOG_INFO("Excluded resource");
            return false;
        }
        return true;
    }
    std::optional<nlohmann::json> parseJson(std::string_view str)
    {
        nlohmann::json j = nlohmann::json::parse(str, nullptr, false);
        if (j.is_discarded())
        {
            return std::nullopt;
        }
        return j;
    }
    void sendErrorResponse(http::status status, auto&& cont)
    {
        StringbodyResponse res{status, 11};
        cont(VariantResponse(std::move(res)));
    }
    auto operator()(VariantRequest&& request, auto&& cont)
    {
        auto stringRequest = std::get_if<StringbodyRequest>(&request);
        if (!stringRequest)
        {
            CLIENT_LOG_ERROR("Error: {}", "Type miss match");
            StringbodyResponse res{http::status::internal_server_error, 11};
            cont(VariantResponse(std::move(res)));
            return;
        }
        auto req = *stringRequest; // copy request
        forward(*stringRequest, [req = std::move(req), this,
                                 cont = std::move(cont)](auto v) mutable {
            Requester::Response resp;
            resp = std::move(v.response());
            if (isAggregatedCollection(req))
            {
                auto parsed = parseJson(resp.body());
                if (!parsed)
                {
                    CLIENT_LOG_ERROR("Error: {}", "Invalid JSON");
                    sendErrorResponse(http::status::internal_server_error,
                                      std::move(cont));
                    return;
                }
                if (parsed.value()["Members"] != nullptr)
                {
                    mergeWithAggregated(req, std::move(resp),
                                        std::move(parsed.value()),
                                        std::move(cont));
                    return;
                }
            }
            resp.prepare_payload();
            cont(VariantResponse(std::move(resp)));
        });
    }
};

} // namespace bmcgw
