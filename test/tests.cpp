#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
#include <jlcommon.h>

#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <cmath>

#define WAIT_FOR_TRUE(test) for (int i = 0; i < 10000 && !test; i++) {usleep(100);}
#define LOG_TIME_OFFSET 23

TEST(LogTest, BasicPrint) {
	char data[256];
	jlcommon::Log::addWrapper([&data](auto str) mutable {strncpy(data, str, 256);});
	
	jlcommon::Log::trace("Hello World: %d", 1);
	ASSERT_STREQ(" T: Hello World: 1\n", data+LOG_TIME_OFFSET);
	
	jlcommon::Log::data("Hello World: %s", "TEST");
	ASSERT_STREQ(" D: Hello World: TEST\n", data+LOG_TIME_OFFSET);
	
	jlcommon::Log::info("Hello World: %.2f", 3.449);
	ASSERT_STREQ(" I: Hello World: 3.45\n", data+LOG_TIME_OFFSET);
	
	jlcommon::Log::warn("Hello World: %ld", 1024L);
	ASSERT_STREQ(" W: Hello World: 1024\n", data+LOG_TIME_OFFSET);
	
	jlcommon::Log::error("Hello World: %03d", 1);
	ASSERT_STREQ(" E: Hello World: 001\n", data+LOG_TIME_OFFSET);
	
	jlcommon::Log::clearWrappers();
	jlcommon::Log::trace("Testing");
	ASSERT_STREQ(" E: Hello World: 001\n", data+LOG_TIME_OFFSET);
}

template<typename T>
void testBlockingQueue(jlcommon::BlockingQueue<T> * q) {
	// Test Throwing Exception
	q->add("EXC");
	ASSERT_EQ(1, q->size());
	ASSERT_FALSE(q->empty());
	ASSERT_STREQ("EXC", q->element());
	ASSERT_STREQ("EXC", q->remove());
	ASSERT_THROW(q->element(), jlcommon::QueueException);
	ASSERT_THROW(q->remove(), jlcommon::QueueException);
	ASSERT_EQ(0, q->size());
	ASSERT_TRUE(q->empty());
	// Test Special Value
	q->offer("SV");
	ASSERT_EQ(1, q->size());
	ASSERT_FALSE(q->empty());
	ASSERT_STREQ("SV", q->peek());
	ASSERT_STREQ("SV", q->poll());
	ASSERT_STREQ(nullptr, q->peek());
	ASSERT_STREQ(nullptr, q->poll());
	ASSERT_EQ(0, q->size());
	ASSERT_TRUE(q->empty());
	// Test Blocks (No Block)
	q->put("BLK");
	ASSERT_EQ(1, q->size());
	ASSERT_FALSE(q->empty());
	ASSERT_STREQ("BLK", q->take());
	ASSERT_EQ(0, q->size());
	ASSERT_TRUE(q->empty());
	// Test Blocks
	{
		bool success1 = false;
		bool success2 = false;
		bool success = false;
		q->put("BLK");
		std::thread t([&success1, &success2, &success, &q]{
			success1 = (strcmp("BLK", q->take()) == 0);
			success2 = (strcmp("BLK Blocked", q->take()) == 0);
			success = success1 && success2;
		});
		WAIT_FOR_TRUE(success1)
		q->put("BLK Blocked");
		t.join();
		ASSERT_TRUE(success);
	}
}

TEST(BlockingQueueTest, LinkedBlockingQueue) {
	jlcommon::LinkedBlockingQueue<const char *> q;
	testBlockingQueue(&q);
}

TEST(BlockingQueueTest, ArrayBlockingQueue) {
	jlcommon::ArrayBlockingQueue<const char *> q;
	testBlockingQueue(&q);
}

TEST(BlockingQueueTest, PriorityBlockingQueue) {
	jlcommon::PriorityBlockingQueue<const char *> q;
	testBlockingQueue(&q);
}

TEST(ThreadPoolTest, FifoThreadPool) {
	auto threadPool = std::make_unique<jlcommon::FifoThreadPool<std::function<void()>>>(1);
	threadPool->start();
	
	bool test = false;
	threadPool->execute([&test]{test = true;});
	WAIT_FOR_TRUE(test)
	ASSERT_TRUE(test);
	
	int testInc = 0;
	threadPool->execute([&testInc]{testInc = (testInc == 0) ? 1 : 0;});
	threadPool->execute([&testInc]{testInc = (testInc == 1) ? 2 : 0;});
	threadPool->execute([&testInc]{testInc = (testInc == 2) ? 3 : 0;});
	threadPool->execute([&testInc]{testInc = (testInc == 3) ? 4 : 0;});
	threadPool->execute([&testInc]{testInc = (testInc == 4) ? 5 : 0;});
	WAIT_FOR_TRUE((testInc >= 5))
	ASSERT_EQ(5, testInc);
	
	threadPool->stop();
}

