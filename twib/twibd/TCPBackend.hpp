#pragma once

#include "platform.hpp"

#include<thread>
#include<list>
#include<queue>
#include<mutex>
#include<condition_variable>

#include "Buffer.hpp"
#include "Device.hpp"
#include "Messages.hpp"
#include "Protocol.hpp"
#include "SocketMessageConnection.hpp"

namespace twili {
namespace twibd {

class Twibd;

namespace backend {

class TCPBackend {
 public:
	TCPBackend(Twibd &twibd);
	~TCPBackend();

	std::string Connect(std::string hostname, std::string port);
	void Connect(sockaddr *sockaddr, socklen_t addr_len);
	
	class Device : public twibd::Device, public std::enable_shared_from_this<Device> {
	 public:
		Device(SOCKET fd, TCPBackend &backend);
		~Device();

		void Begin();
		void Identified(Response &r);
		void IncomingMessage(protocol::MessageHeader &mh, util::Buffer &payload, util::Buffer &object_ids);
		virtual void SendRequest(const Request &&r) override;
		virtual int GetPriority() override;
		virtual std::string GetBridgeType() override;
		
		TCPBackend &backend;
		twibc::SocketMessageConnection connection;
		std::list<WeakRequest> pending_requests;
		Response response_in;
		bool ready_flag = false;
		bool added_flag = false;
	};

 private:
	Twibd &twibd;
	std::list<std::shared_ptr<Device>> devices;

	SOCKET listen_fd;
	
	bool event_thread_destroy = false;
	void event_thread_func();
	std::thread event_thread;
	void NotifyEventThread();

	class EventThreadNotifier : public twibc::SocketMessageConnection::EventThreadNotifier {
	 public:
		EventThreadNotifier(TCPBackend &backend);
		virtual void Notify() override;
	 private:
		TCPBackend &backend;
	} notifier;
};

} // namespace backend
} // namespace twibd
} // namespace twili
