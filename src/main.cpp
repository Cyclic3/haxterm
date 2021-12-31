#include "target.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <curses.h>

#include <array>
#include <charconv>
#include <iostream>

using namespace std::string_view_literals;

boost::asio::io_context io_ctx;

// input modes:
// (s)td
// (f)ile
// (a)ssembly
// he(x)
// autometerpreter???

// output modes:
// (s)td
// he(x), with extraction submode
// (a)ssembly?

class screen_handler {
private:
  enum class input_mode_t {
    Standard,
    Hexadecimal,
    AssembleFile,
    // Used for looking through the output
    Browse
  };

  enum class output_mode_t {
    Standard,
    Hexadecimal
  };

  enum colours_t : int {
    Standard = 0,
    Inverted = 1,
    Escaped = 2,
    InputHexadecimal = 3,
    OutputHexadecimal = 3,
    CurrentInput = 4,
    OldInput = 5,
    MarkedOutput = 6,
    AssembleFile = 7,

    MAX_COLOUR = 7
  };

  enum escape_state_t : int {
    Unescaped = 0,
    InputEscaped = 1,
    OutputEscaped = 2,
    BothEscaped = InputEscaped | OutputEscaped,
  };

  constexpr static std::array<std::pair<int, int>, colours_t::MAX_COLOUR + 1> colours {{
      // XXX: foreground colour does not appear to be easily changeable, maybe force clear if required?
    { COLOR_WHITE, COLOR_BLACK }, // standard
    { COLOR_BLACK, COLOR_WHITE }, // inverted
    { COLOR_BLACK, COLOR_YELLOW }, // escaped
    { COLOR_BLACK, COLOR_GREEN }, // hex
    { COLOR_GREEN | 8, COLOR_BLACK }, // current
    { COLOR_RED | 8, COLOR_BLACK }, // old
    { COLOR_YELLOW | 8, COLOR_BLACK }, // marked
    { COLOR_BLACK, COLOR_MAGENTA }, // asm
  }};

  struct interaction_t {
    bool was_us;
    std::vector<uint8_t> buf;
    // TODO: write this in so that users can mark text across modes
    std::vector<std::pair<size_t, size_t>> marks;
  };

private:
  WINDOW* main_win;
  WINDOW* in_info_win;
  WINDOW* out_info_win;
  int y_max, x_max;
  std::vector<interaction_t> interactions;
  input_mode_t input_mode = input_mode_t::Standard;
  output_mode_t output_mode = output_mode_t::Standard;
  escape_state_t escape_state = Unescaped;

  std::string input_line;
  size_t input_pos = 0;

