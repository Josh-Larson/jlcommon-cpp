//
// Created by Josh Larson on 10/16/19.
//

#pragma once

#include <service.h>
#include <log.h>

#include <vector>
#include <memory>
#include <iostream>
#include <functional>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <string>

namespace jlcommon {

class Manager : public Service {
	public:
	~Manager() override = default;
	
	void addChild(std::unique_ptr<Service> service) {
		services.emplace_back(std::move(service));
	}
	
	inline void forEachChild(const std::function<void(const std::shared_ptr<Service> &)>& handler) {
		for (const auto & service : services) {
			handler(service);
		}
	}
	
	/**
	 * Initializes and starts all services, waiting for any to report that they are no longer operational or the continuePredicate to return false, with a caller-determined periodic sleep.
	 * @return TRUE if all services were started successfully, FALSE otherwise
	 */
	template<typename _Rep, typename _Period>
	bool startRunStop(std::chrono::duration<_Rep, _Period> periodicSleep, const std::function<bool()> & continuePredicate) {
		if (!initialize())
			return false;
		if (!start())
			return false;
		while (continuePredicate() && isOperational()) {
			std::this_thread::sleep_for(periodicSleep);
		}
		stop();
		terminate();
		return true;
	}
	
	/**
	 * Initializes and starts all services, waiting for any to report that they are no longer operational, with a periodic sleep of 100ms.
	 * @return TRUE if all services were started successfully, FALSE otherwise
	 */
	bool startRunStop() noexcept {
		using namespace std::chrono_literals;
		return startRunStop(100ms, []() -> bool { return true; });
	}
	
	bool initialize() noexcept override {
		for (auto & s : services) {
			try {
				if (s->initialize()) {
					initializedServices.emplace_back(s);
					continue;
				}
				
				Log::error("Failed to initialize service: %s\n", s->name().c_str());
			} catch (const std::exception & e) {
				Log::error("Failed to initialize service: %s due to exception: %s\n", s->name().c_str(), e.what());
			} catch (const std::string & e) {
				Log::error("Failed to initialize service: %s due to exception: %s\n", s->name().c_str(), e.c_str());
			} catch (const char * e) {
				Log::error("Failed to initialize service: %s due to exception: %s\n", s->name().c_str(), e);
			} catch (...) {
				Log::error("Failed to initialize service: %s due to an unknown exception\n", s->name().c_str());
			}
			terminate();
		}
		return true;
	}
	
	bool start() noexcept override {
		for (auto & s : services) {
			try {
				if (s->start()) {
					startedServices.emplace_back(s);
					continue;
				}
				
				Log::error("Failed to start service: %s\n", s->name().c_str());
			} catch (const std::exception & e) {
				Log::error("Failed to start service: %s due to exception: %s\n", s->name().c_str(), e.what());
			} catch (const std::string & e) {
				Log::error("Failed to start service: %s due to exception: %s\n", s->name().c_str(), e.c_str());
			} catch (const char * e) {
				Log::error("Failed to start service: %s due to exception: %s\n", s->name().c_str(), e);
			} catch (...) {
				Log::error("Failed to start service: %s due to an unknown exception\n", s->name().c_str());
			}
			stop();
			terminate();
		}
		return true;
	}
	
	bool isOperational() const noexcept override {
		for (auto & s : startedServices) {
			if (!s->isOperational())
				return false;
		}
		return true;
	}
	
	bool stop() noexcept override {
		for (auto & s : startedServices) {
			try {
				if (s->stop())
					continue;
				
				Log::error("Failed to stop service: %s\n", s->name().c_str());
			} catch (const std::exception & e) {
				Log::error("Failed to stop service: %s due to exception: %s\n", s->name().c_str(), e.what());
			} catch (const std::string & e) {
				Log::error("Failed to stop service: %s due to exception: %s\n", s->name().c_str(), e.c_str());
			} catch (const char * e) {
				Log::error("Failed to stop service: %s due to exception: %s\n", s->name().c_str(), e);
			} catch (...) {
				Log::error("Failed to stop service: %s due to an unknown exception\n", s->name().c_str());
			}
		}
		startedServices.clear();
		return true;
	}
	
	bool terminate() noexcept override {
		for (auto & s : initializedServices) {
			try {
				if (s->terminate())
					continue;
				Log::error("Failed to terminate service: %s\n", s->name().c_str());
			} catch (const std::exception & e) {
				Log::error("Failed to terminate service: %s due to exception: %s\n", s->name().c_str(), e.what());
			} catch (const std::string & e) {
				Log::error("Failed to terminate service: %s due to exception: %s\n", s->name().c_str(), e.c_str());
			} catch (const char * e) {
				Log::error("Failed to terminate service: %s due to exception: %s\n", s->name().c_str(), e);
			} catch (...) {
				Log::error("Failed to terminate service: %s due to an unknown exception\n", s->name().c_str());
			}
		}
		initializedServices.clear();
		return true;
	}
	
	std::string name() const override {
		return "Manager";
	}
	
	void setIntentManager(std::shared_ptr<IntentManager> intentManager) noexcept override {
		Service::setIntentManager(intentManager);
		for (auto & s : initializedServices) {
			s->setIntentManager(intentManager);
		}
	}
	
	private:
	std::vector<std::shared_ptr<Service>> services;
	std::vector<std::shared_ptr<Service>> initializedServices;
	std::vector<std::shared_ptr<Service>> startedServices;
	
};

} // namespace jlcommon
