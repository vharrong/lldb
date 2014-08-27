//===-- Host.cpp ------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

// C includes
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef _WIN32
#include "lldb/Host/windows/windows.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/stat.h>
#endif

#if !defined (__GNU__) && !defined (_WIN32)
// Does not exist under GNU/HURD or Windows
#include <sys/sysctl.h>
#endif

#if defined (__APPLE__)
#include <mach/mach_port.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#endif

#if defined (__linux__) || defined (__FreeBSD__) || defined (__FreeBSD_kernel__) || defined (__APPLE__) || defined(__NetBSD__)
#include <spawn.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#endif

#if defined (__linux__)
#include <sys/prctl.h>
#include <linux/prctl.h>
#endif

#if defined (__FreeBSD__)
#include <pthread_np.h>
#endif

// C++ includes
#include <limits>
#include <utility>

#include "lldb/Host/Host.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Core/ArchSpec.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/ThreadSafeSTLMap.h"
#include "lldb/Host/Config.h"
#include "lldb/Host/Endian.h"
#include "lldb/Host/FileSpec.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Host/Pipe.h"
#include "lldb/lldb-private-forward.h"
#include "lldb/Target/FileAction.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ProcessLaunchInfo.h"
#include "lldb/Target/TargetList.h"
#include "lldb/Utility/CleanUp.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#if defined (__APPLE__)
#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR       0x0100
#endif

extern "C"
{
    int __pthread_chdir(const char *path);
    int __pthread_fchdir (int fildes);
}

#endif

using namespace lldb;
using namespace lldb_private;

namespace
{
    // Support for launching in a suspended state, ready to ptrace, at the beginning of the program.
#if defined (__APPLE__) || defined (__linux__) || defined (__FreeBSD__) || defined (__GLIBC__) || defined(__NetBSD__)
    typedef int	(*SpawnFunction) (
        ::pid_t *pid,
        const char *path,
        const posix_spawn_file_actions_t *file_actions,
        const posix_spawnattr_t *attrp,
        char *const argv[],
        char *const envp[],
        PipeSP &sync_pipe_sp);

    int
    SpawnForkPipeSync (
        ::pid_t *pid,
        const char *path,
        const posix_spawn_file_actions_t *file_actions,
        const posix_spawnattr_t *attrp,
        char *const argv[],
        char *const envp[],
        PipeSP &sync_pipe_sp)
    {
        Log *log(lldb_private::GetLogIfAnyCategoriesSet (LIBLLDB_LOG_HOST | LIBLLDB_LOG_PROCESS));
        Error error;

        // We'll use a fork/exec style here.  First we'll fork, then have the parent return
        // success.  The child will set itself up to exec, and will do a read on the sync_pipe_sp right
        // before it execs.  It will block reading one character before it execs.

        assert (!sync_pipe_sp && "pipe was already created");

        sync_pipe_sp.reset (new Pipe ());
        if (! sync_pipe_sp->Open ())
        {
            error.SetErrorToErrno();
            if (log)
                log->Printf ("%s: launch sync pipe open FAILED: %s", __FUNCTION__, error.AsCString ());
            return -1;
        }
        else
        {
            if (log)
                log->Printf ("%s: launch sync pipe open SUCCESS", __FUNCTION__);
        }

        if (log)
            log->Printf ("%s: calling fork()", __FUNCTION__);

        const ::pid_t fork_pid = fork ();
        if (fork_pid == -1)
        {
            const int error_value = errno;
            if (log)
                log->Printf ("%s: fork() call failed: %s", __FUNCTION__, strerror (error_value));
            return error_value;
        }

        if (fork_pid != 0)
        {
            // Parent.
            if (log)
                log->Printf ("%s: fork() call - parent returning after successful fork, child pid %" PRIu64, __FUNCTION__, static_cast<lldb::pid_t> (fork_pid));

            // Close the read side of the pipe.
            if (!sync_pipe_sp->CloseReadFileDescriptor ())
            {
                error.SetErrorToErrno ();
                if (log)
                    log->Printf ("%s: launch sync pipe close the read side in parent FAILED: %s", __FUNCTION__, error.AsCString ());
                // Don't fail the launch because of this.
            }
            else
            {
                if (log)
                    log->Printf ("%s: launch sync pipe close the read side in parent SUCCESS", __FUNCTION__);
            }

            *pid = fork_pid;
            return 0;
        }

        // We're the child.
        if (log)
            log->Printf ("%s: child pid %" PRIu64 " initiating synchronized exec process", __FUNCTION__, static_cast<lldb::pid_t> (getpid()));

        // Close the write side of the launch sync pipe.
        if (!sync_pipe_sp->CloseWriteFileDescriptor ())
        {
            error.SetErrorToErrno ();
            if (log)
                log->Printf ("%s: launch sync pipe child close write side FAILED: %s", __FUNCTION__, error.AsCString ());
            // Don't fail the launch because of this.
        }
        else
        {
            if (log)
                log->Printf ("%s: launch sync pipe child close write side SUCCESS", __FUNCTION__);
        }

        // FIXME set up the launch environment here.

        // For some more recent Linux kernels and distributions,
        // we need to enable a process other than the parent -- i.e.
        // llgs is not the parent of this launched inferior, but a
        // sibling -- to ptrace via a prctl mechanism.
#if defined (__linux__)
#if defined (PR_SET_PTRACER)
        // Attempt to set the permitted ptracer to our parent process (lldb, which we just
        // forked off of) so that llgs, which will be a sibling, can ptrace us later.
        const ::pid_t parent_pid = getppid();
        const int prctl_result = prctl (PR_SET_PTRACER, static_cast<unsigned long>(parent_pid), 0, 0, 0);
        if (prctl_result != 0)
        {
            error.SetErrorToErrno();
            if (log)
                log->Printf ("%s: prctl (PR_SET_PTRACER,%" PRIu64 ",...) FAILED (ignored): %s", __FUNCTION__, static_cast<uint64_t> (parent_pid), error.AsCString ());
            // Don't bail here in case we're calling it on a system combo that doesn't need this.
            // Ubuntu 10.10+ claims it needs it, even though the standard way to check for it in
            // procfs is showing 0 (i.e. disabled) on stock systems.
        }
        else
        {
            if (log)
                log->Printf ("%s: prctl (PR_SET_PTRACER,%" PRIu64 ",...) SUCCESS", __FUNCTION__, static_cast<uint64_t> (parent_pid));
        }
#else
        if (log)
            log->Printf ("%s: prctl (PR_SET_PTRACER,...) skipped because PR_SET_PTRACER is not defined", __FUNCTION__);
#endif // #if defined (PR_SET_PTRACER)
#endif // #if defined (__linux__)

        // Now wait for a read of 1 byte on the launch sync pipe.
        // Our parent (lldb) will send a byte here once we've been
        // successfully attached to by the local llgs.  We need
        // this synchronization to enable the launched inferior to
        // be debugged from the start of the program.
        // This is the child.  Wait now for something to PTRACE us.
        uint8_t sync_byte;
        if (!sync_pipe_sp->Read (&sync_byte, 1))
        {
            error.SetErrorToErrno();
            if (log)
                log->Printf ("%s: launch sync pipe read byte FAILED, canceling launch: %s", __FUNCTION__, error.AsCString ());
            exit (-1);
        }
        else
        {
            if (log)
                log->Printf ("%s: launch sync pipe read byte SUCCESS", __FUNCTION__);
        }

        // Close the read side of the pipe.
        if (!sync_pipe_sp->CloseReadFileDescriptor())
        {
            error.SetErrorToErrno();
            if (log)
                log->Printf ("%s: launch sync pipe child close read side after sync FAILED: %s", __FUNCTION__, error.AsCString ());
            // Don't fail the launch because of this.
        }
        else
        {
            if (log)
                log->Printf ("%s: launch sync pipe child close read side after sync SUCCESS", __FUNCTION__);
        }

        // And now exec.
        if (log)
        {
            log->Printf ("%s: calling execve('%s', argv, envp), argv/envp follows", __FUNCTION__, path ? path : "<null>");
            if (argv)
            {
                for (int i = 0; argv[i] != nullptr; ++i)
                    log->Printf ("-- argv[%d]: %s", i, argv[i]);
            }
            if (envp)
            {
                for (int i = 0; envp[i] != nullptr; ++i)
                    log->Printf ("-- envp[%d]: %s", i, envp[i]);
            }
        }

        const int exec_result = execve (path, argv, envp);

        if (exec_result != 0)
        {
            const int exec_errno = errno;
            if (log)
                log->Printf ("%s: forked pid %" PRIu64 " failed to execve() with path '%s': %s\n", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), path ? path : "<null> path", strerror (exec_errno));
        }
        exit (-1);
    }

    int
    SpawnPosixSpawnp (
                       ::pid_t *pid,
                       const char *path,
                       const posix_spawn_file_actions_t *file_actions,
                       const posix_spawnattr_t *attrp,
                       char *const argv[],
                       char *const envp[],
                       PipeSP & /*sync_pipe_sp*/)
    {
        // Just call ::posix_spawnp and ignore the sync_pipe.
        return ::posix_spawnp (pid, path, file_actions, attrp, argv, envp);
    }

    SpawnFunction
    GetPosixspawnFunction (ProcessLaunchInfo &launch_info)
    {
#if defined (__APPLE__)
        return SpawnPosixSpawnp;
#else
        if (launch_info.GetFlags().Test (eLaunchFlagDebug))
        {
            // We want to start the exe in a suspended state at the program execution
            // start point.
            return SpawnForkPipeSync;
        }

        return SpawnPosixSpawnp;
#endif
    }
