
#!/bin/bash

set -eux

mkdir -p dependencies
mkdir -p dependencies/install
GRPC_VERSION=v1.72.0

build_grpc() {
	git clone --recursive --branch ${GRPC_VERSION} git@github.com:grpc/grpc.git

	cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_INSTALL_PREFIX=install \
	  -DCMAKE_BUILD_TYPE=Release -G Ninja -B build -S grpc

	cmake --build build -j10
	cmake --install build
}

pushd dependencies
build_grpc
popd