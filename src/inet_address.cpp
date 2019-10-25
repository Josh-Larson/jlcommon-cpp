#include <inet_address.h>

#include <netdb.h>		// structs
#include <arpa/inet.h>	// inet_ntop

#include <log.h>
#include <vector>
#include <cstring>

namespace jlcommon {

InetAddress InetAddress::smDefault = InetAddress(nullptr, 0);

InetAddress::InetAddress(const InetAddress & addr) : mInfo((struct sockaddr *) malloc(addr.mInfoLen)), mInfoLen(addr.mInfoLen) {
	memcpy(const_cast<struct sockaddr*>(mInfo), addr.mInfo, mInfoLen);
}

InetAddress::InetAddress(struct sockaddr * info, size_t infoLen) : mInfo((struct sockaddr *) malloc(infoLen)), mInfoLen(infoLen) {
	memcpy(const_cast<struct sockaddr*>(mInfo), info, infoLen);
}

InetAddress::~InetAddress() {
	free(const_cast<struct sockaddr*>(mInfo));
}

int InetAddress::getVersion() {
	return (mInfo->sa_family == AF_INET) ? 4 : 6;
}

int InetAddress::getPort() {
	return ntohs((mInfo->sa_family == AF_INET) ? ((struct sockaddr_in*) mInfo)->sin_port : ((struct sockaddr_in6*) mInfo)->sin6_port);
}

std::string InetAddress::getAddressString() {
	if (mInfo->sa_family == AF_INET) {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(((struct sockaddr_in*) mInfo)->sin_addr), ip, INET_ADDRSTRLEN);
		return std::string(ip);
	} else {
		char ip[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(((struct sockaddr_in6*) mInfo)->sin6_addr), ip, INET6_ADDRSTRLEN);
		return std::string(ip);
	}
}

const struct sockaddr * InetAddress::getAddress() {
	return mInfo;
}

size_t InetAddress::getAddressLength() const {
	return mInfoLen;
}

InetAddress InetAddress::getAnyAddressIPv4(uint16_t port) {
	struct sockaddr_in * addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
	
	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = INADDR_ANY;
	
	InetAddress ret = InetAddress((struct sockaddr*) addr, sizeof(struct sockaddr_in));
	free(addr);
	return ret;
}

InetAddress InetAddress::getAnyAddressIPv6(uint16_t port) {
	struct sockaddr_in6 * addr = (struct sockaddr_in6*) malloc(sizeof(struct sockaddr_in6));
	
	memset(addr, 0, sizeof(struct sockaddr_in6));
	addr->sin6_family = AF_INET6;
	addr->sin6_port = htons(port);
	addr->sin6_addr = in6addr_any;
	
	InetAddress ret = InetAddress((struct sockaddr*) addr, sizeof(struct sockaddr_in6));
	free(addr);
	return ret;
}

InetAddress InetAddress::getByName(const char * host, uint16_t port) {
	std::vector<InetAddress> addrs = getAllByName(host, port);
	return addrs.empty() ? smDefault : addrs[0];
}

InetAddress InetAddress::getLocalHost(uint16_t port) {
	return getByName("localhost", port);
}

InetAddress InetAddress::getLoopbackAddress(uint16_t port) {
	return getByName(NULL, port);
}

std::vector<InetAddress> InetAddress::getAllByName(const char * host, uint16_t port) {
	std::vector<InetAddress> addrs;
	int status;
	auto hints = addrinfo{};
	auto servinfo = static_cast<addrinfo*>(nullptr);
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((status = getaddrinfo(host, "0", &hints, &servinfo)) != 0) {
		Log::error("getaddrinfo error: %s", gai_strerror(status));
		return addrs;
	}
	
	for (struct addrinfo * p = servinfo; p != nullptr; p = p->ai_next) {
		if (p->ai_addr->sa_family == AF_INET)
			((struct sockaddr_in*) p->ai_addr)->sin_port = htons(port);
		else
			((struct sockaddr_in6*) p->ai_addr)->sin6_port = htons(port);
		addrs.emplace_back(p->ai_addr, p->ai_addrlen);
	}
	
	freeaddrinfo(servinfo); // free the linked-list
	return addrs;
}

} // namespace jlcommon

