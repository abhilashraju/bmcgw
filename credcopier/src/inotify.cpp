#include <sys/inotify.h>

#include <boost/asio.hpp>

class inotify_service : public boost::asio::io_context::service
{
  public:
    static boost::asio::io_context::id id;

    class implementation_type
    {
      public:
        int inotify_fd;
    };

    explicit inotify_service(boost::asio::io_context& io_context) :
        boost::asio::io_context::service(io_context)
    {}

    void construct(implementation_type& impl)
    {
        impl.inotify_fd = inotify_init1(IN_NONBLOCK);
        if (impl.inotify_fd == -1)
        {
            // Handle error
        }
        int wd = inotify_add_watch(impl.inotify_fd, "/tmp",
                                   IN_MODIFY | IN_CREATE);
        if (wd == -1)
        {
            // Handle error
        }
    }

    void destroy(implementation_type& impl)
    {
        close(impl.inotify_fd);
    }

    template <typename ReadHandler>
    void async_read_some(implementation_type& impl,
                         boost::asio::mutable_buffer buffer,
                         ReadHandler handler)
    {
        boost::asio::io_context& ctx =
            static_cast<boost::asio::io_context&>(context());
        boost::asio::posix::stream_descriptor descriptor(ctx, impl.inotify_fd);
        boost::asio::async_read(descriptor, buffer, handler);
    }
};

boost::asio::io_context::id inotify_service::id;

class my_io_object
{
  public:
    explicit my_io_object(boost::asio::io_context& io_context) :
        service_(boost::asio::use_service<inotify_service>(io_context))
    {
        service_.construct(impl_);
    }

    template <typename ReadHandler>
    void async_read_some(boost::asio::mutable_buffer buffer,
                         ReadHandler handler)
    {
        service_.async_read_some(impl_, buffer, handler);
    }
    boost::asio::execution_context& get_executor()
    {
        return service_.context();
    }

  private:
    inotify_service& service_;
    inotify_service::implementation_type impl_;
};

int main()
{
    boost::asio::io_context ctx;
    my_io_object obj(ctx);
    boost::asio::mutable_buffer buff;
    obj.async_read_some(buff, [](auto ec, auto bytes) {

    });

    ctx.run();
    // boost::asio::io_context io_context;
    // int fd ;
    // boost::asio::posix::stream_descriptor descriptor(io_context, fd);

    // char buffer[128];
    // descriptor.async_read_some(boost::asio::buffer(buffer), [](const
    // boost::system::error_code& error, std::size_t bytes_transferred) {
    //     // Handle the result of the read operation here
    // });

    // io_context.run();
}
