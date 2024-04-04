#pragma once
#include "parser.hpp"
#include "syncdb.hpp"

#include <filesystem>
#include <fstream>
namespace reactor
{
struct FileParser : Parser
{
    std::ofstream file;
    std::filesystem::path path;
    std::string root{"root"};
    FileParser() {}
    bool makeDirectories(std::filesystem::path dirs)
    {
        if (!std::filesystem::exists(dirs))
        {
            bool ret = std::filesystem::create_directories(dirs);
            SyncDb::globalSyncDb().updateTime(dirs);
            return ret;
        }
        return true;
    }
    bool openFile(std::string_view header)
    {
        auto pos = header.find("Filename: ");
        if (pos == std::string_view::npos)
        {
            return false;
        }
        auto filename =
            header.substr(pos + std::string_view("filename: ").size());
        path = std::filesystem::path{root + std::string(filename.data())};
        std::filesystem::path dirs = path.parent_path();
        if (makeDirectories(dirs))
        {
            file.open(path, std::ios::binary);
            return file.is_open();
        }
        return false;
    }
    bool handleHeader(std::string_view header) override
    {
        return openFile(header);
    }
    bool handleBody(std::string_view data) override
    {
        if (file.is_open())
        {
            file.write(data.data(), data.size());
            SyncDb::globalSyncDb().updateTime(path);
            return true;
        }
        return false;
    }
};
} // namespace reactor