#endif
}

// Define maximum thread name length
#if defined (__linux__) || defined (__FreeBSD__) || defined (__FreeBSD_kernel__) || defined (__NetBSD__)
uint32_t const Host::MAX_THREAD_NAME_LENGTH = 16;
#else
uint32_t const Host::MAX_THREAD_NAME_LENGTH = std::numeric_limits<uint32_t>::max ();
#endif

#if !defined (__APPLE__) && !defined (_WIN32)
struct MonitorInfo
{
    lldb::pid_t pid;                            // The process ID to monitor
    Host::MonitorChildProcessCallback callback; // The callback function to call when "pid" exits or signals
    void *callback_baton;                       // The callback baton for the callback function
    bool monitor_signals;                       // If true, call the callback when "pid" gets signaled.
};

static thread_result_t
MonitorChildProcessThreadFunction (void *arg);

lldb::thread_t
Host::StartMonitoringChildProcess
(
    Host::MonitorChildProcessCallback callback,
    void *callback_baton,
    lldb::pid_t pid,
    bool monitor_signals
)
{
    lldb::thread_t thread = LLDB_INVALID_HOST_THREAD;
    MonitorInfo * info_ptr = new MonitorInfo();

    info_ptr->pid = pid;
    info_ptr->callback = callback;
    info_ptr->callback_baton = callback_baton;
    info_ptr->monitor_signals = monitor_signals;
    
    char thread_name[256];

    if (Host::MAX_THREAD_NAME_LENGTH <= 16)
    {
        // some platforms, the threadname is limited to 16 characters, so need to be abbreviated
        ::snprintf (thread_name, sizeof(thread_name), "wait4(%" PRIu64 ")", pid);
    }
    else
    {
        ::snprintf (thread_name, sizeof(thread_name), "<lldb.host.wait4(pid=%" PRIu64 ")>", pid);
    }

    thread = ThreadCreate (thread_name,
                           MonitorChildProcessThreadFunction,
                           info_ptr,
                           NULL);
                           
    return thread;
}

//------------------------------------------------------------------
// Scoped class that will disable thread canceling when it is
// constructed, and exception safely restore the previous value it
// when it goes out of scope.
//------------------------------------------------------------------
class ScopedPThreadCancelDisabler
{
public:
    ScopedPThreadCancelDisabler()
    {
        // Disable the ability for this thread to be cancelled
        int err = ::pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &m_old_state);
        if (err != 0)
            m_old_state = -1;

    }

    ~ScopedPThreadCancelDisabler()
    {
        // Restore the ability for this thread to be cancelled to what it
        // previously was.
        if (m_old_state != -1)
            ::pthread_setcancelstate (m_old_state, 0);
    }
private:
    int m_old_state;    // Save the old cancelability state.
};

static thread_result_t
MonitorChildProcessThreadFunction (void *arg)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));
    const char *function = __FUNCTION__;
    if (log)
        log->Printf ("%s (arg = %p) thread starting...", function, arg);

    MonitorInfo *info = (MonitorInfo *)arg;

    const Host::MonitorChildProcessCallback callback = info->callback;
    void * const callback_baton = info->callback_baton;
    const bool monitor_signals = info->monitor_signals;

    assert (info->pid <= UINT32_MAX);
    const ::pid_t pid = monitor_signals ? -1 * getpgid(info->pid) : info->pid;

    delete info;

    int status = -1;
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__)
    #define __WALL 0
#endif
    const int options = __WALL;

    while (1)
    {
        log = lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS);
        if (log)
            log->Printf("%s ::wait_pid (pid = %" PRIi32 ", &status, options = %i)...", function, pid, options);

        // Wait for all child processes
        ::pthread_testcancel ();
        // Get signals from all children with same process group of pid
        const ::pid_t wait_pid = ::waitpid (pid, &status, options);
        ::pthread_testcancel ();

        if (wait_pid == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                if (log)
                    log->Printf ("%s (arg = %p) thread exiting because waitpid failed (%s)...", __FUNCTION__, arg, strerror(errno));
                break;
            }
        }
        else if (wait_pid > 0)
        {
            bool exited = false;
            int signal = 0;
            int exit_status = 0;
            const char *status_cstr = NULL;
            if (WIFSTOPPED(status))
            {
                signal = WSTOPSIG(status);
                status_cstr = "STOPPED";
            }
            else if (WIFEXITED(status))
            {
                exit_status = WEXITSTATUS(status);
                status_cstr = "EXITED";
                exited = true;
            }
            else if (WIFSIGNALED(status))
            {
                signal = WTERMSIG(status);
                status_cstr = "SIGNALED";
                if (wait_pid == abs(pid)) {
                    exited = true;
                    exit_status = -1;
                }
            }
            else
            {
                status_cstr = "(\?\?\?)";
            }

            // Scope for pthread_cancel_disabler
            {
                ScopedPThreadCancelDisabler pthread_cancel_disabler;

                log = lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS);
                if (log)
                    log->Printf ("%s ::waitpid (pid = %" PRIi32 ", &status, options = %i) => pid = %" PRIi32 ", status = 0x%8.8x (%s), signal = %i, exit_state = %i",
                                 function,
                                 wait_pid,
                                 options,
                                 pid,
                                 status,
                                 status_cstr,
                                 signal,
                                 exit_status);

                if (exited || (signal != 0 && monitor_signals))
                {
                    bool callback_return = false;
                    if (callback)
                        callback_return = callback (callback_baton, wait_pid, exited, signal, exit_status);
                    
                    // If our process exited, then this thread should exit
                    if (exited && wait_pid == abs(pid))
                    {
                        if (log)
                            log->Printf ("%s (arg = %p) thread exiting because pid received exit signal...", __FUNCTION__, arg);
                        break;
                    }
                    // If the callback returns true, it means this process should
                    // exit
                    if (callback_return)
                    {
                        if (log)
                            log->Printf ("%s (arg = %p) thread exiting because callback returned true...", __FUNCTION__, arg);
                        break;
                    }
                }
            }
        }
    }

    log = lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS);
    if (log)
        log->Printf ("%s (arg = %p) thread exiting...", __FUNCTION__, arg);

    return NULL;
}

#endif // #if !defined (__APPLE__) && !defined (_WIN32)

#if !defined (__APPLE__)

void
Host::SystemLog (SystemLogType type, const char *format, va_list args)
{
    vfprintf (stderr, format, args);
}

#endif

void
Host::SystemLog (SystemLogType type, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    SystemLog (type, format, args);
    va_end (args);
}

lldb::pid_t
Host::GetCurrentProcessID()
{
    return ::getpid();
}

#ifndef _WIN32

lldb::tid_t
Host::GetCurrentThreadID()
{
#if defined (__APPLE__)
    // Calling "mach_thread_self()" bumps the reference count on the thread
    // port, so we need to deallocate it. mach_task_self() doesn't bump the ref
    // count.
    thread_port_t thread_self = mach_thread_self();
    mach_port_deallocate(mach_task_self(), thread_self);
    return thread_self;
#elif defined(__FreeBSD__)
    return lldb::tid_t(pthread_getthreadid_np());
#elif defined(__linux__)
    return lldb::tid_t(syscall(SYS_gettid));
#else
    return lldb::tid_t(pthread_self());
#endif
}

