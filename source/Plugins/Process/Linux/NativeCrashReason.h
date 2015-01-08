//===-- NativeCrashReason.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_NativeCrashReason_H_
#define liblldb_NativeCrashReason_H_

#include "Plugins/Process/POSIX/CrashReason.h"

namespace lldb_private
{

CrashReason
GetCrashReason(const siginfo_t& info);

}  // End lldb_private namespace.

#endif // #ifndef liblldb_NativeCrashReason_H_
