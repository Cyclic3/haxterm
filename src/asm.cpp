#include "common.hpp"

#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/process/io.hpp>
#include <boost/process/async.hpp>

#include <filesystem>

namespace haxterm {
  std::optional<std::vector<uint8_t>> do_assemble(std::string_view path) {
#ifdef linux
    std::future<std::string> buf;
    int err = boost::process::system(boost::process::search_path("nasm"), "-fbin", std::string{path}, "-o/dev/stdout",
                                     boost::process::std_in < boost::process::null,
                                     boost::process::std_out > buf,
                                     boost::process::std_err > boost::process::null);
    if (err)
      return std::nullopt;
    auto res = buf.get();
    return std::vector<uint8_t>(res.begin(), res.end());
#elif defined(WIN32)
#error "secure NASM'ing from user input not yet implemented in windows"
#endif
  }
}