lldb::thread_t
Host::GetCurrentThread ()
{
    return lldb::thread_t(pthread_self());
}

const char *
Host::GetSignalAsCString (int signo)
{
    switch (signo)
    {
    case SIGHUP:    return "SIGHUP";    // 1    hangup
    case SIGINT:    return "SIGINT";    // 2    interrupt
    case SIGQUIT:   return "SIGQUIT";   // 3    quit
    case SIGILL:    return "SIGILL";    // 4    illegal instruction (not reset when caught)
    case SIGTRAP:   return "SIGTRAP";   // 5    trace trap (not reset when caught)
    case SIGABRT:   return "SIGABRT";   // 6    abort()
#if  defined(SIGPOLL)
#if !defined(SIGIO) || (SIGPOLL != SIGIO)
// Under some GNU/Linux, SIGPOLL and SIGIO are the same. Causing the build to
// fail with 'multiple define cases with same value'
    case SIGPOLL:   return "SIGPOLL";   // 7    pollable event ([XSR] generated, not supported)
#endif
#endif
#if  defined(SIGEMT)
    case SIGEMT:    return "SIGEMT";    // 7    EMT instruction
#endif
    case SIGFPE:    return "SIGFPE";    // 8    floating point exception
    case SIGKILL:   return "SIGKILL";   // 9    kill (cannot be caught or ignored)
    case SIGBUS:    return "SIGBUS";    // 10    bus error
    case SIGSEGV:   return "SIGSEGV";   // 11    segmentation violation
    case SIGSYS:    return "SIGSYS";    // 12    bad argument to system call
    case SIGPIPE:   return "SIGPIPE";   // 13    write on a pipe with no one to read it
    case SIGALRM:   return "SIGALRM";   // 14    alarm clock
    case SIGTERM:   return "SIGTERM";   // 15    software termination signal from kill
    case SIGURG:    return "SIGURG";    // 16    urgent condition on IO channel
    case SIGSTOP:   return "SIGSTOP";   // 17    sendable stop signal not from tty
    case SIGTSTP:   return "SIGTSTP";   // 18    stop signal from tty
    case SIGCONT:   return "SIGCONT";   // 19    continue a stopped process
    case SIGCHLD:   return "SIGCHLD";   // 20    to parent on child stop or exit
    case SIGTTIN:   return "SIGTTIN";   // 21    to readers pgrp upon background tty read
    case SIGTTOU:   return "SIGTTOU";   // 22    like TTIN for output if (tp->t_local&LTOSTOP)
#if  defined(SIGIO)
    case SIGIO:     return "SIGIO";     // 23    input/output possible signal
#endif
    case SIGXCPU:   return "SIGXCPU";   // 24    exceeded CPU time limit
    case SIGXFSZ:   return "SIGXFSZ";   // 25    exceeded file size limit
    case SIGVTALRM: return "SIGVTALRM"; // 26    virtual time alarm
    case SIGPROF:   return "SIGPROF";   // 27    profiling time alarm
#if  defined(SIGWINCH)
    case SIGWINCH:  return "SIGWINCH";  // 28    window size changes
#endif
#if  defined(SIGINFO)
    case SIGINFO:   return "SIGINFO";   // 29    information request
#endif
    case SIGUSR1:   return "SIGUSR1";   // 30    user defined signal 1
    case SIGUSR2:   return "SIGUSR2";   // 31    user defined signal 2
    default:
        break;
    }
    return NULL;
}

#endif

void
Host::WillTerminate ()
{
}

#if !defined (__APPLE__) && !defined (__FreeBSD__) && !defined (__FreeBSD_kernel__) && !defined (__linux__) // see macosx/Host.mm

void
Host::ThreadCreated (const char *thread_name)
{
}

void
Host::Backtrace (Stream &strm, uint32_t max_frames)
{
    // TODO: Is there a way to backtrace the current process on other systems?
}

size_t
Host::GetEnvironment (StringList &env)
{
    // TODO: Is there a way to the host environment for this process on other systems?
    return 0;
}

#endif // #if !defined (__APPLE__) && !defined (__FreeBSD__) && !defined (__FreeBSD_kernel__) && !defined (__linux__)

struct HostThreadCreateInfo
{
    std::string thread_name;
    thread_func_t thread_fptr;
    thread_arg_t thread_arg;
    
    HostThreadCreateInfo (const char *name, thread_func_t fptr, thread_arg_t arg) :
        thread_name (name ? name : ""),
        thread_fptr (fptr),
        thread_arg (arg)
    {
    }
};

static thread_result_t
#ifdef _WIN32
__stdcall
#endif
ThreadCreateTrampoline (thread_arg_t arg)
{
    HostThreadCreateInfo *info = (HostThreadCreateInfo *)arg;
    Host::ThreadCreated (info->thread_name.c_str());
    thread_func_t thread_fptr = info->thread_fptr;
    thread_arg_t thread_arg = info->thread_arg;
    
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_THREAD));
    if (log)
        log->Printf("thread created");
    
    delete info;
    return thread_fptr (thread_arg);
}

lldb::thread_t
Host::ThreadCreate
(
    const char *thread_name,
    thread_func_t thread_fptr,
    thread_arg_t thread_arg,
    Error *error
)
{
    lldb::thread_t thread = LLDB_INVALID_HOST_THREAD;
    
    // Host::ThreadCreateTrampoline will delete this pointer for us.
    HostThreadCreateInfo *info_ptr = new HostThreadCreateInfo (thread_name, thread_fptr, thread_arg);
    
#ifdef _WIN32
    thread = ::_beginthreadex(0, 0, ThreadCreateTrampoline, info_ptr, 0, NULL);
    int err = thread <= 0 ? GetLastError() : 0;
#else
    int err = ::pthread_create (&thread, NULL, ThreadCreateTrampoline, info_ptr);
#endif
    if (err == 0)
    {
        if (error)
            error->Clear();
        return thread;
    }
    
    if (error)
        error->SetError (err, eErrorTypePOSIX);
    
    return LLDB_INVALID_HOST_THREAD;
}

#ifndef _WIN32

bool
Host::ThreadCancel (lldb::thread_t thread, Error *error)
{
    int err = ::pthread_cancel (thread);
    if (error)
        error->SetError(err, eErrorTypePOSIX);
    return err == 0;
}

bool
Host::ThreadDetach (lldb::thread_t thread, Error *error)
{
    int err = ::pthread_detach (thread);
    if (error)
        error->SetError(err, eErrorTypePOSIX);
    return err == 0;
}

bool
Host::ThreadJoin (lldb::thread_t thread, thread_result_t *thread_result_ptr, Error *error)
{
    int err = ::pthread_join (thread, thread_result_ptr);
    if (error)
        error->SetError(err, eErrorTypePOSIX);
    return err == 0;
}

lldb::thread_key_t
Host::ThreadLocalStorageCreate(ThreadLocalStorageCleanupCallback callback)
{
    pthread_key_t key;
    ::pthread_key_create (&key, callback);
    return key;
}

void*
Host::ThreadLocalStorageGet(lldb::thread_key_t key)
{
    return ::pthread_getspecific (key);
}

void
Host::ThreadLocalStorageSet(lldb::thread_key_t key, void *value)
{
   ::pthread_setspecific (key, value);
}

