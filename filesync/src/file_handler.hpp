#pragma once
#include <fstream>
namespace bmcgw
{

class FileHandler
{
    struct FileParser
    {
        std::ofstream file;
        std::string currentdata;
        bool headerFound{false};
        std::string root{"root"};
        FileParser() {}
        bool makeDirectories(std::filesystem::path dirs)
        {
            if (!std::filesystem::exists(dirs))
            {
                return std::filesystem::create_directories(dirs);
            }
            return true;
        }
        bool openFile(std::string_view header)
        {
            auto pos = header.find("filename: ");
            if (pos == std::string_view::npos)
            {
                return false;
            }
            auto filename =
                header.substr(pos + std::string_view("filename: ").size());
            std::filesystem::path path{root + std::string(filename.data())};
            std::filesystem::path dirs = path.parent_path();
            if (makeDirectories(dirs))
            {
                file.open(path, std::ios::binary);
                return file.is_open();
            }
            return false;
        }
        bool parse(std::string_view data)
        {
            try
            {
                if (!headerFound)
                {
                    auto pos = data.find("\r\n\r\n");
                    if (pos != std::string_view::npos)
                    {
                        headerFound = true;
                        auto subdata = data.substr(0, pos);
                        auto header = currentdata + std::string(subdata.data(),
                                                                subdata.size());

                        if (!openFile(header))
                        {
                            return false;
                        }
                        currentdata = data.substr(
                            pos + std::string_view("\r\n\r\n").size());
                        file.write(currentdata.data(), currentdata.size());
                        currentdata.clear();
                        return true;
                    }
                    currentdata.append(data);
                    return true;
                }
                file.write(data.data(), data.size());
            }
            catch (const std::exception& e)
            {
                std::cout << e.what() << "\n";
                return false;
            }
            return true;
        }
    };
    template <typename StreamReader>
    struct FileSession
    {
        StreamReader streamReader;
        char data_[1024];
        static constexpr size_t max_length = sizeof(data_);
        FileSession(StreamReader&& streamReader) :
            streamReader(std::move(streamReader))
        {}
        void doRead(FileParser& parser, net::yield_context yield)
        {
            beast::error_code ec{};
            auto length = streamReader.stream().async_read_some(
                net::buffer(data_, max_length), yield[ec]);
            if (ec)
            {
                std::cout << ec.message() << "\n";
                return;
            }
            doWrite(parser, yield, length);
        }
        void doWrite(FileParser& parser, net::yield_context yield,
                     size_t length)
        {
            if (!parser.parse(std::string_view(data_, length)))
            {
                return;
            }
            doRead(parser, yield);
        }
    };

  public:
    FileHandler() {}
    void setIoContext(std::reference_wrapper<net::io_context> ioc) {}
    void handleRead(auto&& streamReader, net::yield_context yield)
    {
        auto session = std::make_shared<FileSession<decltype(streamReader)>>(
            std::move(streamReader));
        FileParser parser;
        session->doRead(parser, yield);
    }
};
} // namespace bmcgw
