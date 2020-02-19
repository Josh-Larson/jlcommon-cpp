//
// Created by Josh Larson on 10/16/19.
//

#pragma once

#include "intent_manager.h"

#include <string>
#include <utility>
#include <memory>

namespace jlcommon {

class Service {
	public:
	virtual ~Service() = default;
	
	virtual bool initialize() { return true; }
	
	virtual bool start() { return true; }
	
	[[nodiscard]] virtual bool isOperational() const noexcept { return true; }
	
	virtual bool stop() { return true; }
	
	virtual bool terminate() { return true; }
	
	[[nodiscard]] virtual std::string name() const noexcept { return "Service"; }
	
	[[nodiscard]] inline std::shared_ptr<IntentManager> getIntentManager() const noexcept { return mIntentManager; }
	
	virtual void setIntentManager(std::shared_ptr<IntentManager> intentManager) noexcept { this->mIntentManager = std::move(intentManager); }
	
	protected:
	template<typename Intent>
	inline void subscribe(const std::shared_ptr<IntentManager>& intentManager, IntentCallback<Intent> && handler) {
		intentManager->subscribe<Intent>(name(), handler);
	}
	
	template<typename Intent>
	inline void subscribe(const std::shared_ptr<IntentManager>& intentManager, const IntentCallback<Intent> & handler) {
		intentManager->subscribe<Intent>(name(), handler);
	}
	
	template<typename Intent, typename ServiceName>
	inline void subscribe(const std::shared_ptr<IntentManager>& intentManager, void(ServiceName::*function)(const Intent &)) {
		intentManager->subscribe<Intent>(name(), std::bind(function, static_cast<ServiceName*>(this), std::placeholders::_1));
	}
	
	private:
	std::shared_ptr<IntentManager> mIntentManager;
	
};

} // namespace jlcommon
