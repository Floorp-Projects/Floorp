# Copyright (c) 2006, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


AC_PREREQ(2.64)

AC_INIT(breakpad, 0.1, google-breakpad-dev@googlegroups.com)
dnl Sanity check: the argument is just a file that should exist.
AC_CONFIG_SRCDIR(README.md)
AC_CONFIG_AUX_DIR(autotools)
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(subdir-objects tar-ustar 1.11.1)
AM_CONFIG_HEADER(src/config.h)
AM_MAINTAINER_MODE

AM_PROG_AR
AM_PROG_AS
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_RANLIB

dnl This must come before all the feature tests below.
AC_ARG_ENABLE(m32,
              AS_HELP_STRING([--enable-m32],
                             [Compile/build with -m32]
                             [(default is no)]),
              [case "${enableval}" in
                 yes)
                   CFLAGS="${CFLAGS} -m32"
                   CXXFLAGS="${CXXFLAGS} -m32"
                   usem32=true
                   ;;
                 no)
                   usem32=false
                   ;;
                 *)
                   AC_MSG_ERROR(bad value ${enableval} for --enable-m32)
                   ;;
               esac],
              [usem32=false])

AC_HEADER_STDC
AC_SYS_LARGEFILE
AX_PTHREAD
AC_CHECK_HEADERS([a.out.h sys/random.h])
AC_CHECK_FUNCS([arc4random getcontext getrandom])
AM_CONDITIONAL([HAVE_GETCONTEXT], [test "x$ac_cv_func_getcontext" = xyes])

AX_CXX_COMPILE_STDCXX(11, noext, mandatory)

dnl Test supported warning flags.
WARN_CXXFLAGS=
dnl This warning flag is used by clang.  Its default behavior is to warn when
dnl given an unknown flag rather than error out.
AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option],[
  ax_compiler_flags_test="-Werror=unknown-warning-option"
],[
  ax_compiler_flags_test=""
])
AX_APPEND_COMPILE_FLAGS(m4_flatten([
  -Wmissing-braces
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wreorder
  -Wsign-compare
  -Wunused-local-typedefs
  -Wunused-variable
  -Wvla
]), [WARN_CXXFLAGS], [${ax_compiler_flags_test}])
AS_VAR_APPEND([WARN_CXXFLAGS], " -Werror")
AC_LANG_POP([C++])
AC_SUBST([WARN_CXXFLAGS])

dnl Test support for O_CLOEXEC
AX_CHECK_DEFINE([fcntl.h], [O_CLOEXEC], [],
                [AC_DEFINE([O_CLOEXEC], [0], [Fallback definition for old systems])])

# Only build Linux client libs when compiling for Linux
case $host in
  *-*-linux* | *-android* )
    LINUX_HOST=true
    ;;
esac
AM_CONDITIONAL(LINUX_HOST, test x$LINUX_HOST = xtrue)

# Only use Android support headers when compiling for Android
case $host in
  *-android*)
    ANDROID_HOST=true
    ;;
esac
AM_CONDITIONAL(ANDROID_HOST, test x$ANDROID_HOST = xtrue)

# Some tools (like mac ones) only support x86 currently.
case $host_cpu in
  i?86|x86_64)
    X86_HOST=true
    ;;
esac
AM_CONDITIONAL(X86_HOST, test x$X86_HOST = xtrue)

AC_ARG_ENABLE(processor,
              AS_HELP_STRING([--disable-processor],
                             [Don't build processor library]
                             [(default is no)]),
              [case "${enableval}" in
                 yes)
                   disable_processor=false
                   ;;
                 no)
                   disable_processor=true
                   ;;
                 *)
                   AC_MSG_ERROR(bad value ${enableval} for --disable-processor)
                   ;;
               esac],
              [disable_processor=false])
AM_CONDITIONAL(DISABLE_PROCESSOR, test x$disable_processor = xtrue)

AC_ARG_ENABLE(tools,
              AS_HELP_STRING([--disable-tools],
                             [Don't build tool binaries]
                             [(default is no)]),
              [case "${enableval}" in
                 yes)
                   disable_tools=false
                   ;;
                 no)
                   disable_tools=true
                   ;;
                 *)
                   AC_MSG_ERROR(bad value ${enableval} for --disable-tools)
                   ;;
               esac],
              [disable_tools=false])
