project(haxterm)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_CXX_FLAGS -fcoroutines-ts)

find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)

file(GLOB_RECURSE source src/*.cpp)
add_executable(haxterm ${source})
target_link_libraries(haxterm PUBLIC Boost::system Boost::filesystem Threads::Threads PRIVATE curses)
