cmake_minimum_required(VERSION 3.7.2)
project(clang_devel_clion)

set(CMAKE_CXX_STANDARD 11)

file(GLOB project_include
        llvm-include/llvm/Pass.h

        llvm-include/llvm/IR/*.h
        llvm-include/llvm/IR/*.def

        llvm-include/llvm/Support/*.h
        llvm-include/llvm/Support/*.def

        llvm-include/llvm/IRReader/*.h
        llvm-include/llvm/IRReader/*.def

        llvm-include/llvm/Transforms/*.h
        llvm-include/llvm/Transforms/*.def

        llvm-include/llvm/Bitcode/*.h
        llvm-include/llvm/Bitcode/*.def
        )

set(SOURCE_FILES
        ${project_include}

        data-flow-analysis/Dataflow.h
        data-flow-analysis/Liveness.h
        data-flow-analysis/LLVMAssignment.cpp
        data-flow-analysis/log.cpp
        data-flow-analysis/log.h
        data-flow-analysis/FuncPointerDataFlow.cpp
        data-flow-analysis/FuncPointerDataFlow.h
        )

include_directories(data-flow-analysis)
include_directories(llvm-include)

add_executable(clang_devel_clion ${SOURCE_FILES})