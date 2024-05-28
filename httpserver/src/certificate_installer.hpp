#pragma once
#include "common/common_defs.hpp"
#include "logger/logger.hpp"
#include "server/http/http_errors.hpp"
#include "server/http/http_target_parser.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
namespace http = reactor::http;
namespace beast = reactor::beast;
namespace redfish
{
class CertificateInstaller
{
  public:
    std::string root = "/tmp";
    using X509UniquePtr =
        std::unique_ptr<X509, decltype([](X509* ptr) { X509_free(ptr); })>;
    using BioPtr =
        std::unique_ptr<BIO, decltype([](BIO* ptr) { BIO_free(ptr); })>;
    CertificateInstaller(auto& router)
    {
        router.add_post_handler(
            "/redfish/v1/Managers/bmc/Truststore/Certificates/{name}",
            std::bind_front(&CertificateInstaller::install, this));
        router.add_post_handler(
            "/redfish/v1/SessionService/Sessions",
            std::bind_front(&CertificateInstaller::session, this));
    }
    std::string getCertHash(auto cert)
    {
        unsigned long hash = X509_NAME_hash(X509_get_subject_name(cert.get()));
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str();
    }
    X509UniquePtr loadCert(std::string_view cert_str)
    {
        BioPtr bio(BIO_new_mem_buf((void*)cert_str.data(), -1));
        X509UniquePtr cert(PEM_read_bio_X509(bio.get(), NULL, 0, NULL));

        if (!cert)
        {
            // Handle error
        }

        return cert;
    }
    reactor::VariantResponse install(const reactor::StringbodyRequest& req,
                                     const reactor::http_function& httpfunc,
                                     reactor::net::yield_context yield)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        try
        {
            nlohmann::json j = nlohmann::json::parse(req.body());
            std::string certData = j["CertificateString"];
            auto hash = getCertHash(loadCert(certData));

            std::filesystem::create_directories(
                std::format("{}/etc/ssl/certs/authority", root));
            std::string filename = std::format("{}/etc/ssl/certs/authority/{}",
                                               root, httpfunc["name"]);

            std::ofstream cert_file(filename);
            cert_file << certData;
            system(
                std::format(
                    "ln -s {}/etc/ssl/certs/authority/{} {}/etc/ssl/certs/authority/{}.0",
                    root, httpfunc["name"], root, hash)
                    .c_str());
            res.body() = std::format("Installing certificate for {} as {}",
                                     httpfunc["name"], hash);
        }
        catch (const std::exception& e)
        {
            res.result(http::status::bad_request);
            res.body() = e.what();
            REACTOR_LOG_ERROR("Error: {}", e.what());
        }

        res.prepare_payload();
        return res;
    }
    reactor::VariantResponse session(const reactor::StringbodyRequest& req,
                                     const reactor::http_function& httpfunc,
                                     reactor::net::yield_context yield)
    {
        reactor::http::response<reactor::http::string_body> res{
            http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set("X-Auth-Token", "JSESSIONID=1234");
        res.body() = R"({"Session created": "JSESSIONID=1234"})";
        res.prepare_payload();
        return res;
    }
};

} // namespace redfish
