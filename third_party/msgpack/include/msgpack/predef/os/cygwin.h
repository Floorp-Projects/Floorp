/*
Copyright Rene Rivera 2008-2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MSGPACK_PREDEF_OS_CYGWIN_H
#define MSGPACK_PREDEF_OS_CYGWIN_H

#include <msgpack/predef/version_number.h>
#include <msgpack/predef/make.h>

/*`
[heading `MSGPACK_OS_CYGWIN`]

[@http://en.wikipedia.org/wiki/Cygwin Cygwin] evironment.

[table
    [[__predef_symbol__] [__predef_version__]]

    [[`__CYGWIN__`] [__predef_detection__]]
    ]
 */

#define MSGPACK_OS_CYGWIN MSGPACK_VERSION_NUMBER_NOT_AVAILABLE

#if !defined(MSGPACK_PREDEF_DETAIL_OS_DETECTED) && ( \
    defined(__CYGWIN__) \
    )
#   undef MSGPACK_OS_CYGWIN
#   define MSGPACK_OS_CYGWIN MSGPACK_VERSION_NUMBER_AVAILABLE
#endif

#if MSGPACK_OS_CYGWIN
#   define MSGPACK_OS_CYGWIN_AVAILABLE
#   include <msgpack/predef/detail/os_detected.h>
#endif

#define MSGPACK_OS_CYGWIN_NAME "Cygwin"

#endif

#include <msgpack/predef/detail/test.h>
MSGPACK_PREDEF_DECLARE_TEST(MSGPACK_OS_CYGWIN,MSGPACK_OS_CYGWIN_NAME)
