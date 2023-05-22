cmake_minimum_required(VERSION 3.25)
project(NanoGCC)

include_directories(include)
add_compile_options(-fno-rtti -save-temps)

set(CMAKE_CXX_STANDARD 17)

add_executable(NanoGCC  include/ClangAPI.h include/Module/Module.h  include/Module/AST.h src/main.cpp src/Module/Module.cpp src/Module/AST.cpp src/Symbol/Symbol.cpp include/Utils/ErrorMessage.h include/CodeGen/x86Code.h src/CodeGen/ELFGen.cpp src/CodeGen/x86CodeEmitter.cpp include/Utils/FreeAll.h src/CodeGen/StdLibrary.def src/CodeGen/Cmd.def)
target_link_libraries(NanoGCC clang clang-cpp Remarks LTO LLVMCore LLVMRemarks LLVMBitstreamReader LLVMBinaryFormat LLVMTargetParser LLVMSupport LLVMDemangle rt dl m z tinfo xml2)