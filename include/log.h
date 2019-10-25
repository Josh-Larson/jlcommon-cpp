#pragma once

#include <functional>

namespace jlcommon {

namespace Log {

void addWrapper(const std::function<void(const char *)>& wrapper);
void clearWrappers();

void trace(const char * format, ...);
void data(const char * format, ...);
void info(const char * format, ...);
void warn(const char * format, ...);
void error(const char * format, ...);

} // namespace Log

} // namespace jlcommon
