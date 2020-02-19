#pragma once
#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include <utility>
#include <vector>
#include <queue>

namespace jlcommon {

struct QueueException : public std::exception {
	const char * mStr;
	explicit QueueException(const char * s) : mStr(s) {}
	~QueueException() noexcept override = default; // Updated
	const char* what() const noexcept override { return mStr; }
};

template <typename T>
class BlockingQueue {
	public:
	/*
	 * Getters
	 */
	[[nodiscard]] int size() const noexcept {
		return implSize();
	}
	
	[[nodiscard]] bool empty() const noexcept {
		return implSize() <= 0;
	}
	
	/*
	 * Throws Exception
	 */
	
	void add(const T & item) { internalAdd(std::move(item)); }
	void add(T && item) { internalAdd(std::move(item)); }
	
	T remove() {
		mLock.lock();
		if (implSize() == 0) {
			mLock.unlock();
			throw QueueException("Empty Queue");
		}
		T ret;
		implPeek(ret);
		implPoll();
		mLock.unlock();
		return ret;
	}
	
	T element() {
		mLock.lock();
		if (implSize() == 0) {
			mLock.unlock();
			throw QueueException("Empty Queue");
		}
		T ret;
		implPeek(ret);
		mLock.unlock();
		return ret;
	}
	
	/*
	 * Special Value
	 */
	
	bool offer(const T & item) { internalAdd(std::move(item)); return true; }
	bool offer(T && item) { internalAdd(std::move(item)); return true; }
	
	T poll() noexcept {
		mLock.lock();
		if (implSize() == 0) {
			mLock.unlock();
			return nullptr;
		}
		T ret;
		implPeek(ret);
		implPoll();
		mLock.unlock();
		return ret;
	}
	
	bool poll(T & container) noexcept {
		mLock.lock();
		const bool hasElement = implSize() > 0;
		if (hasElement) {
			implPeek(container);
			implPoll();
		}
		mLock.unlock();
		return hasElement;
	}
	
	T peek() noexcept {
		mLock.lock();
		if (implSize() == 0) {
			mLock.unlock();
			return nullptr;
		}
		T ret;
		implPeek(ret);
		mLock.unlock();
		return ret;
	}
	
	/*
	 * Blocks
	 */
	
	void put(const T & item) { internalAdd(std::move(item)); }
	void put(T && item) { internalAdd(std::move(item)); }
	
	bool take(T & container, const std::function<bool()>& stopBlocking) noexcept {
		std::unique_lock<std::mutex> lk(mLock);
		if (mAllowBlocking && implSize() == 0 && !stopBlocking())
			mCondition.wait(lk, [this, &stopBlocking]{return !mAllowBlocking || implSize() != 0 || stopBlocking();});
		
		if (implSize() == 0)
			return false;
		implPeek(container);
		implPoll();
		return true;
	}
	
	T take(const std::function<bool()>& stopBlocking) {
		T ret;
		if (take(ret, stopBlocking))
			return ret;
		throw QueueException("Empty Queue");
	}
	
	T take() {
		return take([](){return false;});
	}
	
	void interruptBlocking() noexcept {
		mCondition.notify_all();
	}
	
	void setAllowBlocking(bool allowBlocking) noexcept {
		mLock.lock();
		mAllowBlocking = allowBlocking;
		mLock.unlock();
		mCondition.notify_all();
	}
	
	protected:
	BlockingQueue() :
			mLock(),
			mCondition(),
			mAllowBlocking(true) { }
	
	protected:
	virtual void implAdd(T && item) = 0;
	virtual void implAdd(const T & item) = 0;
	virtual void implPeek(T & item) = 0;
	virtual void implPoll() = 0;
	[[nodiscard]] virtual size_t implSize() const = 0;
	
	private:
	std::mutex mLock;
	std::condition_variable mCondition;
	bool mAllowBlocking;
	
	inline void internalAdd(const T & item) {
		mLock.lock();
		implAdd(item);
		mCondition.notify_one();
		mLock.unlock();
	}
	
	inline void internalAdd(T && item) {
		mLock.lock();
		implAdd(std::move(item));
		mCondition.notify_one();
		mLock.unlock();
	}
	
};

template <typename T>
class LinkedBlockingQueue final : public BlockingQueue<T> {
	public:
	LinkedBlockingQueue() : BlockingQueue<T>(), mData() { }
	
	protected:
	void implAdd(T && item) final { mData.emplace_back(std::move(item)); };
	void implAdd(const T & item) final { mData.emplace_back(item); };
	void implPeek(T & item) final { item = std::move(mData.front()); };
	void implPoll() final { mData.pop_front(); };
	[[nodiscard]] size_t implSize() const final { return  mData.size(); };
	
	private:
	std::list<T> mData;
};

template <typename T>
class ArrayBlockingQueue final : public BlockingQueue<T> {
	public:
	ArrayBlockingQueue() : BlockingQueue<T>(), mData() {}
	
	protected:
	void implAdd(T && item) final { mData.emplace_back(std::move(item)); };
	void implAdd(const T & item) final { mData.emplace_back(item); };
	void implPeek(T & item) final { item = std::move(mData.front()); };
	void implPoll() final { mData.erase(mData.begin()); };
	[[nodiscard]] size_t implSize() const final { return  mData.size(); };
	
	private:
	std::vector<T> mData;
};

template <typename T>
class PriorityBlockingQueue final : public BlockingQueue<T> {
	public:
	PriorityBlockingQueue() : BlockingQueue<T>(), mData() { }
	
	protected:
	void implAdd(T && item) final { mData.emplace(std::move(item)); };
	void implAdd(const T & item) final { mData.emplace(item); };
	void implPeek(T & item) final { item = std::move(mData.top()); };
	void implPoll() final { mData.pop(); };
	[[nodiscard]] size_t implSize() const final { return  mData.size(); };
	
	private:
	std::priority_queue<T> mData;
};

} // namespace jlcommon