bool
Host::SetThreadName (lldb::pid_t pid, lldb::tid_t tid, const char *name)
{
#if defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
    lldb::pid_t curr_pid = Host::GetCurrentProcessID();
    lldb::tid_t curr_tid = Host::GetCurrentThreadID();
    if (pid == LLDB_INVALID_PROCESS_ID)
        pid = curr_pid;

    if (tid == LLDB_INVALID_THREAD_ID)
        tid = curr_tid;

    // Set the pthread name if possible
    if (pid == curr_pid && tid == curr_tid)
    {
        if (::pthread_setname_np (name) == 0)
            return true;
    }
    return false;
#elif defined (__FreeBSD__)
    lldb::pid_t curr_pid = Host::GetCurrentProcessID();
    lldb::tid_t curr_tid = Host::GetCurrentThreadID();
    if (pid == LLDB_INVALID_PROCESS_ID)
        pid = curr_pid;

    if (tid == LLDB_INVALID_THREAD_ID)
        tid = curr_tid;

    // Set the pthread name if possible
    if (pid == curr_pid && tid == curr_tid)
    {
        ::pthread_set_name_np (::pthread_self(), name);
        return true;
    }
    return false;
#elif defined (__linux__) || defined (__GLIBC__)
    void *fn = dlsym (RTLD_DEFAULT, "pthread_setname_np");
    if (fn)
    {
        lldb::pid_t curr_pid = Host::GetCurrentProcessID();
        lldb::tid_t curr_tid = Host::GetCurrentThreadID();
        if (pid == LLDB_INVALID_PROCESS_ID)
            pid = curr_pid;

        if (tid == LLDB_INVALID_THREAD_ID)
            tid = curr_tid;

        if (pid == curr_pid && tid == curr_tid)
        {
            int (*pthread_setname_np_func)(pthread_t thread, const char *name);
            *reinterpret_cast<void **> (&pthread_setname_np_func) = fn;

            if (pthread_setname_np_func (::pthread_self(), name) == 0)
                return true;
        }
    }
    return false;
#else
    return false;
#endif
}

bool
Host::SetShortThreadName (lldb::pid_t pid, lldb::tid_t tid,
                          const char *thread_name, size_t len)
{
    std::unique_ptr<char[]> namebuf(new char[len+1]);
    
    // Thread names are coming in like '<lldb.comm.debugger.edit>' and
    // '<lldb.comm.debugger.editline>'.  So just chopping the end of the string
    // off leads to a lot of similar named threads.  Go through the thread name
    // and search for the last dot and use that.
    const char *lastdot = ::strrchr (thread_name, '.');

    if (lastdot && lastdot != thread_name)
        thread_name = lastdot + 1;
    ::strncpy (namebuf.get(), thread_name, len);
    namebuf[len] = 0;

    int namebuflen = strlen(namebuf.get());
    if (namebuflen > 0)
    {
        if (namebuf[namebuflen - 1] == '(' || namebuf[namebuflen - 1] == '>')
        {
            // Trim off trailing '(' and '>' characters for a bit more cleanup.
            namebuflen--;
            namebuf[namebuflen] = 0;
        }
        return Host::SetThreadName (pid, tid, namebuf.get());
    }
    return false;
}

#endif

#if !defined (__APPLE__) // see Host.mm

bool
Host::GetBundleDirectory (const FileSpec &file, FileSpec &bundle)
{
    bundle.Clear();
    return false;
}

bool
Host::ResolveExecutableInBundle (FileSpec &file)
{
    return false;
}
#endif

#ifndef _WIN32

FileSpec
Host::GetModuleFileSpecForHostAddress (const void *host_addr)
{
    FileSpec module_filespec;
    Dl_info info;
    if (::dladdr (host_addr, &info))
    {
        if (info.dli_fname)
            module_filespec.SetFile(info.dli_fname, true);
    }
    return module_filespec;
}

#endif

#if !defined(__linux__)
bool
Host::FindProcessThreads (const lldb::pid_t pid, TidMap &tids_to_attach)
{
    return false;
}
#endif

lldb::TargetSP
Host::GetDummyTarget (lldb_private::Debugger &debugger)
{
    static TargetSP g_dummy_target_sp;

    Log *log(lldb_private::GetLogIfAnyCategoriesSet (LIBLLDB_LOG_HOST | LIBLLDB_LOG_TARGET));

    // FIXME: Maybe the dummy target should be per-Debugger
    if (!g_dummy_target_sp || !g_dummy_target_sp->IsValid())
    {
        ArchSpec arch(Target::GetDefaultArchitecture());
        if (!arch.IsValid())
            arch = HostInfo::GetArchitecture();
        Error error = debugger.GetTargetList().CreateTarget(debugger,
                                                          NULL,
                                                          arch.GetTriple().getTriple().c_str(),
                                                          false, 
                                                          NULL, 
                                                          g_dummy_target_sp);
        if (log)
        {
            if (error.Success ())
                log->Printf ("Host::%s created dummy target %p SUCCESS", __FUNCTION__, g_dummy_target_sp.get ());
            else
                log->Printf ("Host::%s failed to create dummy target: %s", __FUNCTION__, error.AsCString ());
        }
    }

    return g_dummy_target_sp;
}

struct ShellInfo
{
    ShellInfo () :
        process_reaped (false),
        can_delete (false),
        pid (LLDB_INVALID_PROCESS_ID),
        signo(-1),
        status(-1)
    {
    }

    lldb_private::Predicate<bool> process_reaped;
    lldb_private::Predicate<bool> can_delete;
    lldb::pid_t pid;
    int signo;
    int status;
};

static bool
MonitorShellCommand (void *callback_baton,
                     lldb::pid_t pid,
                     bool exited,       // True if the process did exit
                     int signo,         // Zero for no signal
                     int status)   // Exit value of process if signal is zero
{
    ShellInfo *shell_info = (ShellInfo *)callback_baton;
    shell_info->pid = pid;
    shell_info->signo = signo;
    shell_info->status = status;
    // Let the thread running Host::RunShellCommand() know that the process
    // exited and that ShellInfo has been filled in by broadcasting to it
    shell_info->process_reaped.SetValue(1, eBroadcastAlways);
    // Now wait for a handshake back from that thread running Host::RunShellCommand
    // so we know that we can delete shell_info_ptr
    shell_info->can_delete.WaitForValueEqualTo(true);
    // Sleep a bit to allow the shell_info->can_delete.SetValue() to complete...
    usleep(1000);
    // Now delete the shell info that was passed into this function
    delete shell_info;
    return true;
}

Error
Host::RunShellCommand (const char *command,
                       const char *working_dir,
                       int *status_ptr,
                       int *signo_ptr,
                       std::string *command_output_ptr,
                       uint32_t timeout_sec,
                       const char *shell)
{
    Error error;
    ProcessLaunchInfo launch_info;
    if (shell && shell[0])
    {
        // Run the command in a shell
        launch_info.SetShell(shell);
        launch_info.GetArguments().AppendArgument(command);
        const bool localhost = true;
        const bool will_debug = false;
        const bool first_arg_is_full_shell_command = true;
        launch_info.ConvertArgumentsForLaunchingInShell (error,
                                                         localhost,
                                                         will_debug,
                                                         first_arg_is_full_shell_command,
                                                         0);
    }
    else
    {
        // No shell, just run it
        Args args (command);
        const bool first_arg_is_executable = true;
        launch_info.SetArguments(args, first_arg_is_executable);
    }
    
    if (working_dir)
        launch_info.SetWorkingDirectory(working_dir);
    char output_file_path_buffer[PATH_MAX];
    const char *output_file_path = NULL;
    
    if (command_output_ptr)
    {
        // Create a temporary file to get the stdout/stderr and redirect the
        // output of the command into this file. We will later read this file
        // if all goes well and fill the data into "command_output_ptr"
        FileSpec tmpdir_file_spec;
        if (HostInfo::GetLLDBPath(ePathTypeLLDBTempSystemDir, tmpdir_file_spec))
        {
            tmpdir_file_spec.AppendPathComponent("lldb-shell-output.XXXXXX");
            strncpy(output_file_path_buffer, tmpdir_file_spec.GetPath().c_str(), sizeof(output_file_path_buffer));
        }
        else
        {
            strncpy(output_file_path_buffer, "/tmp/lldb-shell-output.XXXXXX", sizeof(output_file_path_buffer));
        }
        
        output_file_path = ::mktemp(output_file_path_buffer);
    }
    
    launch_info.AppendSuppressFileAction (STDIN_FILENO, true, false);
    if (output_file_path)
    {
        launch_info.AppendOpenFileAction(STDOUT_FILENO, output_file_path, false, true);
        launch_info.AppendDuplicateFileAction(STDOUT_FILENO, STDERR_FILENO);
    }
    else
    {
        launch_info.AppendSuppressFileAction (STDOUT_FILENO, false, true);
        launch_info.AppendSuppressFileAction (STDERR_FILENO, false, true);
    }
    
    // The process monitor callback will delete the 'shell_info_ptr' below...
    std::unique_ptr<ShellInfo> shell_info_ap (new ShellInfo());
    
    const bool monitor_signals = false;
    launch_info.SetMonitorProcessCallback(MonitorShellCommand, shell_info_ap.get(), monitor_signals);
    
    error = LaunchProcess (launch_info);
    const lldb::pid_t pid = launch_info.GetProcessID();

    if (error.Success() && pid == LLDB_INVALID_PROCESS_ID)
        error.SetErrorString("failed to get process ID");

    if (error.Success())
    {
        // The process successfully launched, so we can defer ownership of
        // "shell_info" to the MonitorShellCommand callback function that will
        // get called when the process dies. We release the unique pointer as it
        // doesn't need to delete the ShellInfo anymore.
        ShellInfo *shell_info = shell_info_ap.release();
        TimeValue *timeout_ptr = nullptr;
        TimeValue timeout_time(TimeValue::Now());
        if (timeout_sec > 0) {
            timeout_time.OffsetWithSeconds(timeout_sec);
            timeout_ptr = &timeout_time;
        }
        bool timed_out = false;
        shell_info->process_reaped.WaitForValueEqualTo(true, timeout_ptr, &timed_out);
        if (timed_out)
        {
            error.SetErrorString("timed out waiting for shell command to complete");

            // Kill the process since it didn't complete within the timeout specified
            Kill (pid, SIGKILL);
            // Wait for the monitor callback to get the message
            timeout_time = TimeValue::Now();
            timeout_time.OffsetWithSeconds(1);
            timed_out = false;
            shell_info->process_reaped.WaitForValueEqualTo(true, &timeout_time, &timed_out);
        }
        else
        {
            if (status_ptr)
                *status_ptr = shell_info->status;

            if (signo_ptr)
                *signo_ptr = shell_info->signo;

            if (command_output_ptr)
            {
                command_output_ptr->clear();
                FileSpec file_spec(output_file_path, File::eOpenOptionRead);
                uint64_t file_size = file_spec.GetByteSize();
                if (file_size > 0)
                {
                    if (file_size > command_output_ptr->max_size())
                    {
                        error.SetErrorStringWithFormat("shell command output is too large to fit into a std::string");
                    }
                    else
                    {
                        command_output_ptr->resize(file_size);
                        file_spec.ReadFileContents(0, &((*command_output_ptr)[0]), command_output_ptr->size(), &error);
                    }
                }
            }
        }
        shell_info->can_delete.SetValue(true, eBroadcastAlways);
    }

    if (output_file_path)
        ::unlink (output_file_path);
    // Handshake with the monitor thread, or just let it know in advance that
    // it can delete "shell_info" in case we timed out and were not able to kill
    // the process...
    return error;
}


