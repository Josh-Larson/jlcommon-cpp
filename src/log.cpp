#include <log.h>

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <ctime>
#include <chrono>

namespace jlcommon {

namespace Log {

#if defined(__unix__)
// Taken from: https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
#define CREATE_STRING(format, logType, inputString) \
	std::string inputString; \
	{\
		va_list args; \
		va_start (args, format); \
		size_t len = std::vsnprintf(NULL, 0, format, args); \
		va_end (args); \
		std::vector<char> vec(len + 1); \
		va_start (args, format); \
		std::vsnprintf(&vec[0], len + 1, format, args); \
		va_end (args); \
		\
		auto now = std::chrono::system_clock::now();\
		auto t = std::chrono::system_clock::to_time_t(now);\
		auto time = tm{};\
		localtime_r(&t, &time);\
		long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;\
		std::stringstream ss;\
		ss << std::put_time(&time, "%Y-%m-%d %H:%M:%S.") << std::setfill('0') << std::setw(3) << ms << " " << logType << ": " << &vec[0] << std::endl;\
		inputString = ss.str(); \
	}
#else
#define CREATE_STRING(format, logType, inputString) \
	std::string inputString; \
	{\
		va_list args; \
		va_start (args, format); \
		size_t len = std::vsnprintf(NULL, 0, format, args); \
		va_end (args); \
		std::vector<char> vec(len + 1); \
		va_start (args, format); \
		std::vsnprintf(&vec[0], len + 1, format, args); \
		va_end (args); \
		\
		auto now = std::chrono::system_clock::now();\
		const auto t = std::chrono::system_clock::to_time_t(now);\
		auto time = tm{};\
		localtime_s(&time, &t);\
		long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;\
		std::stringstream ss;\
		ss << std::put_time(&time, "%Y-%m-%d %H:%M:%S.") << std::setfill('0') << std::setw(3) << ms << " " << logType << ": " << &vec[0] << std::endl;\
		inputString = ss.str(); \
	}
#endif
static std::vector<std::function<void(const char *)>> wrappers;

void addWrapper(const std::function<void(const char *)>& wrapper) {
	wrappers.emplace_back(wrapper);
}

void clearWrappers() {
	wrappers.clear();
}

void trace(const char * format, ...) {
	CREATE_STRING(format, "T", userString)
	const char * str = userString.c_str();
	for (auto &wrapper : wrappers) {
		wrapper(str);
	}
}

void data(const char * format, ...) {
	CREATE_STRING(format, "D", userString)
	const char * str = userString.c_str();
	for (auto &wrapper : wrappers) {
		wrapper(str);
	}
}

void info(const char * format, ...) {
	CREATE_STRING(format, "I", userString)
	const char * str = userString.c_str();
	for (auto &wrapper : wrappers) {
		wrapper(str);
	}
}

void warn(const char * format, ...) {
	CREATE_STRING(format, "W", userString)
	const char * str = userString.c_str();
	for (auto &wrapper : wrappers) {
		wrapper(str);
	}
}

void error(const char * format, ...) {
	CREATE_STRING(format, "E", userString)
	const char * str = userString.c_str();
	for (auto &wrapper : wrappers) {
		wrapper(str);
	}
}

} // namespace Log

} // namespace jlcommon

