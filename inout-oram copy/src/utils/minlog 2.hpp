#pragma once

#include <cstring>
#include <iostream>
#include <string>

#include "ext/termcolor.hpp"
#include "ext/tinyformat.hpp"

enum class LoggerVerbosity : int {
  annoying = 8,
  pedantic = 7,
  debug = 6,
  trace = 5,
  verbose = 4,
  info = 3,
  warn = 2,
  error = 1,
  crit = 0,
};

struct Logger {
  LoggerVerbosity verbosity = LoggerVerbosity::info;

private:
  char source[32] = {0}; // fixed size source buffer, because std::string size is implementation-defined

  std::string short_verbosity(LoggerVerbosity level) const {
    switch (level) {
    case LoggerVerbosity::annoying:
      return "ayg";
    case LoggerVerbosity::pedantic:
      return "ped";
    case LoggerVerbosity::debug:
      return "dbg";
    case LoggerVerbosity::trace:
      return "trc";
    case LoggerVerbosity::verbose:
      return "vrb";
    case LoggerVerbosity::info:
      return "inf";
    case LoggerVerbosity::warn:
      return "wrn";
    case LoggerVerbosity::error:
      return "err";
    case LoggerVerbosity::crit:
      return "crt";
    default:
      return std::to_string(static_cast<int>(level));
    }
  }

  void colorize_fg_for(std::ostringstream& oss, LoggerVerbosity level) const {
    switch (level) {
    case LoggerVerbosity::annoying:
    case LoggerVerbosity::pedantic:
    case LoggerVerbosity::debug:
      oss << termcolor::bright_grey;
      break;
    case LoggerVerbosity::trace:
      oss << termcolor::white;
      break;
    case LoggerVerbosity::verbose:
      oss << termcolor::blue;
      break;
    case LoggerVerbosity::info:
      oss << termcolor::green;
      break;
    case LoggerVerbosity::warn:
      oss << termcolor::yellow;
      break;
    case LoggerVerbosity::error:
      oss << termcolor::red;
      break;
    case LoggerVerbosity::crit:
      oss << termcolor::magenta;
      break;
    default:
      oss << termcolor::white;
      break;
    }
  }

  void write_meta(std::ostringstream& oss, LoggerVerbosity level) const {
    auto level_shortname = short_verbosity(level);

    if (strlen(source) > 0) {
      oss << termcolor::on_grey << termcolor::bright_grey;
      oss << "[";
      oss << source;
      oss << "]";
      oss << termcolor::reset;
      oss << " ";
    }

    oss << "[";
    oss << termcolor::on_grey;
    colorize_fg_for(oss, level);
    oss << level_shortname;
    oss << termcolor::reset;
    oss << "]";
  }

  void write_line(std::string log_str, LoggerVerbosity level) const {
    std::ostringstream oss;
    if (termcolor::_internal::is_atty(std::cout)) {
      oss << termcolor::colorize;
    }
    write_meta(oss, level);
    oss << " " << log_str << std::endl;
    std::cout << oss.str();
  }

  void put(std::string log_str, LoggerVerbosity level) const {
#ifdef ENABLE_LOGGING
    if (level > verbosity) {
      return;
    }
    write_line(log_str, level);
#endif
  }

public:
  Logger() : Logger(LoggerVerbosity::info) {}
  Logger(LoggerVerbosity verbosity) : verbosity(verbosity) {}

  Logger for_source(std::string source) const {
    Logger new_logger = *this;
    strncpy(new_logger.source, source.c_str(), sizeof(new_logger.source) - 1);
    return new_logger;
  }

#ifdef ENABLE_LOGGING
  void ayg(std::string log_str) const { put(log_str, LoggerVerbosity::annoying); }
  void ped(std::string log_str) const { put(log_str, LoggerVerbosity::pedantic); }
  void dbg(std::string log_str) const { put(log_str, LoggerVerbosity::debug); }
  void trc(std::string log_str) const { put(log_str, LoggerVerbosity::trace); }
  void vrb(std::string log_str) const { put(log_str, LoggerVerbosity::verbose); }
  void inf(std::string log_str) const { put(log_str, LoggerVerbosity::info); }
  void wrn(std::string log_str) const { put(log_str, LoggerVerbosity::warn); }
  void err(std::string log_str) const { put(log_str, LoggerVerbosity::error); }
  void cri(std::string log_str) const { put(log_str, LoggerVerbosity::crit); }
#else
  void ayg(std::string log_str) const {}
  void ped(std::string log_str) const {}
  void dbg(std::string log_str) const {}
  void trc(std::string log_str) const {}
  void vrb(std::string log_str) const {}
  void inf(std::string log_str) const {}
  void wrn(std::string log_str) const {}
  void err(std::string log_str) const {}
  void cri(std::string log_str) const {}
#endif
};

// global logger
inline Logger g_logger(LoggerVerbosity::info);

inline __attribute__((always_inline)) void enforce(bool condition, std::string message) {
#if __cplusplus >= 202002L // C++20
  if (!condition) [[unlikely]] {
#else
  if (!condition) {
#endif
    throw std::runtime_error(message);
  }
}

inline __attribute__((always_inline)) void ensure(bool condition, std::string message) {
#if __cplusplus >= 202002L // C++20
  if (!condition) [[unlikely]] {
#else
  if (!condition) {
#endif
    throw std::runtime_error(message);
  }
}

inline __attribute__((always_inline)) void ensure(const Logger& logger, bool condition, std::string message) {
#if __cplusplus >= 202002L // C++20
  if (!condition) [[unlikely]] {
#else
  if (!condition) {
#endif
    logger.err(message);
    enforce(condition, message);
  }
}

// a macro version of ENSURE
#if !defined(DISABLE_ASSERTIONS)
#define ENSURE(condition, message)                                                                                     \
  if (!(condition)) {                                                                                                  \
    g_logger.err(message);                                                                                             \
    throw std::runtime_error(message);                                                                                 \
  }
#else
#define ENSURE(condition, message)
#endif

inline __attribute__((always_inline)) void fail(std::string message) {
  g_logger.err(message);
  enforce(false, message);
}

inline __attribute__((always_inline)) void fail(const Logger& logger, std::string message) {
  logger.err(message);
  enforce(false, message);
}
