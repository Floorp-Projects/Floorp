/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _XIDEFINES_H_
#define _XIDEFINES_H_

#include "XIErrors.h"

/*--------------------------------------------------------------------*
 *   Limits
 *--------------------------------------------------------------------*/
#define MAX_COMPONENTS 64
#define MAX_SETUP_TYPES 32


/*--------------------------------------------------------------------*
 *   Widget Dims
 *--------------------------------------------------------------------*/
#define XI_WIN_HEIGHT 320
#define XI_WIN_WIDTH  550


/*--------------------------------------------------------------------*
 *   Macros
 *--------------------------------------------------------------------*/
#define XI_IF_DELETE(_object)                           \
do {                                                    \
    if (_object)                                        \
        delete _object;                                 \
} while(0);

#define XI_IF_FREE(_ptr)                                \
do {                                                    \
    if (_ptr)                                           \
        free(_ptr);                                     \
} while(0);

#define XI_ERR_BAIL(_function)                          \
do {                                                    \
    err = _function;                                    \
    if (err != OK)                                      \
    {                                                   \
        ErrorHandler(err);                              \
        goto BAIL;                                      \
    }                                                   \
} while (0); 

#define XI_ERR_BAIL_EXCEPT(_function, _errexpected)     \
do {                                                    \
    err = _function;                                    \
    if (err != OK && err != _errexpected)               \
    {                                                   \
        ErrorHandler(err);                              \
        goto BAIL;                                      \
    }                                                   \
} while(0);
        
#define XI_VERIFY(_ptr)                                 \
do {                                                    \
    if (!_ptr)                                          \
        return ErrorHandler(E_INVALID_PTR);             \
} while (0);
     

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#endif /* _XIDEFINES_H_ */
