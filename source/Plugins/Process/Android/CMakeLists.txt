set(LLVM_NO_RTTI 1)

include_directories(.)
include_directories(../POSIX)
include_directories(../Utility)

add_lldb_library(lldbPluginProcessAndroid
  NativeProcessAndroid.cpp
  NativeRegisterContextAndroid_x86_64.cpp
  NativeThreadAndroid.cpp
  ProcessAndroid.cpp
  ProcessMonitor.cpp
  ProcFileReader.cpp
  AndroidThread.cpp
  )

