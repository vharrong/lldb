//===-- AndroidThread.h -------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_AndroidThread_H_
#define liblldb_AndroidThread_H_

// Other libraries and framework includes
#include "POSIXThread.h"

//------------------------------------------------------------------------------
// @class AndroidThread
// @brief Abstraction of a Android thread.
class AndroidThread
    : public POSIXThread
{
public:

    //------------------------------------------------------------------
    // Constructors and destructors
    //------------------------------------------------------------------
    AndroidThread(lldb_private::Process &process, lldb::tid_t tid);

    virtual ~AndroidThread();

    //--------------------------------------------------------------------------
    // AndroidThread internal API.

    // POSIXThread override
    virtual void
    RefreshStateAfterStop();
};

#endif // #ifndef liblldb_AndroidThread_H_
