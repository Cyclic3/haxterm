#include "iface.hpp"

#include <boost/process/async_pipe.hpp>
#include <boost/process/child.hpp>
#include <boost/process/io.hpp>

#include <boost/asio/write.hpp>

#include <iostream>

namespace haxterm {
  class local_target : public target {
  private:
    // TODO: optional err?
    boost::process::async_pipe std_in, std_out;
    boost::process::child child;

  public:
//    boost::asio::awaitable<void> write(std::span<const uint8_t> data) override {
//      // TODO: handle errs
//      for (size_t n_written = 0; n_written < data.size();)
//        n_written += co_await std_in.async_write_some(boost::asio::const_buffer(data.data(), data.size()), boost::asio::use_awaitable);
//    }
//    boost::asio::awaitable<size_t> read(std::span<uint8_t> buf) override {
//      co_return co_await std_out.async_read_some(boost::asio::mutable_buffer(buf.data(), buf.size()), boost::asio::use_awaitable);
//    }

    std::future<void> write(std::span<const uint8_t> data) override {
      std::promise<void> promise;
      auto ret = promise.get_future();

      boost::asio::async_write(std_in, boost::asio::const_buffer(data.data(), data.size()),
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

      std_out.async_read_some(boost::asio::mutable_buffer(buf.data(), buf.size()),
                              [promise{std::move(promise)}](boost::system::error_code err, size_t n) mutable {
        if (err.failed())
          promise.set_exception(std::make_exception_ptr<boost::system::system_error>(boost::system::system_error{err}));
        else
          promise.set_value(n);
      });

      return ret;
    }

  public:
    local_target(std::filesystem::path executable, boost::asio::io_context* io_ctx) : std_in{*io_ctx}, std_out{*io_ctx},
      // todo: configurable stderr capture
      child{executable.c_str(), boost::process::std_in < std_in, boost::process::std_out > std_out, boost::process::std_err > boost::process::null} {
    }
    ~local_target() {
      child.terminate();
    }
  };

  std::unique_ptr<target> target::local(std::filesystem::path executable, boost::asio::io_context* io_ctx) {
    return std::make_unique<local_target>(executable, io_ctx);
  }
}