AM_CONDITIONAL(DISABLE_TOOLS, test x$disable_tools = xtrue)

if test x$LINUX_HOST = xfalse -a x$disable_processor = xtrue -a x$disable_tools = xtrue; then
   AC_MSG_ERROR([--disable-processor and --disable-tools were specified, and not building for Linux. Nothing to build!])
fi

AC_ARG_ENABLE(system-test-libs,
              AS_HELP_STRING([--enable-system-test-libs],
                             [Use gtest/gmock/etc... from the system instead ]
                             [of the local copies (default is local)]),
              [case "${enableval}" in
                 yes)
                   system_test_libs=true
                   ;;
                 no)
                   system_test_libs=false
                   ;;
                 *)
                   AC_MSG_ERROR(bad value ${enableval} for --enable-system-test-libs)
                   ;;
               esac],
              [system_test_libs=false])
AM_CONDITIONAL(SYSTEM_TEST_LIBS, test x$system_test_libs = xtrue)

AC_ARG_VAR([GMOCK_CFLAGS], [Compiler flags for gmock])
AC_ARG_VAR([GMOCK_LIBS], [Linker flags for gmock])
AC_ARG_VAR([GTEST_CFLAGS], [Compiler flags for gtest])
AC_ARG_VAR([GTEST_LIBS], [Linker flags for gtest])
if test x$system_test_libs = xtrue; then
  : "${GMOCK_CFLAGS:=-pthread}"
  : "${GMOCK_LIBS:=-lgmock -lgtest -pthread -lpthread}"
  : "${GTEST_CFLAGS:=-pthread}"
  : "${GTEST_LIBS:=-lgtest -pthread -lpthread}"
fi

AC_ARG_ENABLE(selftest,
              AS_HELP_STRING([--enable-selftest],
                             [Run extra tests with "make check" ]
                             [(may conflict with optimizations) ]
                             [(default is no)]),
              [case "${enableval}" in
                 yes)
                   selftest=true
                   ;;
                 no)
                   selftest=false
                   ;;
                 *)
                   AC_MSG_ERROR(bad value ${enableval} for --enable-selftest)
                   ;;
               esac],
              [selftest=false])
AM_CONDITIONAL(SELFTEST, test x$selftest = xtrue)

AC_ARG_WITH(rust-demangle,
            AS_HELP_STRING([--with-rust-demangle=/path/to/rust-demangle-capi],
                             [Link against the rust-demangle library]
                             [to demangle Rust language symbols during]
                             [symbol dumping (default is no)]
                             [Pass the path to the crate root.]),
            [case "${withval}" in
               yes)
                 AC_MSG_ERROR(You must pass the path to the rust-demangle-capi crate for --with-rust-demangle)
                 ;;
               no)
                 rust_demangle=false
                 ;;
               *)
                 if ! test -f "${withval}/Cargo.toml"; then
                   AC_MSG_ERROR(You must pass the path to the rust-demangle-capi crate for --with-rust-demangle)
                 fi
                 RUST_DEMANGLE_CFLAGS="-DHAVE_RUST_DEMANGLE -I${withval}/target/include"
                 RUST_DEMANGLE_LIBS="-L${withval}/target/release -lrust_demangle -lpthread -ldl"
                 ;;
             esac],
            [rust_demangle=false])
AC_ARG_VAR([RUST_DEMANGLE_CFLAGS], [Compiler flags for rust-demangle])
AC_ARG_VAR([RUST_DEMANGLE_LIBS], [Linker flags for rust-demangle])

AC_ARG_WITH(tests-as-root,
            AS_HELP_STRING([--with-tests-as-root],
                           [Run the tests as root. Use this on platforms]
                           [like travis-ci.org that require root privileges]
                           [to use ptrace (default is no)]),
            [case "${withval}" in
               yes)
                 tests_as_root=true
                 ;;
               no)
                 tests_as_root=false
                 ;;
               *)
                 AC_MSG_ERROR(--with-tests-as-root can only be "yes" or "no")
                 ;;
             esac],
            [tests_as_root=false])
AM_CONDITIONAL(TESTS_AS_ROOT, test x$tests_as_root = xtrue)

AC_CONFIG_FILES(m4_flatten([
  breakpad.pc
  breakpad-client.pc
  Makefile
]))

AC_OUTPUT
