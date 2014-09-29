//===-- PlatformAndroid.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_PlatformAndroid_h_
#define liblldb_PlatformAndroid_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "Plugins/Platform/POSIX/PlatformPOSIX.h"

namespace lldb_private {

    class PlatformAndroid : public PlatformPOSIX
    {
    public:

        static void
        DebuggerInitialize (lldb_private::Debugger &debugger);

        static void
        Initialize ();

        static void
        Terminate ();
        
        PlatformAndroid (bool is_host);

        virtual
        ~PlatformAndroid();

        //------------------------------------------------------------
        // lldb_private::PluginInterface functions
        //------------------------------------------------------------
        static lldb::PlatformSP
        CreateInstance (bool force, const lldb_private::ArchSpec *arch);

        static lldb_private::ConstString
        GetPluginNameStatic (bool is_host);

        static const char *
        GetPluginDescriptionStatic (bool is_host);

        lldb_private::ConstString
        GetPluginName() override;
        
        uint32_t
        GetPluginVersion() override
        {
            return 1;
        }

        //------------------------------------------------------------
        // lldb_private::Platform functions
        //------------------------------------------------------------
        Error
        ResolveExecutable (const FileSpec &exe_file,
                           const ArchSpec &arch,
                           lldb::ModuleSP &module_sp,
                           const FileSpecList *module_search_paths_ptr) override;

        const char *
        GetDescription () override
        {
            return GetPluginDescriptionStatic(IsHost());
        }

        void
        GetStatus (Stream &strm) override;

        Error
        GetFileWithUUID (const FileSpec &platform_file,
                         const UUID* uuid, FileSpec &local_file) override;

        bool
        GetProcessInfo (lldb::pid_t pid, ProcessInstanceInfo &proc_info) override;

        bool
        GetSupportedArchitectureAtIndex (uint32_t idx, ArchSpec &arch) override;

        size_t
        GetSoftwareBreakpointTrapOpcode (Target &target, 
                                         BreakpointSite *bp_site) override;

        lldb_private::Error
        LaunchProcess (lldb_private::ProcessLaunchInfo &launch_info) override;

        lldb::ProcessSP
        Attach(ProcessAttachInfo &attach_info, Debugger &debugger,
               Target *target, Listener &listener, Error &error) override;

        bool
        CanDebugProcess () override;

        void
        CalculateTrapHandlerSymbolNames () override;

        Error
        LaunchNativeProcess (
            ProcessLaunchInfo &launch_info,
            lldb_private::NativeProcessProtocol::NativeDelegate &native_delegate,
            NativeProcessProtocolSP &process_sp) override;

        Error
        AttachNativeProcess (lldb::pid_t pid,
                             lldb_private::NativeProcessProtocol::NativeDelegate &native_delegate,
                             NativeProcessProtocolSP &process_sp) override;

    private:
        DISALLOW_COPY_AND_ASSIGN (PlatformAndroid);
    };
} // namespace lldb_private

#endif  // liblldb_PlatformAndroid_h_