TEST(ThreadPoolTest, ScheduledThreadPool_Delayed) {
	auto threadPool = std::make_unique<jlcommon::ScheduledThreadPool<std::function<void()>>>(1);
	threadPool->start();
	
	auto prevExecution = std::chrono::high_resolution_clock::now();
	unsigned long difference = 0;
	threadPool->execute(10, [&prevExecution, &difference]{
		auto cur = std::chrono::high_resolution_clock::now();
		difference = std::chrono::duration_cast<std::chrono::microseconds>(cur - prevExecution).count();
		prevExecution = cur;
	});
	WAIT_FOR_TRUE((difference != 0))
	threadPool->stop();
	ASSERT_LT(std::abs(difference - 10000.0), 1000);
}

TEST(ThreadPoolTest, ScheduledThreadPool_WithFixedRate) {
	auto threadPool = std::make_unique<jlcommon::ScheduledThreadPool<std::function<void()>>>(2);
	threadPool->start();
	
	auto prevExecution = std::chrono::high_resolution_clock::now();
	unsigned long difference = 0;
	unsigned int iterations = 0;
	threadPool->executeWithFixedRate(0, 5, [&prevExecution, &difference, &iterations]{
		auto cur = std::chrono::high_resolution_clock::now();
		difference += abs(std::chrono::duration_cast<std::chrono::microseconds>(cur - prevExecution).count() - 5000);
		iterations++;
		prevExecution = cur;
	});
	usleep(50000);
	threadPool->stop();
	ASSERT_LT((difference / double(iterations)), 1000);
	ASSERT_GE(iterations, 10);
}

TEST(ThreadPoolTest, ScheduledThreadPool_WithFixedDelay) {
	auto threadPool = std::make_unique<jlcommon::ScheduledThreadPool<std::function<void()>>>(1);
	threadPool->start();
	
	auto prevExecution = std::chrono::high_resolution_clock::now();
	unsigned long difference = 0;
	unsigned int iterations = 0;
	threadPool->executeWithFixedDelay(6, 5, [&prevExecution, &difference, &iterations]{
		auto cur = std::chrono::high_resolution_clock::now();
		difference += abs(std::chrono::duration_cast<std::chrono::microseconds>(cur - prevExecution).count() - 6000);
		iterations++;
		prevExecution = std::chrono::high_resolution_clock::now();
		usleep(1000);
	});
	usleep(50000);
	threadPool->stop();
	ASSERT_LT((difference / double(iterations)), 1000);
	ASSERT_GE(iterations, 8);
}

class GenericTask {
	public:
	GenericTask() = default;
	GenericTask(const GenericTask & task) { ++copies; }
	GenericTask(GenericTask && task) noexcept { ++moves; }
	GenericTask & operator=(const GenericTask & task) { ++copies; return *this; }
	GenericTask & operator=(GenericTask && task) noexcept { ++moves; return *this; }
	
	void operator()() {}
	
	static std::atomic_int copies;
	static std::atomic_int moves;
};

std::atomic_int GenericTask::copies = 0;
std::atomic_int GenericTask::moves = 0;

TEST(ThreadPoolTest, ScheduledThreadPool_CopyMove) {
	auto threadPool = std::make_unique<jlcommon::ScheduledThreadPool<GenericTask>>(1);
	threadPool->start();
	
	threadPool->executeWithFixedDelay(6, 5, GenericTask{});
	usleep(50000);
	threadPool->stop();
	ASSERT_EQ(1, static_cast<int>(GenericTask::copies)); // we allow one initial copy
	ASSERT_GT(static_cast<int>(GenericTask::moves), 0);
}

TEST(ThreadPoolTest, ScheduledThreadPool_Interrupt) {
	auto threadPool = std::make_unique<jlcommon::ScheduledThreadPool<std::function<void()>>>(1);
	threadPool->start();
	
	auto prevExecution = std::chrono::high_resolution_clock::now();
	threadPool->execute(10000, []{});
	usleep(100000);
	threadPool->stop();
	unsigned long int difference = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - prevExecution).count();
	ASSERT_LT(difference, 1000);
}

TEST(InetAddress, Equality) {
	jlcommon::InetAddress addr = jlcommon::InetAddress::getLoopbackAddress();
	ASSERT_EQ(addr.getAddressString(), jlcommon::InetAddress::getByName(addr.getAddressString().c_str()).getAddressString());
	
	jlcommon::InetAddress localhost = jlcommon::InetAddress::getLocalHost();
	ASSERT_EQ(localhost.getAddressString(), jlcommon::InetAddress::getByName(localhost.getAddressString().c_str()).getAddressString());
}

