#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <string.h>
#include "textselect.hpp"

#include <imgui.h>
#include <spdlog/sinks/base_sink.h>

std::string stripColorCodes(const std::string &input);

class IgIgPageConsole {
public:
  bool autoScroll = true;
  bool scrollToBottomNextFrame = false;
  bool reclaimFocusOnShow = false;
  std::vector<std::string> items;
  std::vector<std::string> rawItems;
  std::mutex itemsMutex;

  static IgIgPageConsole &instance();

  ~IgIgPageConsole();

  void draw();
  void clear();
  void execCmd(const std::string &cmd);
  inline void log(std::string &&msg) {
    std::unique_lock lk(itemsMutex);
    char *next_token;
    char *token = strtok_s(msg.data(),  "\n", &next_token);
    while (token) {
      items.push_back(std::string(token) + "\n");
      rawItems.push_back(stripColorCodes(std::string(token) + "\n"));
      token = strtok_s(NULL, "\n", &next_token);
    }
    
  }

  int TextEditCallBack(ImGuiInputTextCallbackData *data);
  template <typename... T>
  inline void log(fmt::format_string<T...> fmt, T &&...args) {
    std::unique_lock lk(itemsMutex);
    std::string result = fmt::vformat(fmt, fmt::make_format_args(args...));
    char *next_token;
    char *token = strtok_s(result.data(), "\n", &next_token);
    while (token) {
      items.push_back(std::string(token) + "\n");
      rawItems.push_back(stripColorCodes(std::string(token) + "\n"));
      token = strtok_s(NULL, "\n", &next_token);
    }
  }

private:
  std::vector<std::string> history;
  int64_t historyPos = -1;

 

  std::string inputBuf;

  HANDLE stdoutPipeRead = NULL;
  HANDLE stdoutPipeWrite = NULL;
  std::jthread stdoutPipeReadThread;

  IgIgPageConsole();
};

template <typename Mutex>
class igig_console_sink : public spdlog::sinks::base_sink<Mutex> {
private:
  // Foreground colors
  const spdlog::string_view_t black = "\033[30m";
  const spdlog::string_view_t red = "\033[31m";
  const spdlog::string_view_t green = "\033[32m";
  const spdlog::string_view_t yellow = "\033[33m";
  const spdlog::string_view_t blue = "\033[34m";
  const spdlog::string_view_t magenta = "\033[35m";
  const spdlog::string_view_t cyan = "\033[36m";
  const spdlog::string_view_t white = "\033[37m";

  const spdlog::string_view_t reset = "\033[m";

  std::array<spdlog::string_view_t, spdlog::level::n_levels> colors_;

public:
  igig_console_sink() {
    colors_.at(spdlog::level::trace) = white;
    colors_.at(spdlog::level::debug) = cyan;
    colors_.at(spdlog::level::info) = green;
    colors_.at(spdlog::level::warn) = yellow;
    colors_.at(spdlog::level::err) = red;
    colors_.at(spdlog::level::critical) = red;
    colors_.at(spdlog::level::off) = reset;
  }

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

    if (msg.color_range_end > msg.color_range_start) {
      spdlog::memory_buf_t colored;
      colored.append(formatted.begin(),
                     formatted.begin() + msg.color_range_start);
      const auto& color = colors_.at(static_cast<size_t>(msg.level));
      colored.append(color.begin(), color.end());
      colored.append(formatted.begin() + msg.color_range_start,
                     formatted.begin() + msg.color_range_end);
      colored.append(reset.begin(), reset.end());
      colored.append(formatted.begin() + msg.color_range_end, formatted.end());
      IgIgPageConsole::instance().log(
          spdlog::fmt_lib::to_string(colored));
    } else {
      IgIgPageConsole::instance().log(
          spdlog::fmt_lib::to_string(formatted));
    }
  }

  void flush_() override {}
};

using igig_console_sink_mt = igig_console_sink<std::mutex>;