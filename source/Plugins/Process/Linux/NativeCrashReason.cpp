//===-- NativeCrashReason.cpp -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NativeCrashReason.h"

namespace
{

CrashReason
GetCrashReasonForSIGSEGV(const siginfo_t& info)
{
    assert(info.si_signo == SIGSEGV);

    switch (info.si_code)
    {
    case SI_KERNEL:
        // Linux will occasionally send spurious SI_KERNEL codes.
        // (this is poorly documented in sigaction)
        // One way to get this is via unaligned SIMD loads.
        return CrashReason::eInvalidAddress; // for lack of anything better
    case SEGV_MAPERR:
        return CrashReason::eInvalidAddress;
    case SEGV_ACCERR:
        return CrashReason::ePrivilegedAddress;
    }

    assert(false && "unexpected si_code for SIGSEGV");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGILL(const siginfo_t& info)
{
    assert(info.si_signo == SIGILL);

    switch (info.si_code)
    {
    case ILL_ILLOPC:
        return CrashReason::eIllegalOpcode;
    case ILL_ILLOPN:
        return CrashReason::eIllegalOperand;
    case ILL_ILLADR:
        return CrashReason::eIllegalAddressingMode;
    case ILL_ILLTRP:
        return CrashReason::eIllegalTrap;
    case ILL_PRVOPC:
        return CrashReason::ePrivilegedOpcode;
    case ILL_PRVREG:
        return CrashReason::ePrivilegedRegister;
    case ILL_COPROC:
        return CrashReason::eCoprocessorError;
    case ILL_BADSTK:
        return CrashReason::eInternalStackError;
    }

    assert(false && "unexpected si_code for SIGILL");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGFPE(const siginfo_t& info)
{
    assert(info.si_signo == SIGFPE);

    switch (info.si_code)
    {
    case FPE_INTDIV:
        return CrashReason::eIntegerDivideByZero;
    case FPE_INTOVF:
        return CrashReason::eIntegerOverflow;
    case FPE_FLTDIV:
        return CrashReason::eFloatDivideByZero;
    case FPE_FLTOVF:
        return CrashReason::eFloatOverflow;
    case FPE_FLTUND:
        return CrashReason::eFloatUnderflow;
    case FPE_FLTRES:
        return CrashReason::eFloatInexactResult;
    case FPE_FLTINV:
        return CrashReason::eFloatInvalidOperation;
    case FPE_FLTSUB:
        return CrashReason::eFloatSubscriptRange;
    }

    assert(false && "unexpected si_code for SIGFPE");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGBUS(const siginfo_t& info)
{
    assert(info.si_signo == SIGBUS);

    switch (info.si_code)
    {
    case BUS_ADRALN:
        return CrashReason::eIllegalAlignment;
    case BUS_ADRERR:
        return CrashReason::eIllegalAddress;
    case BUS_OBJERR:
        return CrashReason::eHardwareError;
    }

    assert(false && "unexpected si_code for SIGBUS");
    return CrashReason::eInvalidCrashReason;
}

}

CrashReason
lldb_private::GetCrashReason(const siginfo_t& info)
{
    switch(info.si_signo)
    {
    case SIGSEGV:
        return GetCrashReasonForSIGSEGV(info);
    case SIGBUS:
        return GetCrashReasonForSIGBUS(info);
    case SIGFPE:
        return GetCrashReasonForSIGFPE(info);
    case SIGILL:
        return GetCrashReasonForSIGILL(info);
    }

    assert(false && "unexpected signal");
    return CrashReason::eInvalidCrashReason;
}
