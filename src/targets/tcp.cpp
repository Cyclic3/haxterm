#include "iface.hpp"

#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/write.hpp>

#include <iostream>

namespace haxterm {
  class tcp_target : public target {
  private:
    // TODO: optional err?
    boost::asio::ip::tcp::socket sock;

  public:
//    boost::asio::awaitable<void> write(std::span<const uint8_t> data) override {
//      // TODO: handle errs
//      for (size_t n_written = 0; n_written < data.size();)
//        n_written += co_await sock.async_write_some(boost::asio::const_buffer(data.data(), data.size()), boost::asio::use_awaitable);
//    }
//    boost::asio::awaitable<size_t> read(std::span<uint8_t> buf) override {
//      co_return co_await sock.async_read_some(boost::asio::mutable_buffer(buf.data(), buf.size()), boost::asio::use_awaitable);
//    }
    std::future<void> write(std::span<const uint8_t> data) override {
      std::promise<void> promise;
      auto ret = promise.get_future();

      boost::asio::async_write(sock, boost::asio::const_buffer(data.data(), data.size()),
                               [promise{std::move(promise)}](boost::system::error_code err, size_t /*n*/) mutable {
        if (err.failed())
          promise.set_exception(std::make_exception_ptr<boost::system::system_error>(boost::system::system_error{err}));
        else
          promise.set_value();
      });

      return ret;
    }
    std::future<size_t> read(std::span<uint8_t> buf) override {
      std::promise<size_t> promise;
      auto ret = promise.get_future();

      sock.async_read_some(boost::asio::mutable_buffer(buf.data(), buf.size()),
                           [promise{std::move(promise)}](boost::system::error_code err, size_t n) mutable {
        if (err.failed())
          promise.set_exception(std::make_exception_ptr<boost::system::system_error>(boost::system::system_error{err}));
        else
          promise.set_value(n);
      });

      return ret;
    }

  public:
    tcp_target(std::string_view remote, std::string_view port, boost::asio::io_context* io_ctx) : sock{*io_ctx} {
      boost::asio::ip::tcp::resolver resolver{*io_ctx};
      for (auto ep : resolver.resolve(remote, port)) {
        boost::system::error_code ec;
        sock.connect(ep, ec);
        if (!ec.failed())
          return;
      }
      throw std::runtime_error{"Failed to connect to remote"};
    }
  };

  std::unique_ptr<target> target::tcp(std::string_view remote, std::string_view port, boost::asio::io_context* io_ctx) {
    return std::make_unique<tcp_target>(remote, port, io_ctx);
  }
}