TEST(UdpServer, StandardCommunication) {
	jlcommon::UdpServer client(0);
	jlcommon::UdpServer server(25434);
	
	char message[128];
	message[0] = 0;
	
	jlcommon::InetAddress addr = jlcommon::InetAddress::getLocalHost(server.getPort());
	server.setHandler([&message](const char * data, size_t len){strncpy(message, data, len < 128 ? len : 128);});
	usleep(10000);
	client.send(addr, "Hello World", 12);
	WAIT_FOR_TRUE((strlen(message) > 0))
	ASSERT_STREQ("Hello World", message);
}

class Point {
	public:
	explicit Point(int x): x(x) {}
	int x;
};

class PointIntent {
	public:
	explicit PointIntent(const Point p): p(p) {}
	const Point p;
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-exception-baseclass" // We want these exceptions in the test case to ensure that it cannot crash
TEST(TestIntentManager, TestSubscribeBroadcast) {
	int exceptions = 0;
	jlcommon::Log::addWrapper([&exceptions](auto str) { exceptions++; });
	auto im = jlcommon::IntentManager{};
	bool testFlag = false;
	int value = 0;
	im.subscribe<PointIntent>("exception",    [&](const auto & pi) { testFlag = true; throw std::exception(); });
	im.subscribe<PointIntent>("string",       [&](const auto & pi) { testFlag = true; throw std::string("Testing String"); });
	im.subscribe<PointIntent>("const char *", [&](const auto & pi) { testFlag = true; throw "Testing const char *"; });
	im.subscribe<PointIntent>("other: int",   [&](const auto & pi) { testFlag = true; throw 2; });
	im.subscribe<PointIntent>("no issue",     [&](const auto & pi) { value = pi.p.x; });
	im.broadcast(PointIntent{Point{1}});
	im.runUntilEmpty();
	ASSERT_TRUE(testFlag);
	ASSERT_EQ(1, value);
	ASSERT_EQ(4, exceptions);
	jlcommon::Log::clearWrappers();
	jlcommon::Log::addWrapper([](auto str) { std::cout << str; });
	im.printIntentTiming();
	jlcommon::Log::clearWrappers();
}
#pragma clang diagnostic pop

TEST(TestIntentManager, TestStartStop) {
	auto im = jlcommon::IntentManager{};
	int value = 0;
	im.subscribe<PointIntent>([&](const auto & pi) { value = pi.p.x; });
	im.broadcast(PointIntent{Point{1}});
	im.stop();
	while (im.run());
	ASSERT_EQ(1, value);
	
	im.start();
	im.broadcast(PointIntent{Point{2}});
	im.stop();
	while (im.run());
	ASSERT_EQ(2, value);
}

class CustomService1 final : public jlcommon::Service {
	public:
	static volatile bool initialized;
	
	bool initialize() override {
		initialized = true;
		return true;
	}
	
	[[nodiscard]] bool isOperational() const noexcept override {
		return !initialized;
	}
	
	bool terminate() override {
		initialized = false;
		return true;
	}
};

volatile bool CustomService1::initialized = false;

	class CustomManager final : public jlcommon::Manager<jlcommon::Service> {
	public:
	CustomManager() {
		addChild(std::make_unique<CustomService1>());
	}
	
};

TEST(TestServiceManager, TestRecursiveInitialize) {
	auto manager = CustomManager{};
	ASSERT_FALSE(CustomService1::initialized);
	ASSERT_TRUE(manager.initialize());
	ASSERT_TRUE(CustomService1::initialized);
	ASSERT_TRUE(manager.start());
	ASSERT_TRUE(CustomService1::initialized);
	ASSERT_TRUE(manager.stop());
	ASSERT_TRUE(CustomService1::initialized);
	ASSERT_TRUE(manager.terminate());
	ASSERT_FALSE(CustomService1::initialized);
}

TEST(TestServiceManager, TestStartRunStop) {
	using namespace std::chrono_literals;
	auto manager = CustomManager{};
	ASSERT_FALSE(CustomService1::initialized);
	ASSERT_TRUE(manager.startRunStop());
	
	ASSERT_FALSE(CustomService1::initialized);
	bool initialized = false;
	ASSERT_TRUE(manager.startRunStop(100ms, [&initialized]() -> bool { initialized = CustomService1::initialized; return true; }));
	ASSERT_TRUE(initialized);
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

#pragma clang diagnostic pop