// LaunchProcessPosixSpawn for Apple, Linux, FreeBSD and other GLIBC
// systems

#if defined (__APPLE__) || defined (__linux__) || defined (__FreeBSD__) || defined (__GLIBC__) || defined(__NetBSD__)

// this method needs to be visible to macosx/Host.cpp and
// common/Host.cpp.

short
Host::GetPosixspawnFlags (ProcessLaunchInfo &launch_info)
{
    short flags = POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK;

#if defined (__APPLE__)
    if (launch_info.GetFlags().Test (eLaunchFlagExec))
        flags |= POSIX_SPAWN_SETEXEC;           // Darwin specific posix_spawn flag
    
    if (launch_info.GetFlags().Test (eLaunchFlagDebug))
        flags |= POSIX_SPAWN_START_SUSPENDED;   // Darwin specific posix_spawn flag
    
    if (launch_info.GetFlags().Test (eLaunchFlagDisableASLR))
        flags |= _POSIX_SPAWN_DISABLE_ASLR;     // Darwin specific posix_spawn flag
        
    if (launch_info.GetLaunchInSeparateProcessGroup())
        flags |= POSIX_SPAWN_SETPGROUP;
    
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
#if defined (__APPLE__) && (defined (__x86_64__) || defined (__i386__))
    static LazyBool g_use_close_on_exec_flag = eLazyBoolCalculate;
    if (g_use_close_on_exec_flag == eLazyBoolCalculate)
    {
        g_use_close_on_exec_flag = eLazyBoolNo;
        
        uint32_t major, minor, update;
        if (HostInfo::GetOSVersion(major, minor, update))
        {
            // Kernel panic if we use the POSIX_SPAWN_CLOEXEC_DEFAULT on 10.7 or earlier
            if (major > 10 || (major == 10 && minor > 7))
            {
                // Only enable for 10.8 and later OS versions
                g_use_close_on_exec_flag = eLazyBoolYes;
            }
        }
    }
#else
    static LazyBool g_use_close_on_exec_flag = eLazyBoolYes;
#endif
    // Close all files exception those with file actions if this is supported.
    if (g_use_close_on_exec_flag == eLazyBoolYes)
        flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
#endif // #if defined (__APPLE__)
    return flags;
}

Error
Host::LaunchProcessPosixSpawn (const char *exe_path, ProcessLaunchInfo &launch_info, ::pid_t &pid)
{
    Error error;
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_HOST | LIBLLDB_LOG_PROCESS));

    posix_spawnattr_t attr;
    error.SetError( ::posix_spawnattr_init (&attr), eErrorTypePOSIX);

    if (error.Fail() || log)
        error.PutToLog(log, "::posix_spawnattr_init ( &attr )");
    if (error.Fail())
        return error;

    // Make a quick class that will cleanup the posix spawn attributes in case
    // we return in the middle of this function.
    lldb_utility::CleanUp <posix_spawnattr_t *, int> posix_spawnattr_cleanup(&attr, posix_spawnattr_destroy);

    sigset_t no_signals;
    sigset_t all_signals;
    sigemptyset (&no_signals);
    sigfillset (&all_signals);
    ::posix_spawnattr_setsigmask(&attr, &no_signals);
#if defined (__linux__)  || defined (__FreeBSD__)
    ::posix_spawnattr_setsigdefault(&attr, &no_signals);
#else
    ::posix_spawnattr_setsigdefault(&attr, &all_signals);
#endif

    short flags = GetPosixspawnFlags(launch_info);
    SpawnFunction spawn_function_p = GetPosixspawnFunction (launch_info);

    error.SetError( ::posix_spawnattr_setflags (&attr, flags), eErrorTypePOSIX);
    if (error.Fail() || log)
        error.PutToLog(log, "::posix_spawnattr_setflags ( &attr, flags=0x%8.8x )", flags);
    if (error.Fail())
        return error;

    // posix_spawnattr_setbinpref_np appears to be an Apple extension per:
    // http://www.unix.com/man-page/OSX/3/posix_spawnattr_setbinpref_np/
#if defined (__APPLE__) && !defined (__arm__)
    
    // Don't set the binpref if a shell was provided.  After all, that's only going to affect what version of the shell
    // is launched, not what fork of the binary is launched.  We insert "arch --arch <ARCH> as part of the shell invocation
    // to do that job on OSX.
    
    if (launch_info.GetShell() == nullptr)
    {
        // We don't need to do this for ARM, and we really shouldn't now that we
        // have multiple CPU subtypes and no posix_spawnattr call that allows us
        // to set which CPU subtype to launch...
        const ArchSpec &arch_spec = launch_info.GetArchitecture();
        cpu_type_t cpu = arch_spec.GetMachOCPUType();
        cpu_type_t sub = arch_spec.GetMachOCPUSubType();
        if (cpu != 0 &&
            cpu != static_cast<cpu_type_t>(UINT32_MAX) &&
            cpu != static_cast<cpu_type_t>(LLDB_INVALID_CPUTYPE) &&
            !(cpu == 0x01000007 && sub == 8)) // If haswell is specified, don't try to set the CPU type or we will fail 
        {
            size_t ocount = 0;
            error.SetError( ::posix_spawnattr_setbinpref_np (&attr, 1, &cpu, &ocount), eErrorTypePOSIX);
            if (error.Fail() || log)
                error.PutToLog(log, "::posix_spawnattr_setbinpref_np ( &attr, 1, cpu_type = 0x%8.8x, count => %llu )", cpu, (uint64_t)ocount);

            if (error.Fail() || ocount != 1)
                return error;
        }
    }

