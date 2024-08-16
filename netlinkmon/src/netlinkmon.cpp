#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <boost/asio.hpp>

#include <iostream>

class EthernetMonitor
{
  public:
    EthernetMonitor(boost::asio::io_service& io_service) :
        socket_(io_service, ::socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE))
    {
        if (socket_.native_handle() < 0)
        {
            throw std::runtime_error("Failed to create netlink socket");
        }

        sockaddr_nl addr = {};
        addr.nl_family = AF_NETLINK;
        addr.nl_groups = RTMGRP_LINK;

        if (::bind(socket_.native_handle(), (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            throw std::runtime_error("Failed to bind netlink socket");
        }

        startReceive();
    }

  private:
    void startReceive()
    {
        socket_.async_receive(
            boost::asio::buffer(buffer_),
            [this](boost::system::error_code ec, std::size_t length) {
            if (!ec)
            {
                handleReceive(length);
                startReceive();
            }
            else
            {
                std::cerr << "Receive error: " << ec.message() << std::endl;
            }
        });
    }

    void handleReceive(std::size_t length)
    {
        for (nlmsghdr* nlh = (nlmsghdr*)buffer_.data(); NLMSG_OK(nlh, length);
             nlh = NLMSG_NEXT(nlh, length))
        {
            if (nlh->nlmsg_type == RTM_NEWLINK ||
                nlh->nlmsg_type == RTM_DELLINK)
            {
                ifinfomsg* ifi = (ifinfomsg*)NLMSG_DATA(nlh);
                std::cout
                    << "Ethernet cable status changed for interface index: "
                    << ifi->ifi_index << std::endl;
            }
        }
    }

    boost::asio::posix::stream_descriptor socket_;
    std::array<char, 4096> buffer_;
};

int main()
{
    try
    {
        boost::asio::io_service io_service;
        EthernetMonitor monitor(io_service);
        io_service.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
