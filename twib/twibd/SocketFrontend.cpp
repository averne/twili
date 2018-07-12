#include "SocketFrontend.hpp"

#include<algorithm>
#include<iostream>

#ifdef _WIN32
#include<ws2tcpip.h>
#include<winsock2.h>
#else
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/un.h>
#include<netinet/in.h>
#endif

#include<string.h>
#include<unistd.h>

#include "Twibd.hpp"
#include "Protocol.hpp"
#include "config.hpp"

namespace twili {
namespace twibd {
namespace frontend {

#ifdef _WIN32
static inline char *NetErrStr() {
	char *s = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&s, 0, NULL);
	return s;
}
#else
static inline char *NetErrStr() {
	return strerror(errno);
}
#endif

SocketFrontend::SocketFrontend(Twibd *twibd, int address_family, int socktype, struct sockaddr *bind_addr, size_t bind_addrlen) :
	twibd(twibd),
	address_family(address_family),
	socktype(socktype),
	bind_addrlen(bind_addrlen) {

	if(bind_addr == NULL) {
		log(FATAL, "failed to allocate bind_addr");
		exit(1);
	}
	memcpy((char*) &this->bind_addr, bind_addr, bind_addrlen);
	
	fd = socket(address_family, socktype, 0);
	if(fd == INVALID_SOCKET) {
		log(FATAL, "failed to create socket: %s", NetErrStr());
		exit(1);
	}
	
	log(DEBUG, "created socket: %d", fd);

	if(address_family == AF_INET6) {
		int ipv6only = 0;
		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &ipv6only, sizeof(ipv6only)) == -1) {
			log(FATAL, "failed to make ipv6 server dual stack: %s", NetErrStr());
		}
	}

	UnlinkIfUnix();
	
	if(bind(fd, bind_addr, bind_addrlen) < 0) {
		log(FATAL, "failed to bind socket: %s", NetErrStr());
		closesocket(fd);
		exit(1);
	}

	if(listen(fd, 20) < 0) {
		log(FATAL, "failed to listen on socket: %s", NetErrStr());
		closesocket(fd);
		UnlinkIfUnix();
		exit(1);
	}

#ifndef _WIN32
	if(pipe(event_thread_notification_pipe) < 0) {
		log(FATAL, "failed to create pipe for event thread notifications: %s", NetErrStr());
		closesocket(fd);
		UnlinkIfUnix();
		exit(1);
	}
#endif // _WIN32

	std::thread event_thread(&SocketFrontend::event_thread_func, this);
	this->event_thread = std::move(event_thread);
}

SocketFrontend::~SocketFrontend() {
	event_thread_destroy = true;
	NotifyEventThread();
	event_thread.join();
	
	closesocket(fd);
	UnlinkIfUnix();
#ifndef _WIN32
	close(event_thread_notification_pipe[0]);
	close(event_thread_notification_pipe[1]);
#endif
}

void SocketFrontend::UnlinkIfUnix() {
	if(address_family == AF_UNIX) {
		unlink(((struct sockaddr_un*) &bind_addr)->sun_path);
	}
}

void SocketFrontend::event_thread_func() {
	fd_set readfds;
	fd_set writefds;
	int max_fd = 0;
	while(!event_thread_destroy) {
		log(DEBUG, "socket event thread loop");
		
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		// add server socket
		max_fd = std::max(max_fd, fd);
		FD_SET(fd, &readfds);

#ifndef _WIN32
		// add event thread notification pipe
		max_fd = std::max(max_fd, event_thread_notification_pipe[0]);
		FD_SET(event_thread_notification_pipe[0], &readfds);
#endif
		
		// add client sockets
		for(auto &c : clients) {
			max_fd = std::max(max_fd, c->fd);
			FD_SET(c->fd, &readfds);
			
			if(c->out_buffer.ReadAvailable() > 0) { // only add to writefds if we need to write
				FD_SET(c->fd, &writefds);
			}
		}

		if(select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0) {
			log(FATAL, "failed to select file descriptors: %s", NetErrStr());
			exit(1);
		}

#ifndef _WIN32
		// Check select status on event thread notification pipe
		if(FD_ISSET(event_thread_notification_pipe[0], &readfds)) {
			char buf[64];
			ssize_t r = read(event_thread_notification_pipe[0], buf, sizeof(buf));
			if(r < 0) {
				log(FATAL, "failed to read from event thread notification pipe: %s", strerror(errno));
				exit(1);
			}
			log(DEBUG, "event thread notified: '%.*s'", r, buf);
		}
#endif
		
		// Check select status on server socket
		if(FD_ISSET(fd, &readfds)) {
			log(DEBUG, "incoming connection detected on %d", fd);

			int client_fd = accept(fd, NULL, NULL);
			if(client_fd < 0) {
				log(WARN, "failed to accept incoming connection");
			} else {
				std::shared_ptr<Client> client = std::make_shared<Client>(this, client_fd);
				clients.push_back(client);
				twibd->AddClient(client);
			}
		}
		
		// pump i/o
		for(auto ci = clients.begin(); ci != clients.end(); ci++) {
			std::shared_ptr<Client> &client = *ci;
			if(FD_ISSET(client->fd, &writefds)) {
				client->PumpOutput();
			}
			if(FD_ISSET(client->fd, &readfds)) {
				log(DEBUG, "incoming data for client %x", client->client_id);
				client->PumpInput();
			}
		}

		for (auto i = clients.begin(); i != clients.end(); ) {
			if((*i)->deletion_flag) {
				twibd->RemoveClient(*i);
				i = clients.erase(i);
				continue;
			}

			(*i)->Process();
			i++;
		}
	}
}