#endif

    const char *tmp_argv[2];
    char * const *argv = (char * const*)launch_info.GetArguments().GetConstArgumentVector();
    char * const *envp = (char * const*)launch_info.GetEnvironmentEntries().GetConstArgumentVector();
    if (argv == NULL)
    {
        // posix_spawn gets very unhappy if it doesn't have at least the program
        // name in argv[0]. One of the side affects I have noticed is the environment
        // variables don't make it into the child process if "argv == NULL"!!!
        tmp_argv[0] = exe_path;
        tmp_argv[1] = NULL;
        argv = (char * const*)tmp_argv;
    }

#if !defined (__APPLE__)
    // manage the working directory
    char current_dir[PATH_MAX];
    current_dir[0] = '\0';
#endif

    const char *working_dir = launch_info.GetWorkingDirectory();
    if (working_dir)
    {
#if defined (__APPLE__)
        // Set the working directory on this thread only
        if (__pthread_chdir (working_dir) < 0) {
            if (errno == ENOENT) {
                error.SetErrorStringWithFormat("No such file or directory: %s", working_dir);
            } else if (errno == ENOTDIR) {
                error.SetErrorStringWithFormat("Path doesn't name a directory: %s", working_dir);
            } else {
                error.SetErrorStringWithFormat("An unknown error occurred when changing directory for process execution.");
            }
            return error;
        }
#else
        if (::getcwd(current_dir, sizeof(current_dir)) == NULL)
        {
            error.SetError(errno, eErrorTypePOSIX);
            error.LogIfError(log, "unable to save the current directory");
            return error;
        }

        if (::chdir(working_dir) == -1)
        {
            error.SetError(errno, eErrorTypePOSIX);
            error.LogIfError(log, "unable to change working directory to %s", working_dir);
            return error;
        }
#endif
    }

    const size_t num_file_actions = launch_info.GetNumFileActions ();
    if (num_file_actions > 0)
    {
        posix_spawn_file_actions_t file_actions;
        error.SetError( ::posix_spawn_file_actions_init (&file_actions), eErrorTypePOSIX);
        if (error.Fail() || log)
            error.PutToLog(log, "::posix_spawn_file_actions_init ( &file_actions )");
        if (error.Fail())
            return error;

        // Make a quick class that will cleanup the posix spawn attributes in case
        // we return in the middle of this function.
        lldb_utility::CleanUp <posix_spawn_file_actions_t *, int> posix_spawn_file_actions_cleanup (&file_actions, posix_spawn_file_actions_destroy);

        for (size_t i=0; i<num_file_actions; ++i)
        {
            const FileAction *launch_file_action = launch_info.GetFileActionAtIndex(i);
            if (launch_file_action)
            {
                if (!AddPosixSpawnFileAction(&file_actions, launch_file_action, log, error))
                    return error;
            }
        }

        error.SetError ((*spawn_function_p) (&pid,
                                        exe_path,
                                        &file_actions,
                                        &attr,
                                        argv,
                                        envp,
                                        launch_info.GetLaunchSyncPipe ()),
                        eErrorTypePOSIX);

        if (error.Fail() || log)
        {
            error.PutToLog(log, "::posix_spawnp ( pid => %i, path = '%s', file_actions = %p, attr = %p, argv = %p, envp = %p )",
                           pid, exe_path, static_cast<void*>(&file_actions),
                           static_cast<void*>(&attr),
                           reinterpret_cast<const void*>(argv),
                           reinterpret_cast<const void*>(envp));
            if (log)
            {
                for (int ii=0; argv[ii]; ++ii)
                    log->Printf("argv[%i] = '%s'", ii, argv[ii]);
            }
        }

    }
    else
    {
        error.SetError ((*spawn_function_p) (&pid,
                                        exe_path,
                                        NULL,
                                        &attr,
                                        argv,
                                        envp,
                                        launch_info.GetLaunchSyncPipe()),
                        eErrorTypePOSIX);

        if (error.Fail() || log)
        {
            error.PutToLog(log, "::posix_spawnp ( pid => %i, path = '%s', file_actions = NULL, attr = %p, argv = %p, envp = %p )",
                           pid, exe_path, static_cast<void*>(&attr),
                           reinterpret_cast<const void*>(argv),
                           reinterpret_cast<const void*>(envp));
            if (log)
            {
                for (int ii=0; argv[ii]; ++ii)
                    log->Printf("argv[%i] = '%s'", ii, argv[ii]);
            }
        }
    }

    if (working_dir)
    {
#if defined (__APPLE__)
        // No more thread specific current working directory
        __pthread_fchdir (-1);
#else
        if (::chdir(current_dir) == -1 && error.Success())
        {
            error.SetError(errno, eErrorTypePOSIX);
            error.LogIfError(log, "unable to change current directory back to %s",
                    current_dir);
        }
#endif
    }

    return error;
}

bool
Host::AddPosixSpawnFileAction(void *_file_actions, const FileAction *info, Log *log, Error &error)
{
    if (info == NULL)
        return false;

    posix_spawn_file_actions_t *file_actions = reinterpret_cast<posix_spawn_file_actions_t *>(_file_actions);

    switch (info->GetAction())
    {
        case FileAction::eFileActionNone:
            error.Clear();
            break;

        case FileAction::eFileActionClose:
            if (info->GetFD() == -1)
                error.SetErrorString("invalid fd for posix_spawn_file_actions_addclose(...)");
            else
            {
                error.SetError(::posix_spawn_file_actions_addclose(file_actions, info->GetFD()), eErrorTypePOSIX);
                if (log && (error.Fail() || log))
                    error.PutToLog(log, "posix_spawn_file_actions_addclose (action=%p, fd=%i)",
                                   static_cast<void *>(file_actions), info->GetFD());
            }
            break;

        case FileAction::eFileActionDuplicate:
            if (info->GetFD() == -1)
                error.SetErrorString("invalid fd for posix_spawn_file_actions_adddup2(...)");
            else if (info->GetActionArgument() == -1)
                error.SetErrorString("invalid duplicate fd for posix_spawn_file_actions_adddup2(...)");
            else
            {
                error.SetError(
                    ::posix_spawn_file_actions_adddup2(file_actions, info->GetFD(), info->GetActionArgument()),
                    eErrorTypePOSIX);
                if (log && (error.Fail() || log))
                    error.PutToLog(log, "posix_spawn_file_actions_adddup2 (action=%p, fd=%i, dup_fd=%i)",
                                   static_cast<void *>(file_actions), info->GetFD(), info->GetActionArgument());
            }
            break;

        case FileAction::eFileActionOpen:
            if (info->GetFD() == -1)
                error.SetErrorString("invalid fd in posix_spawn_file_actions_addopen(...)");
            else
            {
                int oflag = info->GetActionArgument();

                mode_t mode = 0;

                if (oflag & O_CREAT)
                    mode = 0640;

                error.SetError(
                    ::posix_spawn_file_actions_addopen(file_actions, info->GetFD(), info->GetPath(), oflag, mode),
                    eErrorTypePOSIX);
                if (error.Fail() || log)
                    error.PutToLog(log,
                                   "posix_spawn_file_actions_addopen (action=%p, fd=%i, path='%s', oflag=%i, mode=%i)",
                                   static_cast<void *>(file_actions), info->GetFD(), info->GetPath(), oflag, mode);
            }
            break;
    }
    return error.Success();
}

#endif // LaunchProcedssPosixSpawn: Apple, Linux, FreeBSD and other GLIBC systems

#if defined(__linux__) || defined(__FreeBSD__) || defined(__GLIBC__) || defined(__NetBSD__)

static const char *
FileActionAsCString (FileAction::Action action)
{
    switch (action)
    {
        case FileAction::eFileActionNone:
            return "eFileActionNone";
        case FileAction::eFileActionClose:
            return "eFileActionClose";
        case FileAction::eFileActionDuplicate:
            return "eFileActionDuplicate";
        case FileAction::eFileActionOpen:
            return "eFileActionOpen";
    }
    return "<unknown>";
}

static void
DuplicateFilePOSIX (int old_fd, int new_fd)
{
    Log *log(lldb_private::GetLogIfAnyCategoriesSet (LIBLLDB_LOG_HOST | LIBLLDB_LOG_PROCESS));

    if (dup2(old_fd, new_fd) == -1)
    {
        if (log)
        {
            Error error;
            error.SetErrorToErrno ();
            log->Printf ("Host::%s pid %" PRIu64 " failed to dup2 old fd %d to new fd %d: %s", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), old_fd, new_fd, error.AsCString ());
        }
    }
    else
    {
        if (log)
            log->Printf ("Host::%s pid %" PRIu64 " duplicate pid (old_fd=%d, new_fd=%d): SUCCESS", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), old_fd, new_fd);
    }
}

