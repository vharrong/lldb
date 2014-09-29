//===-- HostThreadAndroid.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/DataBuffer.h"
#include "lldb/Host/android/HostThreadAndroid.h"
#include "Plugins/Process/Android/ProcFileReader.h"

#include "llvm/ADT/SmallVector.h"

#include <pthread.h>

using namespace lldb_private;

HostThreadAndroid::HostThreadAndroid()
    : HostThreadPosix()
{
}

HostThreadAndroid::HostThreadAndroid(lldb::thread_t thread)
    : HostThreadPosix(thread)
{
}

void
HostThreadAndroid::SetName(lldb::thread_t thread, llvm::StringRef name)
{
	assert( 0 && "not implemented" );
}

void
HostThreadAndroid::GetName(lldb::thread_t thread, llvm::SmallVectorImpl<char> &name)
{
	assert( 0 && "not implemented" );
}
