#include <udp_server.h>

#include <unistd.h>

namespace jlcommon {

UdpServer::~UdpServer() {
	stop();
	delete mBindAddress;
	delete [] mReadBuffer;
}

int UdpServer::send(InetAddress & addr, const void * buffer, size_t len, int flags) {
	return sendto(mSockfd, buffer, len, flags, addr.getAddress(), addr.getAddressLength());
}

void UdpServer::setBroadcast(int enabled) {
	setsockopt(mSockfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
}

void UdpServer::start() {
	int fd;
	if ((fd = socket(mBindAddress->getAddress()->sa_family, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("listener: socket");
		return;
	}
	
	if (bind(fd, mBindAddress->getAddress(), mBindAddress->getAddressLength()) == -1) {
		close(fd);
		perror("listener: bind");
		return;
	}
	
	{
		struct sockaddr_storage addr;
		socklen_t addrLen = sizeof(addr);
		if (getsockname(fd, (struct sockaddr *)&addr, &addrLen) == -1) {
			close(fd);
			perror("getsockname");
			return;
		} else {
			delete mBindAddress;
			mBindAddress = new InetAddress((struct sockaddr *) &addr, addrLen);
		}
	}
	
	mSockfd = fd;
	mReadThread = std::make_unique<std::thread>([this]{read();});
}

void UdpServer::stop() {
	if (mSockfd != -1) {
		shutdown(mSockfd, SHUT_RDWR);
		mReadThread->join();
		mSockfd = -1;
	}
}

void UdpServer::read() {
	fd_set readfds;
	fd_set excpfds;
	
	FD_ZERO(&readfds);
	FD_ZERO(&excpfds);
	FD_SET(mSockfd, &readfds);
	FD_SET(mSockfd, &excpfds);
	
	struct sockaddr_storage remoteAddr;
	socklen_t remoteAddrLen = sizeof(remoteAddr);
	ssize_t n;
	
	while (mSockfd != -1 && select(mSockfd+1, &readfds, NULL, &excpfds, NULL) != -1) {
		if (FD_ISSET(mSockfd, &readfds)) {
			if ((n = recvfrom(mSockfd, mReadBuffer, mReadBufferSize, MSG_DONTWAIT, (struct sockaddr *) &remoteAddr, &remoteAddrLen)) == -1) {
				break;
			}
			mReadHandler(const_cast<const char *>(mReadBuffer), n);
		} else if (FD_ISSET(mSockfd, &excpfds)) {
			break;
		}
	}
}

} // namespace jlcommon
