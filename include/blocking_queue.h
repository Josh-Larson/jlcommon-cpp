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
	int size() const noexcept {
		return mSize();
	}
	
	bool empty() const noexcept {
		return mSize() <= 0;
	}
	
	/*
	 * Throws Exception
	 */
	
	void add(const T & item) { internalAdd(std::move(item)); }
	void add(T && item) { internalAdd(std::move(item)); }
	
	T remove() {
		mLock.lock();
		if (mSize() == 0) {
			mLock.unlock();
			throw QueueException("Empty Queue");
		}
		T ret;
		mPoll(ret);
		mLock.unlock();
		return ret;
	}
	
	T element() {
		mLock.lock();
		if (mSize() == 0) {
			mLock.unlock();
			throw QueueException("Empty Queue");
		}
		T ret;
		mPeek(ret);
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
		if (mSize() == 0) {
			mLock.unlock();
			return nullptr;
		}
		T ret;
		mPoll(ret);
		mLock.unlock();
		return ret;
	}
	
	bool poll(T & container) noexcept {
		mLock.lock();
		if (mSize() == 0) {
			mLock.unlock();
			return false;
		}
		mPoll(container);
		mLock.unlock();
		return true;
	}
	
	T peek() noexcept {
		mLock.lock();
		if (mSize() == 0) {
			mLock.unlock();
			return nullptr;
		}
		T ret;
		mPeek(ret);
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
		if (mAllowBlocking && mSize() == 0 && !stopBlocking())
			mCondition.wait(lk, [this, &stopBlocking]{return !mAllowBlocking || mSize() != 0 || stopBlocking();});
		
		if (mSize() == 0)
			return false;
		mPoll(container);
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
	BlockingQueue(std::function<void(T&&)> add, std::function<void(T&)> peek, std::function<void(T&)> poll, std::function<int()> size) :
			mLock(),
			mCondition(),
			mAdd(std::move(add)),
			mPeek(std::move(peek)),
			mPoll(std::move(poll)),
			mSize(std::move(size)),
			mAllowBlocking(true) {
		
	}
	
	private:
	std::mutex mLock;
	std::condition_variable mCondition;
	const std::function<void(T)> mAdd;
	const std::function<void(T&)> mPeek;
	const std::function<void(T&)> mPoll;
	const std::function<int()> mSize;
	bool mAllowBlocking;
	
	void internalAdd(const T && item) {
		mLock.lock();
		mAdd(std::move(item));
		mCondition.notify_one();
		mLock.unlock();
	}
	
	void internalAdd(T && item) {
		mLock.lock();
		mAdd(std::move(item));
		mCondition.notify_one();
		mLock.unlock();
	}
	
};

template <typename T>
class LinkedBlockingQueue : public BlockingQueue<T> {
	public:
	LinkedBlockingQueue() :
			BlockingQueue<T>([this](T && item){mData.emplace_back(std::move(item));},
							 [this](T & item){ item = std::move(mData.front()); },
							 [this](T & item){ item = std::move(mData.front()); mData.pop_front(); },
							 [this](){return mData.size();}),
			mData() {
		
	}
	
	LinkedBlockingQueue(const LinkedBlockingQueue & rhs) = delete;
	LinkedBlockingQueue(LinkedBlockingQueue & rhs) noexcept :
			BlockingQueue<T>([this](T && item){mData.emplace_back(std::move(item));},
			                 [this](T & item){ item = std::move(mData.front()); },
			                 [this](T & item){ item = std::move(mData.front()); mData.pop_front(); },
			                 [this](){return mData.size();}),
			mData(rhs.mData) { }
	LinkedBlockingQueue & operator =(const LinkedBlockingQueue & rhs) noexcept {
		mData = rhs.mData;
	}
	
	LinkedBlockingQueue(const LinkedBlockingQueue && rhs) = delete;
	LinkedBlockingQueue(LinkedBlockingQueue && rhs) noexcept :
			BlockingQueue<T>([this](T && item){mData.emplace_back(std::move(item));},
			                 [this](T & item){ item = std::move(mData.front()); },
			                 [this](T & item){ item = std::move(mData.front()); mData.pop_front(); },
			                 [this](){return mData.size();}),
			mData(std::move(rhs.mData)) { }
	LinkedBlockingQueue & operator =(LinkedBlockingQueue && rhs) noexcept {
		mData = std::move(rhs.mData);
	}
	
	private:
	std::list<T> mData;
};

template <typename T>
class ArrayBlockingQueue : public BlockingQueue<T> {
	public:
	ArrayBlockingQueue() :
			BlockingQueue<T>([this](T && item){mData.push_back(item);},
			                 [this](T & item){ item = std::move(mData.front()); },
			                 [this](T & item){ item = mData.front(); mData.erase(mData.begin()); },
			                 [this](){return mData.size();}),
			mData() {
		
	}
	
	private:
	std::vector<T> mData;
};

template <typename T>
class PriorityBlockingQueue : public BlockingQueue<T> {
	public:
	PriorityBlockingQueue() :
			BlockingQueue<T>([this](T && item){mData.push(item);},
			                 [this](T & item){ item = std::move(mData.top()); },
			                 [this](T & item){ item = mData.top(); mData.pop(); },
			                 [this](){return mData.size();}),
			mData() {
		
	}
	
	private:
	std::priority_queue<T> mData;
};

} // namespace jlcommon