Error
Host::LaunchProcessForkPipeExec (const char *exe_path, ProcessLaunchInfo &launch_info, ::pid_t &pid)
{
    Log *log(lldb_private::GetLogIfAnyCategoriesSet (LIBLLDB_LOG_HOST | LIBLLDB_LOG_PROCESS));
    Error error;

    // We'll use a fork/exec style here.  First we'll fork, then have the parent return
    // success.  The child will set itself up to exec, and will do a read on the sync_pipe_sp right
    // before it execs.  It will block reading one character before it execs.
    PipeSP &sync_pipe_sp = launch_info.GetLaunchSyncPipe ();
    assert (!sync_pipe_sp && "pipe was already created");

    sync_pipe_sp.reset (new Pipe ());
    if (! sync_pipe_sp->Open ())
    {
        error.SetErrorToErrno();
        if (log)
            log->Printf ("Host::%s: launch sync pipe open FAILED: %s", __FUNCTION__, error.AsCString ());
        return error;
    }
    else
    {
        if (log)
            log->Printf ("Host::%s: launch sync pipe open SUCCESS", __FUNCTION__);
    }

    if (log)
        log->Printf ("Host::%s: calling fork()", __FUNCTION__);

    const ::pid_t fork_pid = fork ();
    if (fork_pid == -1)
    {
        error.SetErrorToErrno();
        if (log)
            log->Printf ("Host::%s: fork() call failed: %s", __FUNCTION__, error.AsCString ());
        return error;
    }

    if (fork_pid != 0)
    {
        // Parent.
        if (log)
            log->Printf ("Host::%s: fork() call - parent returning after successful fork, child pid %" PRIu64, __FUNCTION__, static_cast<lldb::pid_t> (fork_pid));

        // Close the read side of the pipe.
        if (!sync_pipe_sp->CloseReadFileDescriptor ())
        {
            error.SetErrorToErrno ();
            if (log)
                log->Printf ("Host::%s: launch sync pipe close the read side in parent FAILED: %s", __FUNCTION__, error.AsCString ());
            // Don't fail the launch because of this.
        }
        else
        {
            if (log)
                log->Printf ("Host::%s: launch sync pipe close the read side in parent SUCCESS", __FUNCTION__);
        }

        pid = fork_pid;
        return error;
    }

    // We're the child.
    if (log)
        log->Printf ("Host::%s: child pid %" PRIu64 " initiating synchronized exec process", __FUNCTION__, static_cast<lldb::pid_t> (getpid()));

    // Close the write side of the launch sync pipe.
    if (!sync_pipe_sp->CloseWriteFileDescriptor ())
    {
        error.SetErrorToErrno ();
        if (log)
            log->Printf ("Host::%s: launch sync pipe child close write side FAILED: %s", __FUNCTION__, error.AsCString ());
        // Don't fail the launch because of this.
    }
    else
    {
        if (log)
            log->Printf ("Host::%s: launch sync pipe child close write side SUCCESS", __FUNCTION__);
    }

    //
    // Set up the launch environment.
    //

    // FIXME set up signals properly.
#if 0
    sigset_t no_signals;
    sigset_t all_signals;
    sigemptyset (&no_signals);
    sigfillset (&all_signals);
    ::posix_spawnattr_setsigmask(&attr, &no_signals);
#if defined (__linux__)  || defined (__FreeBSD__)
    ::posix_spawnattr_setsigdefault(&attr, &no_signals);
#else
    ::posix_spawnattr_setsigdefault(&attr, &all_signals);
#endif
#endif

    // Setup args.
    const char *tmp_argv[2];
    char * const *argv = (char * const*)launch_info.GetArguments ().GetConstArgumentVector ();
    if (argv == NULL)
    {
        // posix_spawn gets very unhappy if it doesn't have at least the program
        // name in argv[0]. One of the side affects I have noticed is the environment
        // variables don't make it into the child process if "argv == NULL"!!!
        tmp_argv[0] = exe_path;
        tmp_argv[1] = NULL;
        argv = (char * const*)tmp_argv;
    }

    // Setup envp.
    char * const *envp = (char * const*)launch_info.GetEnvironmentEntries ().GetConstArgumentVector ();

    // Working directory.
    // Change the working directory as needed.  This is fine to do, we're in the child forked process and will never come back.
    const char *const working_dir = launch_info.GetWorkingDirectory();
    if (working_dir)
    {
        if (::chdir (working_dir) == -1)
        {
            error.SetErrorToErrno ();
            if (log)
            {
                char current_dir[PATH_MAX];
                current_dir[0] = '\0';
                ::getcwd (current_dir, sizeof(current_dir));

                log->Printf ("Host::%s forked child %" PRIu64 " failed to change working directory to '%s', continuing with current directory '%s'", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), working_dir, current_dir);
            }
            // There is no meaningful way to abort at this point. Just continue without changing the working dir. Noted in the log message.
        }
        else
        {
            if (log)
                log->Printf ("Host::%s forked child pid %" PRIu64 " changed working directory to '%s'", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), working_dir);
        }
    }
    else
    {
        if (log)
        {
            char current_dir[PATH_MAX];
            current_dir[0] = '\0';
            ::getcwd (current_dir, sizeof(current_dir));

            log->Printf ("Host::%s forked child pid %" PRIu64 " using same working directory as parent: '%s'", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), current_dir);
        }
    }

    // File actions.
    for (size_t i = 0; i < launch_info.GetNumFileActions (); ++i)
    {
        const FileAction *action = launch_info.GetFileActionAtIndex(i);
        if (action)
        {
            if (log)
                log->Printf ("Host::%s forked child pid %" PRIu64 " handling file action %s (%d) - fd %d, arg %d, path '%s'", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), FileActionAsCString (action->GetAction ()), action->GetAction (), action->GetFD (), action->GetActionArgument (), action->GetPath () ? action->GetPath () : "<null>");

            switch (action->GetAction ())
            {
            case FileAction::eFileActionNone:
                // Nothing to do.
                break;

            case FileAction::eFileActionClose:
                if (close (action->GetFD ()) != 0)
                {
                    if (log)
                    {
                        error.SetErrorToErrno ();
                        log->Printf ("Host::%s forked child failed to close fd %d: %s", __FUNCTION__, action->GetFD (), error.AsCString ());
                    }
                }
                else
                {
                    if (log)
                        log->Printf ("Host::%s forked child pid %" PRIu64 " close fd %d: SUCCESS", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), action->GetFD ());

                }
                break;

            case FileAction::eFileActionDuplicate:
                DuplicateFilePOSIX (action->GetFD (), action->GetActionArgument ());
                break;

            case FileAction::eFileActionOpen:
                {
                    assert (action->GetPath () && "launch_info file open action with no path");

                    // Open the file specified.
                    const int new_fd = action->GetFD ();
                    assert (new_fd > -1 && "invalid file descriptor specified");
                    if (new_fd <= -1)
                    {
                        if (log)
                            log->Printf ("Host::%s invalid file descriptor specified for file action: %d", __FUNCTION__, new_fd);

                        // FIXME do error checking before the fork, so we can meaningfully report back to the caller.  This is too late to be catching this, we've already forked.
                        // For now, just kill the forked child.
                        exit (-1);
                    }

                    // The open() mode arg is ignored unless we do a create.
                    const int open_flags = action->GetActionArgument ();
                    const mode_t mode = (open_flags & O_CREAT) ? 0640 : 0;

                    const int opened_fd = open(action->GetPath (), open_flags, mode);
                    if (opened_fd == -1)
                    {
                        error.SetErrorToErrno ();
                        if (log)
                            log->Printf ("Host::%s forked child failed to open file '%s': %s", __FUNCTION__, action->GetPath (), error.AsCString ());
                    }
                    else
                    {
                        if (dup2 (opened_fd, new_fd) == -1)
                        {
                            error.SetErrorToErrno ();
                            if (log)
                                log->Printf ("Host::%s forked child failed to dup2 file '%s' (fd %d) to new fd %d: %s", __FUNCTION__, action->GetPath (), opened_fd, action->GetFD (), error.AsCString ());
                        }
                        else
                        {
                            if (log)
                                log->Printf ("Host::%s forked child pid %" PRIu64 " open file (new_fd=%d, path='%s'): SUCCESS", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), action->GetFD (), action->GetPath ());
                        }
                    }
                }
                break;
            }
        }
    }


    // For some more recent Linux kernels and distributions,
    // we need to enable a process other than the parent -- i.e.
    // llgs is not the parent of this launched inferior, but a
    // sibling -- to ptrace via a prctl mechanism.
