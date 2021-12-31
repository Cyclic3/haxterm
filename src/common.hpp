#pragma once

#include <functional>
#include <future>
#include <optional>
#include <span>
#include <string_view>

//// TODO: replace with C++20 coroutines when they work
//#include <boost/asio/coroutine.hpp>

//#define __cpp_impl_coroutine 1
//#include <coroutine>
//#undef __cpp_impl_coroutine

//#define BOOST_ASIO_HAS_STD_COROUTINE
//#define BOOST_ASIO_HAS_CO_AWAIT
//#include <boost/asio/co_spawn.hpp>
//#undef BOOST_ASIO_HAS_CO_AWAIT
//#undef BOOST_ASIO_HAS_STD_COROUTINE

//namespace std::experimental {
//  using namespace std;
//}

namespace haxterm {
//  class defer {
//  private:
//    std::function<void()> func;
//  public:
//    template<typename T>
//    inline defer(T&& f) : func{f} {}
//    ~defer() { func(); }
//  };
  std::optional<std::vector<uint8_t>> do_assemble(std::string_view path);
}
