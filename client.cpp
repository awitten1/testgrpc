
#include <algorithm>
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
      : stub_(keyvaluestore::KeyValueStore::NewStub(channel)) {
	}

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
    	std::cerr << oss.str();
		assert(false);
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
	std::vector<double> times_seconds;

	for (int i = 0; i < NUM_REQUESTS; ++i) {
		double_precision_seconds t1 = now().time_since_epoch();
		client.GetValue("random_value");
		double_precision_seconds t2 = now().time_since_epoch();
		auto diff = t2 - t1;
		times_seconds.push_back(diff.count());
		total += diff.count();
	}

	std::ostringstream oss;
	oss << "thread_id = " << thread_id << " average request latency ms = " << (total / NUM_REQUESTS) * 1000;

	std::sort(times_seconds.begin(), times_seconds.end());
	auto median = times_seconds[((double)NUM_REQUESTS / 2)];
	auto threeq = times_seconds[((double)3*NUM_REQUESTS / 4)];
	auto firstq = times_seconds[((double)NUM_REQUESTS / 4)];
	auto p99 = times_seconds[((double)99*NUM_REQUESTS / 100)];

	oss << " median = " << median*1000 << " firstq " << firstq*1000 << " thirdq " << threeq*1000 << " p99 " << p99*1000 << std::endl;

	std::cout << oss.str();
}

#define THREAD_COUNT 10

int main() {
	std::string port_num_str = std::string(std::getenv("PORT"));
	std::string target_str = "localhost:" + port_num_str;

	grpc::ChannelArguments ca;
	ca.SetCompressionAlgorithm(GRPC_COMPRESS_DEFLATE);
	// Increase the number of channels gRPC will create internally
	ca.SetInt(GRPC_ARG_MAX_CONCURRENT_STREAMS, 100);
	// Ensure HTTP/2 write buffer has enough capacity
	ca.SetInt(GRPC_ARG_HTTP2_WRITE_BUFFER_SIZE, 1024 * 1024);
	KeyValueClient client(grpc::CreateCustomChannel(target_str, grpc::InsecureChannelCredentials(), ca));

	std::vector<std::jthread> threads;
	for (int i = 0; i < THREAD_COUNT; ++i) {
		threads.emplace_back([&client, i]() {
			make_requests(client, i);
		});
	}
	return 0;
}