#if defined (__linux__)
#if defined (PR_SET_PTRACER)
    // Attempt to set the permitted ptracer to our parent process (lldb, which we just
    // forked off of) so that llgs, which will be a sibling, can ptrace us later.
    const ::pid_t parent_pid = getppid();
    const int prctl_result = prctl (PR_SET_PTRACER, static_cast<unsigned long>(parent_pid), 0, 0, 0);
    if (prctl_result != 0)
    {
        error.SetErrorToErrno();
        if (log)
            log->Printf ("Host::%s: prctl (PR_SET_PTRACER,%" PRIu64 ",...) FAILED (ignored): %s", __FUNCTION__, static_cast<uint64_t> (parent_pid), error.AsCString ());
        // Don't bail here in case we're calling it on a system combo that doesn't need this.
        // Ubuntu 10.10+ claims it needs it, even though the standard way to check for it in
        // procfs is showing 0 (i.e. disabled) on stock systems.
    }
    else
    {
        if (log)
            log->Printf ("Host::%s: prctl (PR_SET_PTRACER,%" PRIu64 ",...) SUCCESS", __FUNCTION__, static_cast<uint64_t> (parent_pid));
    }
#else
    if (log)
        log->Printf ("Host::%s: prctl (PR_SET_PTRACER,...) skipped because PR_SET_PTRACER is not defined", __FUNCTION__);
#endif // #if defined (PR_SET_PTRACER)
#endif // #if defined (__linux__)

    // Sync with llgs/attacher via pipe.

    // Now wait for a read of 1 byte on the launch sync pipe.
    // Our parent (lldb) will send a byte here once we've been
    // successfully attached to by the local llgs.  We need
    // this synchronization to enable the launched inferior to
    // be debugged from the start of the program.
    // This is the child.  Wait now for something to PTRACE us.
    uint8_t sync_byte;
    if (!sync_pipe_sp->Read (&sync_byte, 1))
    {
        error.SetErrorToErrno();
        if (log)
            log->Printf ("Host::%s: launch sync pipe read byte FAILED, canceling launch: %s", __FUNCTION__, error.AsCString ());
        exit (-1);
    }
    else
    {
        if (log)
            log->Printf ("Host::%s: launch sync pipe read byte SUCCESS", __FUNCTION__);
    }

    // Close the read side of the pipe.
    if (!sync_pipe_sp->CloseReadFileDescriptor())
    {
        error.SetErrorToErrno();
        if (log)
            log->Printf ("Host::%s: launch sync pipe child close read side after sync FAILED: %s", __FUNCTION__, error.AsCString ());
        // Don't fail the launch because of this.
    }
    else
    {
        if (log)
            log->Printf ("Host::%s: launch sync pipe child close read side after sync SUCCESS", __FUNCTION__);
    }

    //
    // Exec.
    //
    if (log)
    {
        log->Printf ("Host::%s: calling execve('%s', argv, envp), argv/envp follows", __FUNCTION__, exe_path ? exe_path : "<null>");
        if (argv)
        {
            for (int i = 0; argv[i] != nullptr; ++i)
                log->Printf ("-- argv[%d]: %s", i, argv[i]);
        }
        if (envp)
        {
            for (int i = 0; envp[i] != nullptr; ++i)
                log->Printf ("-- envp[%d]: %s", i, envp[i]);
        }
    }

    const int exec_result = execve (exe_path, argv, envp);

    // If we get this far, we didn't successfully exec.  Log and bail.
    if (exec_result != 0)
    {
        const int exec_errno = errno;
        if (log)
            log->Printf ("%s: forked pid %" PRIu64 " failed to execve() with path '%s': %s\n", __FUNCTION__, static_cast<lldb::pid_t> (getpid ()), exe_path ? exe_path : "<null> path", strerror (exec_errno));
    }
    exit (-1);
}

#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__GLIBC__) || defined(__NetBSD__)
// The functions below implement process launching via posix_spawn() for Linux,
// FreeBSD and NetBSD.

Error
Host::LaunchProcess (ProcessLaunchInfo &launch_info)
{
    Error error;
    char exe_path[PATH_MAX];
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));

    PlatformSP host_platform_sp (Platform::GetDefaultPlatform ());

    const ArchSpec &arch_spec = launch_info.GetArchitecture();

    FileSpec exe_spec(launch_info.GetExecutableFile());

    FileSpec::FileType file_type = exe_spec.GetFileType();
    if (file_type != FileSpec::eFileTypeRegular)
    {
        lldb::ModuleSP exe_module_sp;
        error = host_platform_sp->ResolveExecutable (exe_spec,
                                                     arch_spec,
                                                     exe_module_sp,
                                                     NULL);

        if (error.Fail())
            return error;

        if (exe_module_sp)
            exe_spec = exe_module_sp->GetFileSpec();
    }

    if (exe_spec.Exists())
    {
        exe_spec.GetPath (exe_path, sizeof(exe_path));
    }
    else
    {
        launch_info.GetExecutableFile().GetPath (exe_path, sizeof(exe_path));
        error.SetErrorStringWithFormat ("executable doesn't exist: '%s'", exe_path);
        return error;
    }

    assert(!launch_info.GetFlags().Test (eLaunchFlagLaunchInTTY));

    ::pid_t pid = LLDB_INVALID_PROCESS_ID;

    if (launch_info.GetFlags().Test (eLaunchFlagDebug))
    {
        // We need to launch using the launch pipe sync approach.
        // Note MacOSX doesn't need this because they have a special flag they can
        // pass to the posix spawn routine.
        error = LaunchProcessForkPipeExec(exe_path, launch_info, pid);
    }
    else
    {
        // Use a stock posix spawn mechanism.
        error = LaunchProcessPosixSpawn(exe_path, launch_info, pid);
    }

    if (pid != LLDB_INVALID_PROCESS_ID)
    {
        // If all went well, then set the process ID into the launch info
        launch_info.SetProcessID(pid);

        // Make sure we reap any processes we spawn or we will have zombies.
        if (!launch_info.MonitorProcess())
        {
            const bool monitor_signals = false;
            Host::MonitorChildProcessCallback callback = nullptr;

            if (!launch_info.GetFlags().Test(lldb::eLaunchFlagDontSetExitStatus))
            {
                callback = Process::SetProcessExitStatus;
                if (log)
                    log->Printf ("Host::%s monitored child process %s.", __FUNCTION__, callback ? "with Process::SetProcessExitStatus() callback" : "with no callback");

                StartMonitoringChildProcess (callback,
                                             NULL,
                                             pid,
                                             monitor_signals);
            }
            else
            {
                if (log)
                    log->Printf ("Host::%s skipping monitoring of child process --- we can't have stub and lldb both check wait status.", __FUNCTION__);
            }

        }
        else
        {
            if (log)
                log->Printf ("Host::%s monitored child process with user-specified process monitor.", __FUNCTION__);
        }
    }
    else
    {
        // Invalid process ID, something didn't go well.
        if (error.Success())
            error.SetErrorString ("process launch failed for unknown reasons");
    }
    return error;
}

#endif // defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)

#ifndef _WIN32

void
Host::Kill(lldb::pid_t pid, int signo)
{
    ::kill(pid, signo);
}

#endif

#if !defined (__APPLE__)
bool
Host::OpenFileInExternalEditor (const FileSpec &file_spec, uint32_t line_no)
{
    return false;
}

void
Host::SetCrashDescriptionWithFormat (const char *format, ...)
{
}

void
Host::SetCrashDescription (const char *description)
{
}

lldb::pid_t
Host::LaunchApplication (const FileSpec &app_file_spec)
{
    return LLDB_INVALID_PROCESS_ID;
}

#endif

#if !defined (__linux__) && !defined (__FreeBSD__) && !defined (__NetBSD__)

const lldb_private::UnixSignalsSP&
Host::GetUnixSignals ()
{
    static UnixSignalsSP s_unix_signals_sp (new UnixSignals ());
    return s_unix_signals_sp;
}

#endif
