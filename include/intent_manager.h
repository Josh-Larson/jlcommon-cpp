//
// Created by Josh Larson on 10/16/19.
//

#pragma once

#include "blocking_queue.h"
#include "log.h"

#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <memory>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <atomic>
#include <utility>
#include <map>
#include <tuple>
#include <string>

namespace jlcommon {

template<typename T>
using IntentCallback = std::function<void(const T &)>;
using IntentCallbackCompiled = std::function<void()>;

namespace IntentManagerHelper {

class GenericIntentRunner {
	public:
	virtual ~GenericIntentRunner() = default;
	
	virtual std::map<std::pair<std::string, std::string>, uint64_t> getIntentTiming() {
		throw std::exception();
	}
	
};

template<typename T>
class IntentHandler : public GenericIntentRunner {
	public:
	IntentHandler(const IntentCallback<T> && handler, std::string && name): handler(std::move(handler)), name(std::move(name)) {}
	const IntentCallback<T> handler;
	const std::string name;
	
	void addTime(uint64_t time) {
		totalRunTime += time;
		totalRunCount++;
	}
	
	uint64_t getAverageTimeNanoseconds() {
		if (totalRunCount == 0)
			return 0;
		return totalRunTime / totalRunCount;
	}
	
	inline void operator()(const T & arg) const { handler(std::move(arg)); }
	
	private:
	std::atomic_uint64_t totalRunTime{0};
	std::atomic_uint64_t totalRunCount{0};
};

} // namespace IntentManagerHelper

template<typename T>
class IntentRunner : public IntentManagerHelper::GenericIntentRunner {
	public:
	
	inline void subscribe(std::string && name, const IntentCallback<T> && handler) noexcept {
		mHandlers.emplace_back(std::make_shared<IntentManagerHelper::IntentHandler<T>>(std::move(handler), std::move(name)));
	}
	
	inline unsigned int broadcast(LinkedBlockingQueue<IntentCallbackCompiled> & executionQueue, const T & arg) noexcept {
		unsigned int handlerCount = 0;
		for (auto & f : mHandlers) {
			executionQueue.add([f, arg]() {
				try {
					auto begin = std::chrono::high_resolution_clock::now();
					(*f)(std::move(arg));
					auto end = std::chrono::high_resolution_clock::now();
					f->addTime(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count());
				} catch (const std::exception &e) {
					Log::error("Exception thrown when handling %s in %s. %s", typeid(T).name(), f->name.c_str(), e.what());
				} catch (const std::string &s) {
					Log::error("Exception thrown when handling %s in %s. %s", typeid(T).name(), f->name.c_str(), s.c_str());
				} catch (const char *s) {
					Log::error("Exception thrown when handling %s in %s. %s", typeid(T).name(), f->name.c_str(), s);
				} catch (...) {
					Log::error("Exception thrown when handling %s in %s. Unknown Error.", typeid(T).name(), f->name.c_str());
				}
			});
			handlerCount++;
		}
		return handlerCount;
	}
	
	std::map<std::pair<std::string, std::string>, uint64_t> getIntentTiming() override {
		std::map<std::pair<std::string, std::string>, uint64_t> ret;
		auto type = std::string(typeid(T).name());
		for (auto & f : mHandlers) {
			ret[std::make_pair(type, f->name)] = f->getAverageTimeNanoseconds();
		}
		return ret;
	}
	
	private:
	std::vector<std::shared_ptr<IntentManagerHelper::IntentHandler<T>>> mHandlers;
	
};

class IntentManager {
	private:
	template<typename T>
	inline void internalSubscribe(std::string && name, const IntentCallback<T> && handler) noexcept {
		for (auto & runners : mHandlers) {
			if (runners.first == typeid(T)) {
				reinterpret_cast<IntentRunner<T>*>(runners.second.get())->subscribe(std::move(name), std::move(handler));
				return;
			}
		}
		auto runner = std::make_shared<IntentRunner<T>>();
		mHandlers.emplace_back(typeid(T), runner);
		runner->subscribe(std::move(name), std::move(handler));
	}
	
	public:
	IntentManager() = default;

	template<typename T>
	void subscribe(std::string name, const IntentCallback<T> handler) noexcept {
		internalSubscribe(std::move(name), std::move(handler));
	}
	
	template<typename T>
	void subscribe(const IntentCallback<T> handler) noexcept {
		internalSubscribe(std::string("N/A"), std::move(handler));
	}
	
	template<typename T>
	unsigned int broadcast(T && arg) noexcept {
		for (auto & runners : mHandlers) {
			if (runners.first == typeid(T)) {
				return reinterpret_cast<IntentRunner<T>*>(runners.second.get())->broadcast(mExecutionQueue, std::forward<T>(arg));
			}
		}
#ifdef DEBUG_INTENT_MANAGER_NO_SUBSCRIBERS
		Log::warn("No matching subscribers for intent type: %s", typeid(T).name());
#endif
		return 0;
	}
	
	void runUntilEmpty() {
		IntentCallbackCompiled operation;
		while (!mExecutionQueue.empty()) {
			if (mExecutionQueue.poll(operation)) {
				operation();
			}
		}
	}
	
	bool run() {
		IntentCallbackCompiled operation;
		if (mExecutionQueue.take(operation, [this](){return !static_cast<bool>(mRunning);})) {
			operation();
			return true;
		}
		return false;
	}
	
	void start() {
		mRunning = true;
	}
	
	void stop() {
		mRunning = false;
		mExecutionQueue.interruptBlocking();
	}
	
	void printIntentTiming() const {
		size_t maxName = 1;
		std::vector<std::tuple<std::string, std::string, uint64_t>> records;
		for (auto & handler : mHandlers) {
			for (auto & timing : handler.second->getIntentTiming()) {
				if (timing.first.first.length() + timing.first.second.length() + 1 >= maxName)
					maxName = timing.first.first.length() + timing.first.second.length() + 1;
				records.emplace_back(timing.first.first, timing.first.second, timing.second);
			}
		}
		std::sort(records.begin(), records.end(), [](const auto & a, const auto & b) {
			if (std::get<2>(a) == std::get<2>(b)) {
				if (std::get<1>(a) == std::get<1>(b))
					return std::get<0>(a) < std::get<0>(b);
				return std::get<1>(a) < std::get<1>(b);
			}
			return std::get<2>(a) > std::get<2>(b);
		});
		Log::data("Intent Timing:");
		for (auto & record : records) {
			const auto intentName = std::get<0>(record);
			const auto serviceName = std::get<1>(record);
			const auto intentTime = std::get<2>(record);
			const auto nameLength = maxName - intentName.length() - 1;
			const auto formatString = "        %s %-"+std::to_string(nameLength)+"s   %.3fus";
			Log::data(formatString.c_str(), intentName.c_str(), serviceName.c_str(), intentTime / 1000.0);
		}
	}
	
	private:
	std::vector<std::pair<std::type_index, std::shared_ptr<IntentManagerHelper::GenericIntentRunner>>> mHandlers;
	LinkedBlockingQueue<IntentCallbackCompiled> mExecutionQueue;
	std::atomic_bool mRunning{true};
	
};

} // namespace jlcommon
