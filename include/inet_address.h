#pragma once
#include <netinet/in.h>
#include <vector>
#include <string>

namespace jlcommon {

class InetAddress {
	public:
	InetAddress(const InetAddress & addr);
	InetAddress(struct sockaddr * info, size_t infoLen);
	virtual ~InetAddress();
	
	int getVersion();
	int getPort();
	std::string getAddressString();
	const struct sockaddr * getAddress();
	size_t getAddressLength() const;
	
	inline static InetAddress getByName(const char * host) { return getByName(host, 0); }
	inline static InetAddress getLocalHost() { return getLocalHost(0); }
	inline static InetAddress getLoopbackAddress() { return getLoopbackAddress(0); }
	inline static std::vector<InetAddress> getAllByName(const char * host) { return getAllByName(host, 0); }
	
	static InetAddress getAnyAddressIPv4(uint16_t port);
	static InetAddress getAnyAddressIPv6(uint16_t port);
	static InetAddress getByName(const char * host, uint16_t port);
	static InetAddress getLocalHost(uint16_t port);
	static InetAddress getLoopbackAddress(uint16_t port);
	static std::vector<InetAddress> getAllByName(const char * host, uint16_t port);
	
	private:
	const struct sockaddr * mInfo;
	const size_t mInfoLen;
	
	static InetAddress smDefault;
	
};

} // namespace jlcommon

