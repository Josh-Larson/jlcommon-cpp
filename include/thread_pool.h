#pragma once
#include "blocking_queue.h"

#include <vector>		// std::vector
#include <utility>		// std::pair
#include <thread>
#include <atomic>
#include <cstdint>
#include <queue>
#include <chrono>

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
	virtual void onCompleted(T & task) noexcept { }
	virtual bool getTask(T & task) noexcept = 0;
	
	private:
	std::thread ** mThreads;
	std::atomic_flag mCriticalSection;
	std::atomic_bool mStarted;
	std::atomic_uint mThreadsStarted;
	unsigned int mThreadCount;
	
	void runWorker() {
		T task;
		mThreadsStarted.fetch_add(1);
		while (mStarted) {
			if (getTask(task)) {
				task();
				onCompleted(task);
			}
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
	bool getTask(T & task) noexcept override {
		return mQueue.take(task, [](){ return false; });
	}
	
	private:
	LinkedBlockingQueue<T> mQueue;
};

template<typename T>
class SchedulingInfo {
	public:
	SchedulingInfo(std::chrono::steady_clock::time_point nextExecution, const std::chrono::duration<unsigned long int, std::ratio<1, 1000000>> delay, const T & task, const unsigned char mode) :
			nextExecution(nextExecution),
			delay(delay),
			task(std::move(task)),
			mode(mode) { }
	SchedulingInfo() :
			nextExecution(std::chrono::steady_clock::now()),
			delay(std::chrono::microseconds(0)),
			task(T{}),
			mode(0) { }
	SchedulingInfo(const SchedulingInfo<T> & p) : nextExecution(p.nextExecution), delay(p.delay), task(p.task), mode(p.mode) { }
	SchedulingInfo(SchedulingInfo<T> & p) : nextExecution(p.nextExecution), delay(p.delay), task(p.task), mode(p.mode) { }
	SchedulingInfo(const SchedulingInfo<T> && p) noexcept : nextExecution(p.nextExecution), delay(p.delay), task(p.task), mode(p.mode) { }
	SchedulingInfo(SchedulingInfo<T> && p) noexcept : nextExecution(p.nextExecution), delay(p.delay), task(p.task), mode(p.mode) { }
	SchedulingInfo<T>& operator=(const SchedulingInfo<T> & p) {
		nextExecution = p.nextExecution;
		delay = p.delay;
		task = p.task;
		mode = p.mode;
	}
	SchedulingInfo<T>& operator=(SchedulingInfo<T> && p) noexcept {
		nextExecution = std::move(p.nextExecution);
		delay = std::move(p.delay);
		task = std::move(p.task);
		mode = std::move(p.mode);
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
	
	void execute(unsigned long delay, const T task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		mLock.lock();
		mQueue.emplace(now + std::chrono::microseconds(delay * 1000), std::chrono::microseconds(0), task, 0);
		mLock.unlock();
		mCondition.notify_all();
	}
	
	void executeWithFixedRate(unsigned long initialDelay, unsigned long periodicDelay, const T task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		mLock.lock();
		mQueue.emplace(now + std::chrono::microseconds(initialDelay * 1000), std::chrono::microseconds(periodicDelay * 1000), task, 1);
		mLock.unlock();
		mCondition.notify_all();
	}
	
	void executeWithFixedDelay(unsigned long initialDelay, unsigned long periodicDelay, const T task) {
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		
		mLock.lock();
		mQueue.emplace(now + std::chrono::microseconds(initialDelay * 1000), std::chrono::microseconds(periodicDelay * 1000), task, 2);
		mLock.unlock();
		mCondition.notify_all();
	}
	
	protected:
	bool getTask(SchedulingInfo<T> & task) noexcept override {
		std::unique_lock<std::mutex> lk(mLock);
		if (mQueue.empty())
			mCondition.wait(lk, [this]{return !mRunning || !mQueue.empty(); });
		if (!mRunning || mQueue.empty()) // mRunning should also be false
			return false;
		
		// Wait for task to be ready
		do {
			const SchedulingInfo<T> &t = mQueue.top();
			if (t.nextExecution < std::chrono::steady_clock::now()) {
				task = std::move(const_cast<SchedulingInfo<T>&>(t)); // Const-ness is set by the priority queue, not the thread pool
				break;
			}
			mCondition.wait_until(lk, t.nextExecution, [this]{return !mRunning; });
			if (mQueue.empty())
				mCondition.wait(lk, [this]{return !mRunning || !mQueue.empty(); });
			if (!mRunning || mQueue.empty())
				return false;
		} while (true);
		mQueue.pop();
		
		lk.unlock();
		mCondition.notify_one();
		return true;
	}
	
	void onCompleted(SchedulingInfo<T> & task) noexcept override {
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
		mQueue.emplace(std::move(task));
		mLock.unlock();
		mCondition.notify_one();
	}
	
	private:
	std::mutex mLock;
	std::condition_variable mCondition;
	std::priority_queue<SchedulingInfo<T>> mQueue;
	bool mRunning;
	
};

} // namespace jlcommon
