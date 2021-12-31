#include "../common.hpp"

namespace boost::asio { class io_context; }

#include <filesystem>
#include <future>
#include <string>
#include <string_view>

namespace haxterm {
  struct target {
//    virtual boost::asio::awaitable<void> write(std::span<const uint8_t> data) = 0;
//    virtual boost::asio::awaitable<size_t> read(std::span<uint8_t> buf) = 0;
    virtual std::future<void> write(std::span<const uint8_t> data) = 0;
    virtual std::future<size_t> read(std::span<uint8_t> buf) = 0;


    inline std::future<void> write(std::string_view str ) {
      return  write(std::span<const uint8_t>{reinterpret_cast<uint8_t const*>(str.data()), str.size()});
    }


//    inline boost::asio::awaitable<void> write(std::string_view str) {
//      co_await write(std::span<const uint8_t>{reinterpret_cast<uint8_t const*>(str.data()), str.size()});
//    };

//    inline boost::asio::awaitable<std::string> read(size_t buf_size = 65536) {
//      std::string buf;
//      buf.resize(buf_size);
//      buf.resize(co_await read(std::span<uint8_t>{reinterpret_cast<uint8_t*>(buf.data()), buf.size()}));
//      co_return buf;
//    }
    virtual ~target() = default;

    // TODO: args?
    static std::unique_ptr<target> local(std::filesystem::path executable, boost::asio::io_context* io_ctx);
    static std::unique_ptr<target> tcp(std::string_view remote, std::string_view port, boost::asio::io_context* io_ctx);
  };
}
