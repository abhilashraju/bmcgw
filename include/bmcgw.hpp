#pragma once
#include "common_defs.hpp"
#include "http_errors.hpp"
#include "requester.hpp"
#include "utilities.hpp"

#include <filesystem>
#include <functional>
#include <ranges>
namespace http = chai::http;
namespace beast = chai::beast;
namespace bmcgw
{
struct Aggregator
{
    nlohmann::json routeConfig = R"(
    {
        "Exposes": [
            {
                "Hostname": "xxxx",
                "Port": "443",
                "Name": "sat0",
                "Type": "SatelliteController",
                "AuthType": "Token"
            }
        ],
        "Master": {
            "Hostname": "xxxx",
            "Port": "443",
            "Name": "master",
            "Type": "MasterController",
            "AuthType": "Token"
        }
    }
    )"_json;
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
    net::io_context ioc;
    std::vector<Requester> requesters;
    std::unordered_map<std::string, RequestBlock> blocks;
    Requester masterController;
    Requester makeRequester(nlohmann::json& v)
    {
        std::string name = v["Name"];
        std::string type = v["Type"];
        std::string hostname = v["Hostname"];
        std::string port = v["Port"];
        std::string authType = v["AuthType"];
        Requester r(ioc);
        r.withMachine(hostname).withPort(port).withName(std::move(name));
        if (authType != "None")
        {
            r.withCredentials("xxx", "xxx");
        }
        return r;
    }

    Aggregator() : masterController(makeRequester(routeConfig["Master"]))
    {
        auto array = routeConfig["Exposes"].get<std::vector<nlohmann::json>>();
        for (auto& v : array)
        {
            requesters.emplace_back(makeRequester(v));
        }
    }
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
                splitId.back() = name + "_" + splitId.back();
                member["@odata.id"] = join(splitId | std::views::drop(1), '/');
                outer.emplace_back(member);
            }
        }
    }
    auto mergeWithAggregated(StringbodyRequest req, nlohmann::json& body)
    {
        aggregate(std::move(req), [&body](auto r) {
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
                CLIENT_LOG_INFO("Response: {}", j.dump(4));
            }
        });
    }
    auto findItemRequester(const std::string& item)
    {
        return std::ranges::find_if(requesters, [&item](auto& v) {
            return item.starts_with(v.name + "_");
        });
    }
    auto forward(Requester::Client::Session::Request req, auto&& cont)
    {
        auto iter = findItemRequester(split(req.target(), '/').back());
        if (iter != std::end(requesters))
        {
            iter->forward(iter->applyToken(std::move(req)), std::move(cont));
            return;
        }
        masterController.forward(std::move(req), cont);
    }
    Aggregator* getForwarder(const std::string& path)
    {
        return this;
    }

    bool isAggregatedItem(const std::string& item)
    {
        return findItemRequester(item) != std::end(requesters);
    }
    bool isAggregatedResource(const std::string& path)
    {
        const auto& segments = split(path, '/');
        if (isAggregatedItem(segments.back()))
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
    auto operator()(VariantRequest& request) -> VariantResponse
    {
        auto stringRequest = std::get_if<StringbodyRequest>(&request);
        if (!stringRequest)
        {
            throw std::runtime_error("Request type miss match");
        }
        Requester::Response results;
        forward(*stringRequest, [&results, stringRequest, this](auto v) {
            results = std::move(v.response());
            if (isAggregatedResource(stringRequest->target()))
            {
                nlohmann::json body = nlohmann::json::parse(results.body());
                mergeWithAggregated(*stringRequest, body);
                results.body() = body.dump(2);
            }
            CLIENT_LOG_INFO("Response: {}", results.body());
        });
        results.prepare_payload();
        return results;
    }
};

} // namespace bmcgw
