cmake_minimum_required(VERSION 3.25)
project(NanoGCC)

include_directories(include)
add_compile_options(-fno-rtti)

set(CMAKE_CXX_STANDARD 17)

add_executable(NanoGCC  include/ClangAPI.h include/Module/Module.h include/MachineCode/ByteCode.h include/Module/AST.h src/main.cpp src/Module/Module.cpp src/Module/AST.cpp src/Module/ByteCode.cpp include/MachineCode/AOT.h include/MachineCode/JIT.h include/MachineCode/x86Code.h src/MachineCode/JIT.cpp src/MachineCode/AOT.cpp src/MachineCode/x86Code.cpp src/Symbol/Symbol.cpp src/Symbol/Symbol.cpp)
target_link_libraries(NanoGCC clang clang-cpp Remarks LTO LLVMCore LLVMRemarks LLVMBitstreamReader LLVMBinaryFormat LLVMTargetParser LLVMSupport LLVMDemangle rt dl m z tinfo xml2)