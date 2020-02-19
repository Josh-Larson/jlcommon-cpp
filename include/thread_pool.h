#pragma once
#include "blocking_queue.h"

#include <vector>		// std::vector
#include <utility>		// std::pair, std::forward
#include <thread>
#include <atomic>
#include <cstdint>
#include <queue>
#include <chrono>
#include <algorithm>

namespace jlcommon {

template<typename T>
class ThreadPool {
	public:
	explicit ThreadPool(unsigned int nThreads) :
			mThreadCount(nThreads),
			mThreads(nullptr),
			mCriticalSection(false),
			mStarted(false),
			mThreadsStarted(0) { }
	
	virtual ~ThreadPool() {
		stop();
	}
	
	virtual void start() {
		while (!mCriticalSection.test_and_set());
		
		if (mStarted) {
			mCriticalSection.clear();
			return;
		}
		mStarted = true;
		mThreads = new std::thread*[mThreadCount];
		for (unsigned int i = 0; i < mThreadCount; i++) {
			mThreads[i] = new std::thread([this]{runWorker();});
		}
		
		while (mThreadsStarted < mThreadCount) {
			std::this_thread::yield();
		}
		mCriticalSection.clear();
	}
	
	virtual void stop() {
		while (!mCriticalSection.test_and_set());
		
		if (!mStarted) {
			mCriticalSection.clear();
			return;
		}
		mStarted = false;
		while (mThreadsStarted > 0) {
			std::this_thread::yield();
		}
		for (unsigned int i = 0; i < mThreadCount; i++) {
			mThreads[i]->join();
			delete mThreads[i];
		}
		delete [] mThreads;
		
		mCriticalSection.clear();
	}
	
	protected:
	virtual void onCompleted(T && task) noexcept { }
	virtual void runTask() noexcept = 0;
	
	private:
	std::thread ** mThreads;
	std::atomic_flag mCriticalSection;
	std::atomic_bool mStarted;
	std::atomic_uint mThreadsStarted;
	unsigned int mThreadCount;
	
	void runWorker() {
		mThreadsStarted.fetch_add(1);
		while (mStarted) {
			runTask();
		}
		mThreadsStarted.fetch_sub(1);
	}
	
};

template<typename T>
class FifoThreadPool : public ThreadPool<T> {
	public:
	explicit FifoThreadPool(unsigned int nThreads) :
			ThreadPool<T>(nThreads),
			mQueue() {
		
	}
	
	void start() override {
		mQueue.setAllowBlocking(true);
		ThreadPool<T>::start();
	}
	
	void stop() override {
		mQueue.setAllowBlocking(false);
		ThreadPool<T>::stop();
	}
	
	void execute(T task) {
		mQueue.offer(task);
	}
	
	protected:
	void runTask() noexcept override {
		T task;
		if (mQueue.take(task, [](){ return false; })) {
			task();
			this->onCompleted(std::move(task));
		}
	}
	
	private:
	LinkedBlockingQueue<T> mQueue;
};

template<typename T>
class SchedulingInfo {
	public:
	template<typename TF>
	SchedulingInfo(std::chrono::steady_clock::time_point nextExecution, const std::chrono::duration<unsigned long int, std::ratio<1, 1000000>> delay, TF && task, const unsigned char mode) :
			nextExecution(nextExecution),
			delay(delay),
			task(std::forward<TF>(task)),
			mode(mode) { }
	SchedulingInfo() :
			nextExecution(std::chrono::steady_clock::now()),
			delay(std::chrono::microseconds(0)),
			task(T{}),
			mode(0) { }
	SchedulingInfo(const SchedulingInfo<T> & p) : nextExecution(p.nextExecution), delay(p.delay), task(p.task), mode(p.mode) { }
	SchedulingInfo(SchedulingInfo<T> && p) noexcept : nextExecution(p.nextExecution), delay(p.delay), task(std::move(p.task)), mode(p.mode) { }
	SchedulingInfo<T>& operator=(const SchedulingInfo<T> & p) {
		nextExecution = p.nextExecution;
		delay = p.delay;
		task = p.task;
		mode = p.mode;
		return *this;
	}
	SchedulingInfo<T>& operator=(SchedulingInfo<T> && p) noexcept {
		nextExecution = std::move(p.nextExecution);
		delay = std::move(p.delay);
		task = std::move(p.task);
		mode = std::move(p.mode);
		return *this;
	}
	
