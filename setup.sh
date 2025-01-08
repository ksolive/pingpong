sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_driver_1.41.bin
sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_driver_2.11.54c9c4c.bin
sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.21.100.1.bin
sudo chmod 777 sgx_linux_x64_driver_2.11.54c9c4c.bin
sudo apt update
sudo apt-get install -y build-essential ocaml ocamlbuild automake autoconf libtool wget python-is-python3 libssl-dev git cmake perl
sudo ./sgx_linux_x64_driver_2.11.54c9c4c.bin

cd /opt/intel
sudo git clone https://github.com/intel/linux-sgx.git
cd linux-sgx
sudo apt install unzip
sudo make preparation
sudo cp external/toolset/ubuntu20.04/* /usr/local/bin
sudo make sdk
sudo make sdk_install_pkg

sudo apt-get install -y build-essential python
# mark
cd linux/installer/bin

sudo ./sgx_linux_x64_sdk_2.23.100.2.bin
no
/opt/intel/
source /opt/intel/sgxsdk/environment

cd /opt/intel/linux-sgx
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list
sudo wget https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key
sudo apt-key add intel-sgx-deb.key
sudo apt-get update
sudo apt-get install -y libsgx-launch libsgx-urts libsgx-quote-ex libsgx-urts libsgx-epid libsgx-urts libsgx-dcap-ql
cd /opt/intel/linux-sgx/SampleCode/SampleEnclave
sudo make

cd /opt/intel/linux-sgx
sudo wget https://download.01.org/intel-sgx/sgx-linux/2.19/as.ld.objdump.r4.tar.gz
sudo tar -zxf ./as.ld.objdump.r4.tar.gz
sudo cp external/toolset/ubuntu20.04/* /usr/local/bin/
# which ar  as  ld  objcopy  objdump  ranlib
sudo wget https://github.com/intel/intel-sgx-ssl/archive/refs/tags/lin_2.19_1.1.1t.zip
sudo unzip lin_2.19_1.1.1t.zip
cd intel-sgx-ssl-lin_2.19_1.1.1t/openssl_source
sudo wget https://www.openssl.org/source/old/1.1.1/openssl-1.1.1t.tar.gz
cd /opt/intel/linux-sgx/intel-sgx-ssl-lin_2.19_1.1.1t/Linux
sudo make all test
# mark
sudo make install
cd /opt/intel/linux-sgx

sudo wget https://github.com/docopt/docopt.cpp/archive/refs/tags/v0.6.3.tar.gz
sudo tar -zxf ./v0.6.3.tar.gz
cd ./docopt.cpp-0.6.3/
sudo cmake . 
sudo make install

sudo snap install cmake --channel=3.19/stable --classic

sudo apt-get install -y build-essential autoconf libtool pkg-config automake zlib1g-dev
cd ~/muti-boomerang-dev/thirdparty/protobuf/cmake
mkdir build
cd build
cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/../install ..
make -j `nproc`
make install

cd ~/muti-boomerang-dev/thirdparty/grpc
git submodule update --init
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=`pwd`/../../protobuf/cmake/install -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_PROTOBUF_PROVIDER=package -DgRPC_ZLIB_PROVIDER=package -DgRPC_CARES_PROVIDER=module -DgRPC_SSL_PROVIDER=package \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install \
      ../
make -j `nproc`
make install

sudo cp ~/muti-boomerang-dev/FindSGX.cmake /opt/intel/sgxsdk
# Please note to revise: set(EDL_SEARCH_PATHS Enclave /opt/intel/sgxssl/include)
