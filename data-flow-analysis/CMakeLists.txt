set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Core
  IRReader
  Passes
  )

# Support plugins.

add_llvm_tool(dataflow
        LLVMAssignment.cpp
        FuncPointerDataFlow.cpp
        utils.cpp
        log.cpp
  )