	std::chrono::steady_clock::time_point nextExecution;
	std::chrono::duration<unsigned long int, std::ratio<1, 1000000>> delay; // microseconds
	T task;
	unsigned char mode; // 0=Once, 1=Fixed Rate, 2=Fixed Delay
	
	void operator()() { task(); }
	bool operator <(const SchedulingInfo & b) const { return nextExecution > b.nextExecution; }
	bool operator <=(const SchedulingInfo & b) const { return nextExecution >= b.nextExecution; }
	bool operator >=(const SchedulingInfo & b) const { return nextExecution <= b.nextExecution; }
	bool operator >(const SchedulingInfo & b) const { return nextExecution < b.nextExecution; }
	bool operator ==(const SchedulingInfo & b) const { return nextExecution == b.nextExecution; }
	bool operator !=(const SchedulingInfo & b) const { return nextExecution != b.nextExecution; }
};

template<typename T>
class ScheduledThreadPool : public ThreadPool<SchedulingInfo<T>> {
	public:
	explicit ScheduledThreadPool(unsigned int nThreads) :
			ThreadPool<SchedulingInfo<T>> (nThreads),
			mLock(),
			mCondition(),
			mQueue(),
			mRunning(false) {
		
	}
	
	virtual void start() {
		mRunning = true;
		ThreadPool<SchedulingInfo<T>>::start();
	}
	
	virtual void stop() {
		mRunning = false;
		mCondition.notify_all();
		ThreadPool<SchedulingInfo<T>>::stop();
	}
	
	template<typename TF>
	void execute(unsigned long delay, TF && task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		const auto info = SchedulingInfo<T>{now + std::chrono::microseconds(delay * 1000), std::chrono::microseconds(0), std::forward<TF>(task), 0};
		mLock.lock();
		mQueue.insert(std::upper_bound(mQueue.begin(), mQueue.end(), info), std::move(info));
		mLock.unlock();
		mCondition.notify_all();
	}
	
	template<typename TF>
	void executeWithFixedRate(unsigned long initialDelay, unsigned long periodicDelay, TF && task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		auto info = SchedulingInfo<T>{now + std::chrono::microseconds(initialDelay * 1000), std::chrono::microseconds(periodicDelay * 1000), std::forward<TF>(task), 1};
		mLock.lock();
		mQueue.insert(std::upper_bound(mQueue.begin(), mQueue.end(), info), std::move(info));
		mLock.unlock();
		mCondition.notify_all();
	}
	
	template<typename TF>
	void executeWithFixedDelay(unsigned long initialDelay, unsigned long periodicDelay, TF && task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		const auto info = SchedulingInfo<T>{now + std::chrono::microseconds(initialDelay * 1000), std::chrono::microseconds(periodicDelay * 1000), std::forward<TF>(task), 2};
		mLock.lock();
		mQueue.emplace(std::upper_bound(mQueue.begin(), mQueue.end(), info), std::move(info));
		mLock.unlock();
		mCondition.notify_all();
	}
	
	protected:
	void runTask() noexcept override {
		std::unique_lock<std::mutex> lk(mLock);
		if (mQueue.empty())
			mCondition.wait(lk, [this]{return !mRunning || !mQueue.empty(); });
		if (!mRunning || mQueue.empty()) // mRunning should also be false
			return;
		
		// Wait for task to be ready
		do {
			if (mQueue.begin()->nextExecution < std::chrono::steady_clock::now()) {
				auto task = std::move(mQueue.front());
				mQueue.erase(mQueue.begin());
				lk.unlock();
				mCondition.notify_one();
				// Run
				task();
				onCompleted(std::move(task));
				return;
			}
			mCondition.wait_until(lk, mQueue.begin()->nextExecution, [this]{return !mRunning; });
			if (mQueue.empty())
				mCondition.wait(lk, [this]{return !mRunning || !mQueue.empty(); });
			if (!mRunning || mQueue.empty())
				return;
		} while (true);
	}
	
	void onCompleted(SchedulingInfo<T> && task) noexcept override {
		switch (task.mode) {
			case 1:
				task.nextExecution += task.delay;
				break;
			case 2:
				task.nextExecution = std::chrono::steady_clock::now() + task.delay;
				break;
			default:
				return;
		}
		
		mLock.lock();
		mQueue.insert(std::upper_bound(mQueue.begin(), mQueue.end(), task), std::move(task));
		mLock.unlock();
		mCondition.notify_one();
	}
	
	private:
	std::mutex mLock;
	std::condition_variable mCondition;
	std::vector<SchedulingInfo<T>> mQueue;
	bool mRunning;
	
};

} // namespace jlcommon