  std::unique_ptr<haxterm::target> target;
  boost::asio::io_context* io_ctx;


private:
  void update_main() {
    wmove(main_win, 0, 0);
    wclear(main_win);

    switch (output_mode) {
      case output_mode_t::Standard: {
        // TODO: optimise
        for (auto& interaction: interactions) {
          auto flag = interaction.was_us ? COLOR_PAIR(OldInput) : COLOR_PAIR(Standard);
          wattron(main_win, flag);

          for (char i : interaction.buf) {
            if (i != '\e')
              ::wprintw(main_win, "%c", i);
            else
              ::wprintw(main_win, "\ufffd", i);
          }

          wattroff(main_win, flag);
        }
      } break;

      case output_mode_t::Hexadecimal: {
        // TODO: optimise
        for (auto& interaction: interactions) {
          auto flag = interaction.was_us ? COLOR_PAIR(OldInput) : COLOR_PAIR(Standard);
          wattron(main_win, flag);

          constexpr size_t hexdump_width = 16;
          const size_t n_lines = (interaction.buf.size() + hexdump_width - 1) / hexdump_width;

          for (size_t line = 0; line < n_lines; ++line) {
            ::wprintw(main_win, "%08x ", line * hexdump_width);
            for (size_t i = 0; i < hexdump_width; ++i) {
              auto idx = line * hexdump_width + i;
              if (idx >= interaction.buf.size())
                ::wprintw(main_win, "  ");
              else
                ::wprintw(main_win, "%02x", interaction.buf.at(idx));
              if (idx % 2 == 1)
                ::wprintw(main_win, " ");
            }
            for (size_t i = 0; i < hexdump_width; ++i) {
              auto idx = line * hexdump_width + i;
              if (idx >= interaction.buf.size())
                break;
              char c = interaction.buf.at(idx);
              ::wprintw(main_win, "%c", ::isprint(c) ? c : '.');
            }

            ::wprintw(main_win, "\n");
          }

          wattroff(main_win, flag);
        }
      } break;
      default: abort();
    }
  }
  void update_in() {
    wclear(in_info_win);
    mvwaddch(in_info_win, 0, x_max/2 - 1, ACS_VLINE);
    wmove(in_info_win, 0, 0);
    wprintw(in_info_win, "input: ");
    if (escape_state & InputEscaped) {
      wprintw(in_info_win, "ESCAPED");
      wbkgd(in_info_win, COLOR_PAIR(colours_t::Escaped));
      return;
    }
    switch (input_mode) {
      case input_mode_t::Hexadecimal:
        wprintw(in_info_win, "hexadecimal");
        wbkgd(in_info_win, COLOR_PAIR(colours_t::InputHexadecimal));
        break;

      case input_mode_t::AssembleFile:
        wprintw(in_info_win, "assembly file");
        wbkgd(in_info_win, COLOR_PAIR(colours_t::AssembleFile));
        break;

      case input_mode_t::Standard:
      default:
        wprintw(in_info_win, "standard");
        wbkgd(in_info_win, COLOR_PAIR(colours_t::Inverted));
    }
    mvwaddch(in_info_win, 0, x_max/2 - 1, ACS_VLINE);
  }
  void update_out() {
    wmove(out_info_win, 0, 0);
    wclear(out_info_win);
    wprintw(out_info_win, "output: ");
    if (escape_state & OutputEscaped) {
      wprintw(out_info_win, "ESCAPED");
      wbkgd(out_info_win, COLOR_PAIR(colours_t::Escaped));
      return;
    }
    switch (output_mode) {
      case output_mode_t::Hexadecimal:
        wprintw(out_info_win, "hexadecimal");
        wbkgd(out_info_win, COLOR_PAIR(colours_t::OutputHexadecimal));
        break;

      case output_mode_t::Standard:
      default:
        wprintw(out_info_win, "standard");
        wbkgd(out_info_win, COLOR_PAIR(colours_t::Inverted));
    }
  }
  void create_wins() {
    getmaxyx(stdscr, y_max, x_max);

    in_info_win = newwin(1, 20, y_max-1, 0);
    out_info_win = newwin(1, 20, y_max-1, x_max - 20);
    main_win = newwin(y_max - 1, x_max, 0, 0);

    // Enable extra keys
    keypad(main_win, true);
    // non-blocking getch
    nodelay(main_win, true);
    // Enable scrolling
    scrollok(main_win, true);
    // maybe useless for scrolling idk
//    idlok(main_win, true);
  }
  void update_all() {
    update_main();
    update_in();
    update_out();
  }
  void refresh_all() {
    wrefresh(in_info_win);
    wrefresh(out_info_win);

    // Update this last so it gets focus
    wrefresh(main_win);
  }
  void update_dims() noexcept {
    delwin(main_win);
    delwin(in_info_win);
    delwin(out_info_win);
    create_wins();
    update_all();
    refresh_all();
  }

