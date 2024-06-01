#include "common/command_line_parser.hpp"
#include "requester.hpp"

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Function to generate a CA certificate
std::pair<EVP_PKEY*, X509*> generateCACert(int serial,
                                           const nlohmann::json& caConf)
{
    EVP_PKEY* key = EVP_PKEY_new();
    X509* cert = X509_new();

    // Generate CA key
    RSA* rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
    EVP_PKEY_assign_RSA(key, rsa);

    // Set CA cert properties
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 31536000L);
    X509_set_pubkey(cert, key);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(
        name, "C", MBSTRING_ASC,
        (const unsigned char*)caConf["C"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "ST", MBSTRING_ASC,
        (const unsigned char*)caConf["ST"].get<std::string>().data(), -1, -1,
        0);
    X509_NAME_add_entry_by_txt(
        name, "L", MBSTRING_ASC,
        (const unsigned char*)caConf["L"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "O", MBSTRING_ASC,
        (const unsigned char*)caConf["O"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "OU", MBSTRING_ASC,
        (const unsigned char*)caConf["OU"].get<std::string>().data(), -1, -1,
        0);
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_ASC,
        (const unsigned char*)caConf["CN"].get<std::string>().data(), -1, -1,
        0);
    X509_set_issuer_name(cert, name);
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, NULL, cert, NULL, NULL, 0);
    X509_add_ext(cert,
                 X509V3_EXT_nconf_nid(
                     nullptr, &ctx, NID_basic_constraints,
                     caConf["NID_basic_constraints"].get<std::string>().data()),
                 -1);
    X509_add_ext(
        cert,
        X509V3_EXT_nconf_nid(nullptr, &ctx, NID_key_usage,
                             caConf["NID_key_usage"].get<std::string>().data()),
        -1);
    X509_add_ext(
        cert,
        X509V3_EXT_nconf_nid(
            nullptr, &ctx, NID_subject_key_identifier,
            caConf["NID_subject_key_identifier"].get<std::string>().data()),
        -1);
    X509_add_ext(
        cert,
        X509V3_EXT_nconf_nid(
            nullptr, &ctx, NID_authority_key_identifier,
            caConf["NID_authority_key_identifier"].get<std::string>().data()),
        -1);

    X509_sign(cert, key, EVP_sha256());

    return std::make_pair(key, cert);
}