void SocketFrontend::NotifyEventThread() {
#ifdef _WIN32
	struct sockaddr_storage addr;
	size_t addrlen = sizeof(addr);
	if(getsockname(fd, &addr, &addrlen) != 0) {
		log(FATAL, "failed to get local server address: %s", NetErrStr());
		exit(1);
	}

	// Connect to the server in order to wake it up
	SOCKET cfd = socket(address_family, socktype, 0);
	if(cfd < 0) {
		log(FATAL, "failed to create socket: %s", NetErrStr());
		exit(1);
	}

	if(connect(cfd, (struct sockaddr*) &addr, addrlen) < 0) {
		log(FATAL, "failed to connect for notification: %s", NetErrStr());
		exit(1);
	}
	
	closesocket(cfd);
#else
	char buf[] = ".";
	if(write(event_thread_notification_pipe[1], buf, sizeof(buf)) != sizeof(buf)) {
		log(FATAL, "failed to write to event thread notification pipe: %s", strerror(errno));
		exit(1);
	}
#endif
}

SocketFrontend::Client::Client(SocketFrontend *frontend, int fd) : frontend(frontend), twibd(frontend->twibd), fd(fd) {
}

SocketFrontend::Client::~Client() {
	closesocket(fd);
}

void SocketFrontend::Client::PumpOutput() {
	log(DEBUG, "pumping out 0x%lx bytes", out_buffer.ReadAvailable());
	std::lock_guard<std::mutex> lock(out_buffer_mutex);
	if(out_buffer.ReadAvailable() > 0) {
		ssize_t r = send(fd, out_buffer.Read(), out_buffer.ReadAvailable(), 0);
		if(r < 0) {
			deletion_flag = true;
			return;
		}
		if(r > 0) {
			out_buffer.MarkRead(r);
		}
	}
}

void SocketFrontend::Client::PumpInput() {
	std::tuple<uint8_t*, size_t> target = in_buffer.Reserve(8192);
	ssize_t r = recv(fd, std::get<0>(target), std::get<1>(target), 0);
	if(r <= 0) {
		deletion_flag = true;
		return;
	} else {
		in_buffer.MarkWritten(r);
	}
}

void SocketFrontend::Client::Process() {
	while(in_buffer.ReadAvailable() > 0) {
		if(!has_current_mh) {
			if(in_buffer.Read(current_mh)) {
				has_current_mh = true;
			} else {
				in_buffer.Reserve(sizeof(protocol::MessageHeader));
				return;
			}
		}

		current_payload.Clear();
		if(in_buffer.Read(current_payload, current_mh.payload_size)) {
			twibd->PostRequest(
				Request(
					client_id,
					current_mh.device_id,
					current_mh.object_id,
					current_mh.command_id,
					current_mh.tag,
					std::vector<uint8_t>(current_payload.Read(), current_payload.Read() + current_payload.ReadAvailable())));
			has_current_mh = false;
		} else {
			in_buffer.Reserve(current_mh.payload_size);
			return;
		}
	}
}

void SocketFrontend::Client::PostResponse(Response &r) {
	std::lock_guard<std::mutex> lock(out_buffer_mutex);
	protocol::MessageHeader mh;
	mh.device_id = r.device_id;
	mh.object_id = r.object_id;
	mh.result_code = r.result_code;
	mh.tag = r.tag;
	mh.payload_size = r.payload.size();

	out_buffer.Write(mh);
	out_buffer.Write(r.payload);

	frontend->NotifyEventThread();
}

} // namespace frontend
} // namespace twibd
} // namespace twili