  // todo: proper error handling
  void flush_input() {
    // Don't let this do a bad
    input_pos = 0;
    switch (input_mode) {
      case input_mode_t::Standard: {
        interactions.push_back({.was_us = true, .buf = std::vector<uint8_t>(input_line.begin(), input_line.end())});
        // *techinically* as long as we don't delete the interaction_t, this write will not hit memory issues, due to the magic of move
        //
        // but that's actually more effort to impl and has very little performance effect
        target->write(interactions.back().buf).wait();
//        boost::asio::co_spawn(*io_ctx, target->write(interactions.back().buf), boost::asio::detached);
      } break;
      case input_mode_t::AssembleFile: {
        // Based on: https://stackoverflow.com/a/217605
        auto rstrip_end_iter = std::find_if_not(input_line.rbegin(), input_line.rend(), ::isspace);
        auto rstring_end = input_line.rend() - rstrip_end_iter;
        auto path = input_line.substr(0, rstring_end);
        auto asm_res = haxterm::do_assemble(path);
        if (!asm_res) {
          input_line.clear();
          update_main();
          return;
        }
        interactions.push_back({.was_us = true, .buf = std::move(*asm_res)});
        target->write(interactions.back().buf).wait();
      } break;

      case input_mode_t::Hexadecimal: {
        std::vector<uint8_t> buf;

        for (size_t i = 0; i < input_line.size();) {
          if (::isspace(input_line.at(i))) {
            ++i;
            continue;
          }

          if (i + 1 >= input_line.size())
            return;

          buf.emplace_back();
          auto res = std::from_chars(input_line.data() + i, input_line.data() + i + 2, buf.back(), 16);

          if (static_cast<int>(res.ec) != 0)
            return;

          i+= 2;
        }
        interactions.push_back({.was_us = true, .buf = std::move(buf)});

        target->write(interactions.back().buf).wait();
      } break;

      default: abort();
    }

    // Grumble grumble implementation defined grumble grumble
    input_line.clear();
    // reload the window
    update_main();
  }

  void move_left() {
    int y,x;
    getyx(main_win, y, x);

    if (x == 0) {
      if (y == 0)
        return;
      --y;
      x = x_max - 1;
    }
    else
      --x;

    wmove(main_win, y, x);
  }

  void move_right() {
    int y,x;
    getyx(main_win, y, x);

    if (x == x_max + 1) {
      x = 0;
      ++y;
    }
    else
      ++x;

    wmove(main_win, y, x);
  }

public:
  // Returns true if the loop should continue
  bool handle_char(int c) {
    switch (escape_state) {
      case BothEscaped: {
        switch (c) {
          case 'i':
          case KEY_LEFT:
            escape_state = InputEscaped;
            update_out();
            break;

          case 'o':
          case KEY_RIGHT:
            escape_state = OutputEscaped;
            update_in();
            break;

          case 'f':
            flush_input();
            escape_state = Unescaped;
            update_out();
            update_in();
            break;

          case '\e':
          default:
            escape_state = Unescaped;
            update_out();
            update_in();
            break;
        }
      } { refresh_all(); return true; }
      case InputEscaped: {
        switch (c) {
          case 'o':
          case KEY_RIGHT:
            escape_state = OutputEscaped;
            update_out();
            break;

          case 's':
            escape_state = Unescaped;
            input_mode = input_mode_t::Standard;
            break;

          case 'x':
            escape_state = Unescaped;
            input_mode = input_mode_t::Hexadecimal;
            break;

          case 'b':
            escape_state = Unescaped;
            input_mode = input_mode_t::Browse;
            break;

          case 'a':
            escape_state = Unescaped;
            input_mode = input_mode_t::AssembleFile;

          default:
            escape_state = Unescaped;
            break;
        }
      } { update_in(); refresh_all(); return true; }
      case OutputEscaped: {
        switch (c) {
          case 'i':
          case KEY_LEFT:
            escape_state = InputEscaped;
            update_in();
            break;

          case 's':
            escape_state = Unescaped;
            output_mode = output_mode_t::Standard;
            update_main();
            break;

          case 'x':
            escape_state = Unescaped;
            output_mode = output_mode_t::Hexadecimal;
            update_main();
            break;

        default:
          escape_state = Unescaped;
          break;
        }
      } { update_out(); refresh_all(); return true; }
      case Unescaped: {}
    }


    if (c < 256 && isprint(c)) {
      wattron(main_win, COLOR_PAIR(CurrentInput));

      waddch(main_win, c);

      int y,x;
      getyx(main_win, y, x);
      wprintw(main_win, "%s", input_line.c_str() + input_pos);
      wmove(main_win, y, x);
      refresh_all();


      input_line.insert(input_pos, 1, c);
      ++input_pos;

      wattroff(main_win, COLOR_PAIR(CurrentInput));

      return true;
    }

    switch (c) {
      case '\e': {
        escape_state = BothEscaped;
        update_in(); update_out();
        refresh_all();
      } break;
      case '\n':
      case KEY_ENTER: {
        input_line.push_back('\n');
        wprintw(main_win, "\n");
        flush_input();
      } break;
      case KEY_BACKSPACE: {
        if (input_pos == 0)
          break;
        move_left();
        wdelch(main_win);
        --input_pos;
        input_line.erase(input_pos, 1);
        refresh_all();
      } break;
      case KEY_DC: { // delete key
        if (input_pos >= input_line.size())
          break;
        wdelch(main_win);
        input_line.erase(input_pos, 1);
        refresh_all();
      } break;
      case KEY_LEFT: {
        if (input_pos == 0)
          break;
        input_pos -= 1;
        move_left();
        refresh_all();
      } break;
      case KEY_RIGHT: {
        if (input_pos >= input_line.size() - 1)
          break;
        input_pos += 1;
        move_right();
        refresh_all();
      } break;
      case KEY_END: {
        for (;input_pos < input_line.size();++input_pos)
          move_right();
      } break;
      case KEY_HOME: {
        for (;input_pos > 0;--input_pos)
          move_left();
      } break;
      case KEY_RESIZE: update_dims(); break;
    }
    return true;
  }