// Function to generate a certificate
std::pair<EVP_PKEY*, X509*>
    generateCert(std::string commonName,
                 std::vector<X509_EXTENSION*> extensions, EVP_PKEY* caKey,
                 X509* caCert, int serial, const nlohmann::json& conf,
                 X509_REQ* csr = nullptr)
{
    EVP_PKEY* key = EVP_PKEY_new();
    X509* cert = X509_new();

    // Set cert properties
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 31536000L);

    if (csr == nullptr)
    {
        RSA* rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
        EVP_PKEY_assign_RSA(key, rsa);
        X509_set_pubkey(cert, key);
    }
    else
    {
        EVP_PKEY* reqPubKey = X509_REQ_get_pubkey(csr);
        EVP_PKEY_copy_parameters(reqPubKey, key);
        X509_set_pubkey(cert, reqPubKey);
        EVP_PKEY_free(reqPubKey);
    }

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(
        name, "C", MBSTRING_ASC,
        (const unsigned char*)conf["C"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "ST", MBSTRING_ASC,
        (const unsigned char*)conf["ST"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "L", MBSTRING_ASC,
        (const unsigned char*)conf["L"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "O", MBSTRING_ASC,
        (const unsigned char*)conf["O"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(
        name, "OU", MBSTRING_ASC,
        (const unsigned char*)conf["OU"].get<std::string>().data(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (const unsigned char*)commonName.c_str(), -1, -1,
                               0);
    X509_set_issuer_name(cert, X509_get_subject_name(caCert));

    for (auto ext : extensions)
    {
        X509_add_ext(cert, ext, -1);
    }

    X509_sign(cert, caKey, EVP_sha256());

    return std::make_pair(key, cert);
}
std::string getCertHash(auto cert)
{
    unsigned long hash = X509_NAME_hash(X509_get_subject_name(cert));
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}
struct CFile
{
    FILE* file;
    CFile(const char* filename, const char* mode) : file(fopen(filename, mode))
    {}
    ~CFile()
    {
        if (file)
        {
            fclose(file);
        }
    }
    operator FILE*() const
    {
        return file;
    }
};
std::string readCert(X509* cert)
{
    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, cert);
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    std::string certStr(mem->data, mem->length);
    BIO_free(bio);
    return certStr;
}
int main(int argc, const char* argv[])
{
    auto [conf, machine, p, user,
          passwd] = getArgs(parseCommandline(argc, argv), "-conf", "-machine",
                            "-p", "-user", "-password");
    int serial = 1000;
    std::ifstream file({conf.data(), conf.length()});
    nlohmann::json caConf = nlohmann::json::parse(file);

    std::pair<EVP_PKEY*, X509*> caCertPair = generateCACert(serial,
                                                            caConf["CA"]);
    EVP_PKEY* caKey = caCertPair.first;
    X509* caCert = caCertPair.second;

    serial++;

    std::vector<X509_EXTENSION*> clientExtensions;
    // Add client extensions

    std::pair<EVP_PKEY*, X509*> clientCertPair =
        generateCert(caConf["Entity"]["CN"], clientExtensions, caKey, caCert,
                     serial, caConf["Entity"]);
    EVP_PKEY* clientKey = clientCertPair.first;
    X509* clientCert = clientCertPair.second;

    serial++;

    std::vector<X509_EXTENSION*> serverExtensions;
    // Add server extensions

    std::pair<EVP_PKEY*, X509*> serverCertPair =
        generateCert("reactor_server", serverExtensions, caKey, caCert, serial,
                     caConf["Entity"]);
    EVP_PKEY* serverKey = serverCertPair.first;
    X509* serverCert = serverCertPair.second;

    // Write certificates to files
    CFile caKeyFile("ca-cer.pem", "w");
    PEM_write_PrivateKey(caKeyFile, caKey, nullptr, nullptr, 0, nullptr,
                         nullptr);
    PEM_write_X509(caKeyFile, caCert);
    CFile ckFile("client-key.pem", "w");
    PEM_write_PrivateKey(ckFile, clientKey, nullptr, nullptr, 0, nullptr,
                         nullptr);

    CFile clientCertFile("client-cert.pem", "w");
    PEM_write_PrivateKey(clientCertFile, clientKey, nullptr, nullptr, 0,
                         nullptr, nullptr);
    PEM_write_X509(clientCertFile, clientCert);

    CFile serverKeyFile("server-key.pem", "w");
    PEM_write_PrivateKey(serverKeyFile, serverKey, nullptr, nullptr, 0, nullptr,
                         nullptr);

    CFile serverCertFile("server-cert.pem", "w");
    PEM_write_PrivateKey(serverCertFile, serverKey, nullptr, nullptr, 0,
                         nullptr, nullptr);
    PEM_write_X509(serverCertFile, serverCert);

    auto caCertStr = readCert(caCertPair.second);
    std::string cahash = getCertHash(caCertPair.second);
    std::ofstream caCertFile(std::format("{}.0.cacert", cahash));
    caCertFile << caCertStr;
    // Clean up
    EVP_PKEY_free(caKey);
    X509_free(caCert);
    EVP_PKEY_free(clientKey);
    X509_free(clientCert);
    EVP_PKEY_free(serverKey);
    X509_free(serverCert);
    net::io_context ioc;
    nlohmann::json j;

    std::string caCertPath =
        "/redfish/v1/Managers/bmc/Truststore/Certificates/ca-cert.pem";
    std::string replaceCertPath = "/redfish/v1/CertificateService/Actions/";
    replaceCertPath += "CertificateService.ReplaceCertificate";
    REACTOR_LOG_DEBUG("CA Cert: {}", caCertStr);
    j["CertificateString"] = caCertStr;
    j["CertificateType"] = "PEM";
    // j["CertificateUri"]["@odata.id"] = caCertPath + "/2";
    REACTOR_LOG_DEBUG("Json: {}", j.dump());
    bmcgw::Requester requester(ioc);
    requester.withMachine(machine).withPort(p).withName("test");
    requester.withCredentials(user, passwd);
    requester.post(caCertPath, std::move(j), [](auto& v) {
        REACTOR_LOG_DEBUG("Response: {}", v.response().data().dump());
    });
    ioc.run();
    return 0;
}
