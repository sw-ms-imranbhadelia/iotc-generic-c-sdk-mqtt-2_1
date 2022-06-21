# iotc-generic-c-sdk-mqtt-2_1
Generic IoTConnect C SDK for Windows

#### Building and Running with Visual Studio 2019

* Download and install [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/).
* Download and install [CMake](https://cmake.org/download/). Ensure to add CMake to system path.
* Download and extract [Vcpkg](https://github.com/microsoft/vcpkg/releases).

Run PowerShell and execute (use x86 instead of x64 if on a 32-bit machine):

```shell script
cd <vcpkg install directory>
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
.\vcpkg.exe install curl:x64-windows
.\vcpkg.exe install openssl:x64-windows
exit
```

By exiting the PowerShell we ensure that we pick up the "integrate install" environment. 

Run a new PowerShell or use developer console in Visual Studio. If running PowerShell 
ensure to change the directory to where this repo is extracted or cloned. 


```shell script
cd samples/basic-sample
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg install directory>/scripts/buildsystems/vcpkg.cmake 
cmake --build . --target basic-sample 
.\Debug\basic-sample.exe
