#pragma once
#include <string>
namespace reactor
{

struct Parser
{
    constexpr static const char* heder_delimiter = "\r\n\r\n";
    std::string currentdata;
    std::string header;
    bool headerProcessed{false};
    virtual ~Parser() {}
    bool isHeaderPrcessed()
    {
        return headerProcessed;
    }
    bool parseHeader()
    {
        if (header.empty())
        {
            auto pos = currentdata.find(heder_delimiter);
            if (pos != std::string_view::npos)
            {
                auto subdata = currentdata.substr(0, pos);
                header = std::string(subdata.data(), subdata.size());
                currentdata = currentdata.substr(
                    pos + std::string_view(heder_delimiter).size());
                return true;
            }

            return false;
        }
        return true;
    }
    bool processHeader()
    {
        headerProcessed = true;
        return handleHeader(header);
    }
    bool processBody()
    {
        return handleBody(currentdata);
    }
    virtual bool handleHeader(std::string_view header) = 0;
    virtual bool handleBody(std::string_view data) = 0;
    bool parse(std::string_view data)
    {
        try
        {
            currentdata = std::string(data.data(), data.size());
            if (!parseHeader())
            {
                return true; // wait for more data;
            }
            if (!isHeaderPrcessed())
            {
                if (!processHeader())
                {
                    return false; // error in processing header
                }
            }
            return processBody();
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << "\n";
            return false;
        }
        return true;
    }
};
} // namespace reactor
