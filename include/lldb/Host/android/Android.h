//===-- lldb-android.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_lldb_android_h_
#define LLDB_lldb_android_h_

#include <errno.h>

#define _isatty			isatty
#define SYS_tgkill		__NR_tgkill
#define PT_DETACH		PTRACE_DETACH

typedef int				__ptrace_request;

#endif  // LLDB_lldb_android_h_
