//===-- EditLineAndroid.cpp -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#if defined( __ANDROID_NDK__ )

#include "lldb/Host/android/editlineandroid.h"
#include <vector>
#include <assert.h>

// edit line EL_ADDFN function pointer type
typedef unsigned char(*el_addfn_func)(EditLine *e, int ch);
typedef const char* (*el_prompt_func)(EditLine *);

// edit line wrapper binding container
struct el_binding
{
    //
    const char   *name;
    const char   *help;
    // function pointer to callback routine
    el_addfn_func func;
    // ascii key this function is bound to
    const char   *key;
};

// edit line initalise
EditLine *
el_init (const char *, FILE *, FILE *, FILE *)
{
    //
    //SetConsoleTitleA( "lldb" );
	assert( !"Not implemented!" );
    // return dummy handle
    return (EditLine*) -1;
}

const char *
el_gets (EditLine *el, int *length)
{
    assert( !"Not implemented!" );
    return NULL;
}

int
el_set (EditLine *el, int code, ...)
{
    assert( !"Not implemented!" );
    return 0;
}

void
el_end (EditLine *el)
{
    assert( !"Not implemented!" );
}

void
el_reset (EditLine *)
{
    assert( !"Not implemented!" );
}

int
el_getc (EditLine *, char *)
{
    assert( !"Not implemented!" );
    return 0;
}

void
el_push (EditLine *, const char *)
{
}

void
el_beep (EditLine *)
{
    //Beep( 1000, 500 );
	assert( !"Not implemented!" );
}

int
el_parse (EditLine *, int, const char **)
{
    assert( !"Not implemented!" );
    return 0;
}

int
el_get (EditLine *el, int code, ...)
{
    assert( !"Not implemented!" );
    return 0;
}

int
el_source (EditLine *el, const char *file)
{
	assert( !"Not implemented!" );
    // init edit line by reading the contents of 'file'
    // nothing to do here on windows...
    return 0;
}

void
el_resize (EditLine *)
{
    assert( !"Not implemented!" );
}

const LineInfo *
el_line (EditLine *el)
{
	assert( !"Not implemented!" );
    return 0;
}

int
el_insertstr (EditLine *, const char *)
{
    assert( !"Not implemented!" );
    return 0;
}

void
el_deletestr (EditLine *, int)
{
    assert( !"Not implemented!" );
}

History *
history_init (void)
{
    // return dummy handle
    return (History*) -1;
}

void
history_end (History *)
{
//    assert( !"Not implemented!" );
}

int
history (History *, HistEvent *, int op, ...)
{
    // perform operation 'op' on the history list with
    // optional arguments as needed by the operation.
    return 0;
}

#endif
