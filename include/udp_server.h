#pragma once
#include "inet_address.h"
#include <string>
#include <thread>
#include <functional>
#include <utility>
#include <memory>

namespace jlcommon {

using UdpServerHandler = std::function<void(const char * buffer, size_t len)>;

class UdpServer {
	public:
	inline UdpServer() : UdpServer(0) {}
	explicit inline UdpServer(int port) : UdpServer(nullptr, port) {}
	inline UdpServer(const std::string& host, int port) : UdpServer(host.c_str(), port) {}
	inline UdpServer(const char * host, int port) : UdpServer(InetAddress::getByName(host, port)) {}
	explicit UdpServer(const InetAddress& bindAddress) :
			mBindAddress   {new InetAddress(bindAddress)},
			mReadThread    {},
			mReadHandler   {[](auto buf, auto len){ (void)buf; (void)len; }},
			mSockfd        {-1},
			mReadBuffer    {new char[1500]},
			mReadBufferSize{1500} {
		start();
	}
	~UdpServer();
	
	inline void setHandler(UdpServerHandler handler) {
		mReadHandler = std::move(handler);
	}
	
	inline auto getPort() {
		return mBindAddress->getPort();
	}
	
	inline auto getFd() {
		return mSockfd;
	}
	
	inline int send(InetAddress & addr, const void * buffer, size_t len) { return send(addr, buffer, len, 0); }
	int send(InetAddress & addr, const void * buffer, size_t len, int flags);
	void setBroadcast(int enabled);
	
	private:
	InetAddress * mBindAddress;
	std::unique_ptr<std::thread> mReadThread;
	UdpServerHandler mReadHandler;
	int mSockfd;
	char * mReadBuffer;
	size_t mReadBufferSize;
	
	void start();
	void stop();
	void read();
};

} // namespace jlcommon

