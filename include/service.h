//
// Created by Josh Larson on 10/16/19.
//

#pragma once

#include <string>
#include <utility>

namespace jlcommon {

class Service {
	public:
	virtual ~Service() = default;
	
	virtual bool initialize() { return true; }
	
	virtual bool start() { return true; }
	
	virtual bool isOperational() const noexcept { return true; }
	
	virtual bool stop() { return true; }
	
	virtual bool terminate() { return true; }
	
	virtual std::string name() const { return "Service"; }
	
	inline std::shared_ptr<IntentManager> getIntentManager() const noexcept { return mIntentManager; }
	
	virtual void setIntentManager(std::shared_ptr<IntentManager> intentManager) noexcept { this->mIntentManager = std::move(intentManager); }
	
	private:
	std::shared_ptr<IntentManager> mIntentManager;
	
};

} // namespace jlcommon