  void loop() {
    constexpr size_t buf_size = 65536;
    // todo: rework this mess
    std::vector<uint8_t> buf;
    std::future<size_t> current_read;
    while (true) {
      int c = wgetch(main_win);
      if (c != ERR)
        handle_char(c);
      if (!current_read.valid()) {
        buf.resize(buf_size);
        current_read = target->read(buf);
      }
      else if (current_read.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
        buf.resize(current_read.get());
        interactions.emplace_back(interaction_t{.was_us = false, .buf = std::move(buf)});
        update_main();
      }
    }
  }

public:
  screen_handler(decltype(target)&& target_, boost::asio::io_context* io_ctx_) : target{std::move(target_)}, io_ctx{io_ctx_} {
    initscr();
    raw();
    cbreak();
    noecho();
    set_escdelay(false);
    start_color();
    for (size_t i = 0; i <= colours_t::MAX_COLOUR; ++i)
      init_pair(i, colours[i].first, colours[i].second);

    create_wins();
    update_all();
    refresh_all();
  }
  ~screen_handler() {
    endwin();
    // possible window memory leak?
    //
    // doesn't really matter as this is a singleton class
  }
};

void print_help(char* argv_0) {
  std::cerr << argv_0 << " (tcp <host> <port> | local <path>)" << std::endl;
}

//boost::asio::awaitable<void> foo(boost::asio::io_context* io_ctx) {
//  auto target = haxterm::target::local("bash", io_ctx);
//  co_await target->write("id");
//  std::cout << co_await target->read() <<std::endl;
//}

int main(int argc, char** argv) {
  if (argc == 0) {
    std::cout << "You are a moron" << std::endl;
    abort();
  }

  if (argc == 1) {
    print_help(argv[0]);
    return 1;
  }

  boost::asio::io_context io_ctx;
  std::unique_ptr<haxterm::target> target;

  if (argv[1] == "tcp"sv && argc == 4)
    target = haxterm::target::tcp(argv[2], argv[3], &io_ctx);
  else if (argv[1] == "local"sv && argc == 3)
    target = haxterm::target::local(argv[2], &io_ctx);
  else if (argv[1] == "help"sv || argv[1] == "--help"sv || argv[1] == "-h"sv) {
    print_help(argv[0]);
    return 0;
  }
  else {
    print_help(argv[0]);
    return 1;
  }


  std::vector<std::jthread> workers;

  auto guard = boost::asio::make_work_guard(io_ctx);

  for (size_t i = 0; i < std::max<size_t>(1, std::thread::hardware_concurrency()); ++i)
    workers.emplace_back([&]{io_ctx.run();});


  screen_handler scr(std::move(target), &io_ctx);
  scr.loop();
}
