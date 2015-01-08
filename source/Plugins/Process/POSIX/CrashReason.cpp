//===-- CrashReason.cpp -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CrashReason.h"

#include <sstream>

namespace {

void
AppendFaultAddr (std::string& str, lldb::addr_t addr)
{
    std::stringstream ss;
    ss << " (fault address: 0x" << std::hex << addr << ")";
    str += ss.str();
}

}

std::string
GetCrashReasonString (CrashReason reason, lldb::addr_t fault_addr)
{
    std::string str;

    switch (reason)
    {
    default:
        assert(false && "invalid CrashReason");
        break;

    case CrashReason::eInvalidAddress:
        str = "invalid address";
        AppendFaultAddr (str, fault_addr);
        break;
    case CrashReason::ePrivilegedAddress:
        str = "address access protected";
        AppendFaultAddr (str, fault_addr);
        break;
    case CrashReason::eIllegalOpcode:
        str = "illegal instruction";
        break;
    case CrashReason::eIllegalOperand:
        str = "illegal instruction operand";
        break;
    case CrashReason::eIllegalAddressingMode:
        str = "illegal addressing mode";
        break;
    case CrashReason::eIllegalTrap:
        str = "illegal trap";
        break;
    case CrashReason::ePrivilegedOpcode:
        str = "privileged instruction";
        break;
    case CrashReason::ePrivilegedRegister:
        str = "privileged register";
        break;
    case CrashReason::eCoprocessorError:
        str = "coprocessor error";
        break;
    case CrashReason::eInternalStackError:
        str = "internal stack error";
        break;
    case CrashReason::eIllegalAlignment:
        str = "illegal alignment";
        break;
    case CrashReason::eIllegalAddress:
        str = "illegal address";
        break;
    case CrashReason::eHardwareError:
        str = "hardware error";
        break;
    case CrashReason::eIntegerDivideByZero:
        str = "integer divide by zero";
        break;
    case CrashReason::eIntegerOverflow:
        str = "integer overflow";
        break;
    case CrashReason::eFloatDivideByZero:
        str = "floating point divide by zero";
        break;
    case CrashReason::eFloatOverflow:
        str = "floating point overflow";
        break;
    case CrashReason::eFloatUnderflow:
        str = "floating point underflow";
        break;
    case CrashReason::eFloatInexactResult:
        str = "inexact floating point result";
        break;
    case CrashReason::eFloatInvalidOperation:
        str = "invalid floating point operation";
        break;
    case CrashReason::eFloatSubscriptRange:
        str = "invalid floating point subscript range";
        break;
    }

    return str;
}

const char *
CrashReasonAsString (CrashReason reason)
{
#ifdef LLDB_CONFIGURATION_BUILDANDINTEGRATION
    // Just return the code in ascii for integration builds.
    chcar str[8];
    sprintf(str, "%d", reason);
#else
    const char *str = nullptr;

    switch (reason)
    {
    case CrashReason::eInvalidCrashReason:
        str = "eInvalidCrashReason";
        break;

    // SIGSEGV crash reasons.
    case CrashReason::eInvalidAddress:
        str = "eInvalidAddress";
        break;
    case CrashReason::ePrivilegedAddress:
        str = "ePrivilegedAddress";
        break;

    // SIGILL crash reasons.
    case CrashReason::eIllegalOpcode:
        str = "eIllegalOpcode";
        break;
    case CrashReason::eIllegalOperand:
        str = "eIllegalOperand";
        break;
    case CrashReason::eIllegalAddressingMode:
        str = "eIllegalAddressingMode";
        break;
    case CrashReason::eIllegalTrap:
        str = "eIllegalTrap";
        break;
    case CrashReason::ePrivilegedOpcode:
        str = "ePrivilegedOpcode";
        break;
    case CrashReason::ePrivilegedRegister:
        str = "ePrivilegedRegister";
        break;
    case CrashReason::eCoprocessorError:
        str = "eCoprocessorError";
        break;
    case CrashReason::eInternalStackError:
        str = "eInternalStackError";
        break;

    // SIGBUS crash reasons:
    case CrashReason::eIllegalAlignment:
        str = "eIllegalAlignment";
        break;
    case CrashReason::eIllegalAddress:
        str = "eIllegalAddress";
        break;
    case CrashReason::eHardwareError:
        str = "eHardwareError";
        break;

    // SIGFPE crash reasons:
    case CrashReason::eIntegerDivideByZero:
        str = "eIntegerDivideByZero";
        break;
    case CrashReason::eIntegerOverflow:
        str = "eIntegerOverflow";
        break;
    case CrashReason::eFloatDivideByZero:
        str = "eFloatDivideByZero";
        break;
    case CrashReason::eFloatOverflow:
        str = "eFloatOverflow";
        break;
    case CrashReason::eFloatUnderflow:
        str = "eFloatUnderflow";
        break;
    case CrashReason::eFloatInexactResult:
        str = "eFloatInexactResult";
        break;
    case CrashReason::eFloatInvalidOperation:
        str = "eFloatInvalidOperation";
        break;
    case CrashReason::eFloatSubscriptRange:
        str = "eFloatSubscriptRange";
        break;
    }
#endif

    return str;
}
