
#include "test.grpc.pb.h"
#include <atomic>
#include <grpcpp/support/status.h>


#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <utility>

std::atomic<int> active_requests = 0;
int max_active_requests = 0;

class OnBlockExit {
	std::function<void()> func_;
public:
	template<typename Func>
	OnBlockExit(Func&& func) {
		func_ = std::forward<Func>(func);
	}
	~OnBlockExit() {
		func_();
	}
};

class KeyValueStoreService : public keyvaluestore::KeyValueStore::Service {
    grpc::Status GetValue(grpc::ServerContext*,
		const keyvaluestore::Request*,
		keyvaluestore::Response* response) {

		auto ar = ++active_requests;
		if (max_active_requests < ar) {
			max_active_requests = ar;
			std::cout << ar << std::endl;
		}
		OnBlockExit ob([]() {
			--active_requests;
		});


		*response->mutable_value() = std::string((1 << 18), ' ');

		return grpc::Status::OK;
	}
};

void RunServer(uint16_t port) {
	std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
	KeyValueStoreService service;

	grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();
	grpc::ServerBuilder builder;
	grpc::ResourceQuota rq;
	rq.SetMaxThreads(211);

	builder.SetResourceQuota(rq);


    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS, 10);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, 10);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, 100);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::CQ_TIMEOUT_MSEC, 10);

	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;

	server->Wait();
}


int main() {
	std::string port_num_str = std::string(std::getenv("PORT"));
	uint16_t port_num = std::stoi(port_num_str);
	RunServer(port_num);

	return 0;
}