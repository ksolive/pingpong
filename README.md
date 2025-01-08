# PingPong

----------

PingPong is a new metadata-private messaging system without coordination (dialing). PingPong addresses the usability and performance limitation by introducing a metadata-private notification system Ping along with a metadata-private message store Pong. 

We are refining the instructions for PingPong to help you get up and running as quickly as possible. In the meantime, please follow the steps below to set up and run PingPong.

This repository is anonymized for submission on CCS'25 (Submission ID: 239).


# 1. Installation

<details>
  <summary>Prerequisites</summary>

<!--TODO: Precise OS versions-->

- Operating systems
  - Ubuntu 20.04
  - gcc 7.50 (not change)
  - cmake 3.26.3 (snap install cmake --classic)
  - make 4.1 (not change)
  - openssl 1.1.1t (sgxssl requires)
- Compute backends
  - Intel CPU with SGX support
   </details>

Recommend using Azure Standard DCxds v3 series virtual machines.

## 1.1 Complete Compilation Environment Installation
### Use the following commands to install the required tools to build the Intel(R) SGX.
```
sudo apt update
sudo apt-get install -y build-essential ocaml ocamlbuild automake autoconf libtool wget python-is-python3 libssl-dev git cmake perl python libsgx-launch libsgx-urts libsgx-quote-ex libsgx-epid libsgx-dcap-ql
```
***
### Install the SGX Driver & SGX PSW & SGX SDK & SGX SSL
* linux sgx: 2.16
* sgx driver: 1.41
* sgx sdk: 2.21
* sgx psw: no version
* sgx ssl: linux-2.19-1.1.1m (https://github.com/intel/intel-sgx-ssl/archive/refs/tags/lin_2.19_1.1.1t.zip)
* openssl: 1.1.1t (https://www.openssl.org/source/old/1.1.1/openssl-1.1.1t.tar.gz)
#### Before starting everything, you first should test if SGX is supported. :) We kindly suggest Azure DCsv3-series.
```
git clone https://github.com/ayeks/SGX-hardware.git
cd SGX-hardware
gcc test-sgx.c -o test-sgx
./test-sgx
```
The output should be like this.
```
...
Extended feature bits (EAX=07H, ECX=0H)
eax: 0 ebx: 29c6fbf ecx: 0 edx: 0
sgx available: 1

CPUID Leaf 12H, Sub-Leaf 0 of Intel SGX Capabilities (EAX=12H,ECX=0)
eax: 1 ebx: 0 ecx: 0 edx: 241f
sgx 1 supported: 1
sgx 2 supported: 0
...
```

Or you can check using "cpuid -1 | grep -i sgx", and you should get the output as below.

**Note** that the first two lines must be true, and at least one of the last two lines needs to be true.
```
  SGX: Software Guard Extensions supported = true
  SGX_LC: SGX launch config supported      = true
  SGX capability (0x12/0):
  SGX1 supported                         = true
  SGX2 supported                         = false
  SGX attributes (0x12/1):
```
If supported, install all SGX tools in /opt/intel.
```
sudo mkdir -p /opt/intel
cd /opt/intel
```
#### Install the SGX Driver
```
sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_driver_1.41.bin
sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_driver_2.11.54c9c4c.bin
sudo wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.21.100.1.bin
sudo chmod 777 sgx_linux_x64_driver_2.11.54c9c4c.bin
sudo ./sgx_linux_x64_driver_2.11.54c9c4c.bin
```
#### Install the SGX PSW
```
cd /opt/intel/linux-sgx
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list
sudo wget https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key
sudo apt-key add intel-sgx-deb.key
sudo apt-get update
sudo apt-get install -y libsgx-launch libsgx-urts libsgx-quote-ex libsgx-urts libsgx-epid libsgx-urts libsgx-dcap-ql
```
#### Install the SGX SDK
```
cd /opt/intel
sudo git clone https://github.com/intel/linux-sgx.git
cd linux-sgx
sudo apt install unzip
sudo make preparation
sudo cp external/toolset/ubuntu20.04/* /usr/local/bin
sudo make sdk
sudo make sdk_install_pkg
sudo apt-get install -y build-essential python
cd linux/installer/bin
sudo ./sgx_linux_x64_sdk_2.23.100.2.bin
```
Please choose "no" and change the install path to "/opt/intel". 
**Note:** the install path must be "/opt/intel" !
```
echo "source /opt/intel/sgxsdk/environment" >> ~/.bashrc
source ~/.bashrc
```
You can find sample code for testing from /opt/intel/sgxsdk/SampleCode/SampleEnclave.
```
pushd /opt/intel/sgxsdk/SampleCode/SampleEnclave
sudo make
./app
popd
```
The output should be like:
```
Checksum(0x0x7ffed9a2bb00, 100) = 0xfffd4143
Info: executing thread synchronization, please wait...  
Info: SampleEnclave successfully returned.
Enter a character before exit ...
```
Copy FindSGXSDK.cmake from boomerang to /opt/intel/sgxsdk/.
```
cp <CodePath>/boomerang/FindSGXSDK.cmake /opt/intel/sgxsdk/
```
#### Install the SGX SSL
```
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
sudo make install
```
* (Not suggested) If you want to change the location of the sgx family installation, look for the location marked # change_sgx_path in the code and change it to the corresponding value by referring to the normal location.*

***
Recommended for subsequent installations in boomerang/thirdparty

***

###  Install the docopt (Option Parser)

```
sudo wget https://github.com/docopt/docopt.cpp/archive/refs/tags/v0.6.3.tar.gz
sudo tar -zxf ./v0.6.3.tar.gz
cd ./docopt.cpp-0.6.3/
sudo cmake . 
sudo make install
```
***

###  Install the gRPC & Protocol Buffer for PingSystem and PongSystem
```
# in ./PingSystem and ./PongSystem
cd thirdparty
git submodule update --init
```
Protobuf:
```
sudo apt-get install build-essential autoconf libtool pkg-config automake zlib1g-dev
pushd protobuf/cmake
mkdir build
pushd build
cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/../install ..
make -j `nproc`
make install
popd
popd
```
gRPC:
```
pushd grpc
git submodule update --init
mkdir build
pushd build
cmake -DCMAKE_PREFIX_PATH=`pwd`/../../protobuf/cmake/install -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_PROTOBUF_PROVIDER=package -DgRPC_ZLIB_PROVIDER=package -DgRPC_CARES_PROVIDER=module -DgRPC_SSL_PROVIDER=package \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install \
      ../
make
make install
popd
popd
```
Please reset `PROTO_INSTALL_DIR` and `GRPC_INSTALL_DIR` in `src/CMakeLists.txt` by the actual installation position. They are marked as # grpc_path.


# 2. Multi-server Deployment Guide

You need to separately run the code for the 'PingServer' section and the 'PongServer' section, and modify the configuration files. Both use almost identical deployment steps.

## 2.1 Pong: The metadata-private message system

Compile the 'client' and 'loadbalancer'

```python
# in ./PongSystem/
mkdir build; cd build; cmake ..
make Client Enode
```

Compile the 'PongSystemOram' and place the executable file in 'PongSystem/build/pack'.

Modify `./PongSystem/config/config_multi_server.json` file. The format shows as below, and `port` is recommended to be a number larger than 1024 and be successive in the same machine.

```python
clt_addr: {
    private ip:port
}
enode_addr: {
    private ip:port
}
bnode_addr: {
    private ip:port
}
nat: {
    private ip:public ip
}
```

Modify `PongSystem/scripts/gen.py` to match the `./PongSystem/config/config_multi_server.json`.

```
...
    usernum = 10000
    grpcnum = 1
    paranum = 8
    bnodenum = 24
    enode_num = 8
...
```

Run 'gen.py' and the generated Bash script.

```
chmod +x e8b10u1g1w.sh
./e8b10u1g1w.sh >> log.txt
```

## 2.2 Ping: The metadata-private notification system

Compile the 'client', 'enode' and 'bnode'

```python
# in ./PingSystem/
mkdir build; cd build; cmake ..
make Client Enode Bnode
```

Modify `./PingSystem/config/config_multi_server.json` file. The format shows as below, and `port` is recommended to be a number larger than 1024 and be successive in the same machine.

```python
clt_addr: {
    private ip:port
}
enode_addr: {
    private ip:port
}
bnode_addr: {
    private ip:port
}
nat: {
    private ip:public ip
}
```

Modify `PingSystem/scripts/gen.py` to match the `./PingSystem/config/config_multi_server.json`.

```
...
    usernum = 10000
    grpcnum = 1
    paranum = 8
    bnodenum = 24
    enode_num = 8
...
```

Run `PingSystem/scripts/gen.py` and the generated Bash script.

```
chmod +x e8b10u1g1w.sh
./e8b10u1g1w.sh >> log.txt
```
