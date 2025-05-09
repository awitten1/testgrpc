
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"


#include "test.grpc.pb.h"
#include "test.pb.h"

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

class ServerImpl final {
 public:
  ~ServerImpl() {
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
	for (auto& cq : cq_)  {
    	cq->Shutdown();
	}
  }

  // There is no shutdown handling in this code.
  void Run(std::string port) {
    std::string server_address = "0.0.0.0:" + port;

    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.

	for (int i = 0; i < num_threads_; ++i) {
   		cq_.push_back(builder.AddCompletionQueue());
	}
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;

    // Proceed to the server's main loop.
    HandleRpcs();
  }

 private:
  // Class encompassing the state and logic needed to serve a request.
  class CallData {
   public:
    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion queue "cq" used for asynchronous communication
    // with the gRPC runtime.
    CallData(keyvaluestore::KeyValueStore::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
      // Invoke the serving logic right away.
      Proceed();
    }

    void Proceed() {
	  static std::atomic<int> incs = 0;

      if (status_ == CREATE) {
        // Make this instance progress to the PROCESS state.
        status_ = PROCESS;

        // As part of the initial CREATE state, we *request* that the system
        // start processing SayHello requests. In this request, "this" acts are
        // the tag uniquely identifying the request (so that different CallData
        // instances can serve different requests concurrently), in this case
        // the memory address of this CallData instance.
        service_->RequestGetValue(&ctx_, &request_, &responder_, cq_, cq_,
                                  this);
      } else if (status_ == PROCESS) {

        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new CallData(service_, cq_);

        // The actual processing.
        reply_.set_value(std::string(1 << 18, ' '));

        // And we are done! Let the gRPC runtime know we've finished, using the
        // memory address of this instance as the uniquely identifying tag for
        // the event.
        status_ = FINISH;
        responder_.Finish(reply_, grpc::Status::OK, this);
      } else {
        CHECK_EQ(status_, FINISH);
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
      }
    }

   private:
    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    keyvaluestore::KeyValueStore::AsyncService* service_;
    // The producer-consumer queue where for asynchronous server notifications.
    grpc::ServerCompletionQueue* cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    grpc::ServerContext ctx_;

    // What we get from the client.
    keyvaluestore::Request request_;
    // What we send back to the client.
    keyvaluestore::Response reply_;

    // The means to get back to the client.
    grpc::ServerAsyncResponseWriter<keyvaluestore::Response> responder_;

    // Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;  // The current serving state.
  };

  // This can be run in multiple threads if needed.
  void HandleRpcs() {
    // Spawn a new CallData instance to serve new clients.
    for (int i = 0; i < num_threads_; ++i) {
        new CallData(&service_, cq_[i].get());
    }

    auto work = [this](int i) {
      void* tag;
      bool ok;
      auto& cq = cq_[i];
      while (cq->Next(&tag, &ok)) {
        if (ok) {
          static_cast<CallData*>(tag)->Proceed();
        }
      }
    };
    for (int i = 0; i < num_threads_ - 1; ++i) {
        thread_pool_.emplace_back(work, i);
    }
    work(num_threads_ - 1);

  }

  int num_threads_ = 8;
  keyvaluestore::KeyValueStore::AsyncService service_;
  std::unique_ptr<grpc::Server> server_;

  std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> cq_;
  std::vector<std::jthread> thread_pool_;
};

int main(int argc, char** argv) {
	std::string port_num_str = std::string(std::getenv("PORT"));
	ServerImpl server;

	server.Run(port_num_str);

	return 0;
}