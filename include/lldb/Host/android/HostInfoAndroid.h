//===-- HostInfoAndroid.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_Host_android_HostInfoAndroid_h_
#define lldb_Host_android_HostInfoAndroid_h_

#include "lldb/Host/FileSpec.h"
#include "lldb/Host/posix/HostInfoPosix.h"

#include "llvm/ADT/StringRef.h"

#include <string>

namespace lldb_private
{

class HostInfoAndroid : public HostInfoPosix
{
    friend class HostInfoBase;

  private:
    // Static class, unconstructable.
    HostInfoAndroid();
    ~HostInfoAndroid();

  public:
    static void Initialize();

    static bool GetOSVersion(uint32_t &major, uint32_t &minor, uint32_t &update);
    static llvm::StringRef GetDistributionId();
    static FileSpec GetProgramFileSpec();

  protected:
    static bool ComputeSystemPluginsDirectory(FileSpec &file_spec);
    static bool ComputeUserPluginsDirectory(FileSpec &file_spec);
    static void ComputeHostArchitectureSupport(ArchSpec &arch_32, ArchSpec &arch_64);
};
}

#endif
