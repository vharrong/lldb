//===-- HostNativeThread.h --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_Host_HostNativeThread_h_
#define lldb_Host_HostNativeThread_h_

#if defined(_WIN32)
#include "lldb/Host/windows/HostThreadWindows.h"
namespace lldb_private
{
typedef HostThreadWindows HostNativeThread;
}
#elif defined(__linux__)
#if defined(__ANDROID_NDK__)
#include "lldb/Host/android/HostThreadAndroid.h"
namespace lldb_private
{
typedef HostThreadAndroid HostNativeThread;
}
#else
#include "lldb/Host/linux/HostThreadLinux.h"
namespace lldb_private
{
typedef HostThreadLinux HostNativeThread;
}
#endif // __ANDROID_NDK__
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include "lldb/Host/freebsd/HostThreadFreeBSD.h"
namespace lldb_private
{
typedef HostThreadFreeBSD HostNativeThread;
}
#elif defined(__APPLE__)
#include "lldb/Host/macosx/HostThreadMacOSX.h"
namespace lldb_private
{
typedef HostThreadMacOSX HostNativeThread;
}
#endif

#endif
