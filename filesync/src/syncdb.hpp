#pragma once
#include <filesystem>
namespace reactor
{
struct SyncDb
{
    void addConfig(std::filesystem::path filePath)
    {
        auto iter = std::ranges::find_if(monitorConfigs, [&filePath](auto& v) {
            return v.filePath == filePath;
        });
        if (iter == std::end(monitorConfigs))
        {
            monitorConfigs.push_back(
                {std::filesystem::file_time_type{}, filePath});
        }

        if (std::filesystem::is_directory(filePath))
        {
            std::filesystem::directory_iterator dir(filePath);
            for (auto& file : dir)
            {
                addConfig(file.path());
            }
            return;
        }
    }
    void applyDirectoryChanges(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
        {
            std::filesystem::directory_iterator dir(path);
            for (auto& file : dir)
            {
                addConfig(file.path());
            }
        }
    }
    template <typename Handler>
    void checkChanges(Handler handler)
    {
        for (auto& config : monitorConfigs)
        {
            if (!std::filesystem::exists(config.filePath))
            {
                REACTOR_LOG_DEBUG("File does not exist: {}",
                                  config.filePath.string());
                continue;
            }
            auto currentWriteTime =
                std::filesystem::last_write_time(config.filePath);
            if (currentWriteTime != config.lastWriteTime)
            {
                config.lastWriteTime = currentWriteTime;
                handler(config.filePath);
                // upload file
            }
        }
    }
    void updateTime(const std::filesystem::path& path)
    {
        auto iter = std::ranges::find_if(
            monitorConfigs, [&path](auto& v) { return v.filePath == path; });
        if (iter != std::end(monitorConfigs))
        {
            iter->lastWriteTime = std::filesystem::last_write_time(path);
        }
    }
    static SyncDb& globalSyncDb()
    {
        static SyncDb syncDb;
        return syncDb;
    }
    void addPaths(const std::vector<std::string>& paths)
    {
        for (auto& path : paths)
        {
            addConfig(path);
        }
    }

  private:
    SyncDb() {}
    struct MonitorConfigs
    {
        std::filesystem::file_time_type lastWriteTime;
        std::filesystem::path filePath;
    };
    std::vector<MonitorConfigs> monitorConfigs;
};

} // namespace reactor
