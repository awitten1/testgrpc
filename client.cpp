
#undef NDEBUG

#include <chrono>
#include "test.grpc.pb.h"
#include "test.pb.h"
#include <sstream>
#include <stdexcept>
#include <string>
#include <grpcpp/grpcpp.h>

#include <thread>
#include <vector>

class KeyValueClient {
 public:
  KeyValueClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(keyvaluestore::KeyValueStore::NewStub(channel)) {}

  std::string GetValue(const std::string& key) const {
    keyvaluestore::Request request;
    request.set_key(key);

    keyvaluestore::Response reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    grpc::ClientContext context;

    // The actual RPC.
    grpc::Status status = stub_->GetValue(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
    	assert(reply.value().size() == (1 << 18));
    } else {
		std::ostringstream oss;
		oss << "failed to get value. Reason = " << status.error_message() << std::endl;
    	std::cerr << oss.str() << std::endl;
    }
	return std::move(*reply.mutable_value());
  }

 private:
  std::unique_ptr<keyvaluestore::KeyValueStore::Stub> stub_;
};

#define NUM_REQUESTS 10000

void make_requests(const KeyValueClient& client, int thread_id) {
	auto& now = std::chrono::system_clock::now;
	using double_precision_seconds = std::chrono::duration<double>;
	double total = 0;

	for (int i = 0; i < NUM_REQUESTS; ++i) {
		double_precision_seconds t1 = now().time_since_epoch();
		client.GetValue("random_value");
		double_precision_seconds t2 = now().time_since_epoch();
		auto diff = t2 - t1;
		total += diff.count();
	}

	std::cout << "thread_id = " << thread_id << " average request latency ms = " << (total / NUM_REQUESTS) * 1000 << std::endl;
}

#define THREAD_COUNT 200

int main() {
	std::string port_num_str = std::string(std::getenv("PORT"));
	std::string target_str = "localhost:" + port_num_str;
	KeyValueClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

	std::vector<std::jthread> threads;
	for (int i = 0; i < THREAD_COUNT; ++i) {
		threads.emplace_back([&client, i]() {
			make_requests(client, i);
		});
	}
	return 0;
}