//===-- HostThreadAndroid.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_Host_android_HostThreadAndroid_h_
#define lldb_Host_android_HostThreadAndroid_h_

#include "lldb/Host/posix/HostThreadPosix.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallString.h"

namespace lldb_private
{

class HostThreadAndroid : public HostThreadPosix
{
  public:
    HostThreadAndroid();
    HostThreadAndroid(lldb::thread_t thread);

    static void SetName(lldb::thread_t thread, llvm::StringRef name);
    static void GetName(lldb::thread_t thread, llvm::SmallVectorImpl<char> &name);
};
}

#endif
