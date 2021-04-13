# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import six
from six import StringIO

from mozunit import main

from common import BaseConfigureTest
from mozbuild.configure.util import Version
from mozbuild.util import (
    memoize,
    ReadOnlyNamespace,
)
from mozpack import path as mozpath
from test_toolchain_helpers import (
    FakeCompiler,
    CompilerResult,
    PrependFlags,
)


DEFAULT_C99 = {
    "__STDC_VERSION__": "199901L",
}

DEFAULT_C11 = {
    "__STDC_VERSION__": "201112L",
}

DEFAULT_CXX_97 = {
    "__cplusplus": "199711L",
}

DEFAULT_CXX_11 = {
    "__cplusplus": "201103L",
}

DRAFT_CXX_14 = {
    "__cplusplus": "201300L",
}

DEFAULT_CXX_14 = {
    "__cplusplus": "201402L",
}

DRAFT_CXX17_201500 = {
    "__cplusplus": "201500L",
}

DRAFT_CXX17_201406 = {
    "__cplusplus": "201406L",
}

DEFAULT_CXX_17 = {
    "__cplusplus": "201703L",
}

SUPPORTS_GNU99 = {
    "-std=gnu99": DEFAULT_C99,
}

SUPPORTS_GNUXX11 = {
    "-std=gnu++11": DEFAULT_CXX_11,
}

SUPPORTS_GNUXX14 = {
    "-std=gnu++14": DEFAULT_CXX_14,
}

SUPPORTS_CXX14 = {
    "-std=c++14": DEFAULT_CXX_14,
}

SUPPORTS_GNUXX17 = {
    "-std=gnu++17": DEFAULT_CXX_17,
}

SUPPORTS_CXX17 = {
    "-std=c++17": DEFAULT_CXX_17,
}


@memoize
def GCC_BASE(version):
    version = Version(version)
    return FakeCompiler(
        {
            "__GNUC__": version.major,
            "__GNUC_MINOR__": version.minor,
            "__GNUC_PATCHLEVEL__": version.patch,
            "__STDC__": 1,
        }
    )


@memoize
def GCC(version):
    return GCC_BASE(version) + SUPPORTS_GNU99


@memoize
def GXX(version):
    return GCC_BASE(version) + DEFAULT_CXX_97 + SUPPORTS_GNUXX11


SUPPORTS_DRAFT_CXX14_VERSION = {
    "-std=gnu++14": DRAFT_CXX_14,
}

SUPPORTS_GNUXX1Z = {
    "-std=gnu++1z": DRAFT_CXX17_201406,
}

SUPPORTS_DRAFT_CXX17_201500_VERSION = {
    "-std=gnu++17": DRAFT_CXX17_201500,
}

GCC_4_9 = GCC("4.9.3")
GXX_4_9 = GXX("4.9.3") + SUPPORTS_DRAFT_CXX14_VERSION
GCC_5 = GCC("5.2.1") + DEFAULT_C11
GXX_5 = GXX("5.2.1") + SUPPORTS_GNUXX14
GCC_6 = GCC("6.4.0") + DEFAULT_C11
GXX_6 = (
    GXX("6.4.0")
    + DEFAULT_CXX_14
    + SUPPORTS_GNUXX17
    + SUPPORTS_DRAFT_CXX17_201500_VERSION
)
GCC_7 = GCC("7.3.0") + DEFAULT_C11
GXX_7 = GXX("7.3.0") + DEFAULT_CXX_14 + SUPPORTS_GNUXX17 + SUPPORTS_CXX17
GCC_8 = GCC("8.3.0") + DEFAULT_C11
GXX_8 = GXX("8.3.0") + DEFAULT_CXX_14 + SUPPORTS_GNUXX17 + SUPPORTS_CXX17

DEFAULT_GCC = GCC_7
DEFAULT_GXX = GXX_7

GCC_PLATFORM_LITTLE_ENDIAN = {
    "__ORDER_LITTLE_ENDIAN__": 1234,
    "__ORDER_BIG_ENDIAN__": 4321,
    "__BYTE_ORDER__": 1234,
}

GCC_PLATFORM_BIG_ENDIAN = {
    "__ORDER_LITTLE_ENDIAN__": 1234,
    "__ORDER_BIG_ENDIAN__": 4321,
    "__BYTE_ORDER__": 4321,
}

GCC_PLATFORM_X86 = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    None: {
        "__i386__": 1,
    },
    "-m64": {
        "__i386__": False,
        "__x86_64__": 1,
    },
}

GCC_PLATFORM_X86_64 = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    None: {
        "__x86_64__": 1,
    },
    "-m32": {
        "__x86_64__": False,
        "__i386__": 1,
    },
}

GCC_PLATFORM_ARM = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    "__arm__": 1,
}

GCC_PLATFORM_LINUX = {
    "__linux__": 1,
}

GCC_PLATFORM_DARWIN = {
    "__APPLE__": 1,
}

GCC_PLATFORM_WIN = {
    "_WIN32": 1,
    "WINNT": 1,
}

GCC_PLATFORM_OPENBSD = {
    "__OpenBSD__": 1,
}

GCC_PLATFORM_X86_LINUX = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_LINUX)
GCC_PLATFORM_X86_64_LINUX = FakeCompiler(GCC_PLATFORM_X86_64, GCC_PLATFORM_LINUX)
GCC_PLATFORM_ARM_LINUX = FakeCompiler(GCC_PLATFORM_ARM, GCC_PLATFORM_LINUX)
GCC_PLATFORM_X86_OSX = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_DARWIN)
GCC_PLATFORM_X86_64_OSX = FakeCompiler(GCC_PLATFORM_X86_64, GCC_PLATFORM_DARWIN)
GCC_PLATFORM_X86_WIN = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_WIN)
GCC_PLATFORM_X86_64_WIN = FakeCompiler(GCC_PLATFORM_X86_64, GCC_PLATFORM_WIN)


@memoize
def CLANG_BASE(version):
    version = Version(version)
    return FakeCompiler(
        {
            "__clang__": 1,
            "__clang_major__": version.major,
            "__clang_minor__": version.minor,
            "__clang_patchlevel__": version.patch,
        }
    )


@memoize
def CLANG(version):
    return GCC_BASE("4.2.1") + CLANG_BASE(version) + SUPPORTS_GNU99


@memoize
def CLANGXX(version):
    return (
        GCC_BASE("4.2.1")
        + CLANG_BASE(version)
        + DEFAULT_CXX_97
        + SUPPORTS_GNUXX11
        + SUPPORTS_GNUXX14
    )


CLANG_3_3 = CLANG("3.3.0") + DEFAULT_C99
CLANGXX_3_3 = CLANGXX("3.3.0")
CLANG_4_0 = CLANG("4.0.2") + DEFAULT_C11
CLANGXX_4_0 = CLANGXX("4.0.2") + SUPPORTS_GNUXX1Z
CLANG_5_0 = CLANG("5.0.1") + DEFAULT_C11
CLANGXX_5_0 = CLANGXX("5.0.1") + SUPPORTS_GNUXX17
XCODE_CLANG_3_3 = (
    CLANG("5.0")
    + DEFAULT_C99
    + {
        # Real Xcode clang has a full version here, but we don't care about it.
        "__apple_build_version__": "1",
    }
)
XCODE_CLANGXX_3_3 = CLANGXX("5.0") + {
    "__apple_build_version__": "1",
}
XCODE_CLANG_4_0 = (
    CLANG("9.0.0")
    + DEFAULT_C11
    + {
        "__apple_build_version__": "1",
    }
)
XCODE_CLANGXX_4_0 = (
    CLANGXX("9.0.0")
    + SUPPORTS_GNUXX1Z
    + {
        "__apple_build_version__": "1",
    }
)
XCODE_CLANG_5_0 = (
    CLANG("9.1.0")
    + DEFAULT_C11
    + {
        "__apple_build_version__": "1",
    }
)
XCODE_CLANGXX_5_0 = (
    CLANGXX("9.1.0")
    + SUPPORTS_GNUXX17
    + {
        "__apple_build_version__": "1",
    }
)
DEFAULT_CLANG = CLANG_5_0
DEFAULT_CLANGXX = CLANGXX_5_0


def CLANG_PLATFORM(gcc_platform):
    base = {
        "--target=x86_64-linux-gnu": GCC_PLATFORM_X86_64_LINUX[None],
        "--target=x86_64-apple-darwin11.2.0": GCC_PLATFORM_X86_64_OSX[None],
        "--target=i686-linux-gnu": GCC_PLATFORM_X86_LINUX[None],
        "--target=i686-apple-darwin11.2.0": GCC_PLATFORM_X86_OSX[None],
        "--target=arm-linux-gnu": GCC_PLATFORM_ARM_LINUX[None],
    }
    undo_gcc_platform = {
        k: {symbol: False for symbol in gcc_platform[None]} for k in base
    }
    return FakeCompiler(gcc_platform, undo_gcc_platform, base)


CLANG_PLATFORM_X86_LINUX = CLANG_PLATFORM(GCC_PLATFORM_X86_LINUX)
CLANG_PLATFORM_X86_64_LINUX = CLANG_PLATFORM(GCC_PLATFORM_X86_64_LINUX)
CLANG_PLATFORM_X86_OSX = CLANG_PLATFORM(GCC_PLATFORM_X86_OSX)
CLANG_PLATFORM_X86_64_OSX = CLANG_PLATFORM(GCC_PLATFORM_X86_64_OSX)
CLANG_PLATFORM_X86_WIN = CLANG_PLATFORM(GCC_PLATFORM_X86_WIN)
CLANG_PLATFORM_X86_64_WIN = CLANG_PLATFORM(GCC_PLATFORM_X86_64_WIN)


@memoize
def VS(version):
    version = Version(version)
    return FakeCompiler(
        {
            None: {
                "_MSC_VER": "%02d%02d" % (version.major, version.minor),
                "_MSC_FULL_VER": "%02d%02d%05d"
                % (version.major, version.minor, version.patch),
                "_MT": "1",
            },
            "*.cpp": DEFAULT_CXX_97,
        }
    )


VS_2017u8 = VS("19.15.26726")

VS_PLATFORM_X86 = {
    "_M_IX86": 600,
    "_WIN32": 1,
}

VS_PLATFORM_X86_64 = {
    "_M_X64": 100,
    "_WIN32": 1,
    "_WIN64": 1,
}

# Despite the 32 in the name, this macro is defined for 32- and 64-bit.
MINGW32 = {
    "__MINGW32__": True,
}

# Note: In reality, the -std=gnu* options are only supported when preceded by
# -Xclang.
CLANG_CL_3_9 = (
    CLANG_BASE("3.9.0")
    + VS("18.00.00000")
    + DEFAULT_C11
    + SUPPORTS_GNU99
    + SUPPORTS_GNUXX11
    + SUPPORTS_CXX14
) + {
    "*.cpp": {
        "__STDC_VERSION__": False,
        "__cplusplus": "201103L",
    },
}
CLANG_CL_8_0 = (
    CLANG_BASE("8.0.0")
    + VS("18.00.00000")
    + DEFAULT_C11
    + SUPPORTS_GNU99
    + SUPPORTS_GNUXX11
    + SUPPORTS_CXX14
    + SUPPORTS_CXX17
) + {
    "*.cpp": {
        "__STDC_VERSION__": False,
        "__cplusplus": "201103L",
    },
}

CLANG_CL_PLATFORM_X86 = FakeCompiler(
    VS_PLATFORM_X86, GCC_PLATFORM_X86[None], GCC_PLATFORM_LITTLE_ENDIAN
)
CLANG_CL_PLATFORM_X86_64 = FakeCompiler(
    VS_PLATFORM_X86_64, GCC_PLATFORM_X86_64[None], GCC_PLATFORM_LITTLE_ENDIAN
)

LIBRARY_NAME_INFOS = {
    "linux-gnu": {
        "DLL_PREFIX": "lib",
        "DLL_SUFFIX": ".so",
        "LIB_PREFIX": "lib",
        "LIB_SUFFIX": "a",
        "IMPORT_LIB_SUFFIX": "",
        "OBJ_SUFFIX": "o",
    },
    "darwin11.2.0": {
        "DLL_PREFIX": "lib",
        "DLL_SUFFIX": ".dylib",
        "LIB_PREFIX": "lib",
        "LIB_SUFFIX": "a",
        "IMPORT_LIB_SUFFIX": "",
        "OBJ_SUFFIX": "o",
    },
    "mingw32": {
        "DLL_PREFIX": "",
        "DLL_SUFFIX": ".dll",
        "LIB_PREFIX": "lib",
        "LIB_SUFFIX": "a",
        "IMPORT_LIB_SUFFIX": "a",
        "OBJ_SUFFIX": "o",
    },
    "msvc": {
        "DLL_PREFIX": "",
        "DLL_SUFFIX": ".dll",
        "LIB_PREFIX": "",
        "LIB_SUFFIX": "lib",
        "IMPORT_LIB_SUFFIX": "lib",
        "OBJ_SUFFIX": "obj",
    },
    "openbsd6.1": {
        "DLL_PREFIX": "lib",
        "DLL_SUFFIX": ".so.1.0",
        "LIB_PREFIX": "lib",
        "LIB_SUFFIX": "a",
        "IMPORT_LIB_SUFFIX": "",
        "OBJ_SUFFIX": "o",
    },
}


class BaseToolchainTest(BaseConfigureTest):
    def setUp(self):
        super(BaseToolchainTest, self).setUp()
        self.out = StringIO()
        self.logger = logging.getLogger("BaseToolchainTest")
        self.logger.setLevel(logging.ERROR)
        self.handler = logging.StreamHandler(self.out)
        self.logger.addHandler(self.handler)

    def tearDown(self):
        self.logger.removeHandler(self.handler)
        del self.handler
        del self.out
        super(BaseToolchainTest, self).tearDown()

    def do_toolchain_test(self, paths, results, args=[], environ={}):
        """Helper to test the toolchain checks from toolchain.configure.

        - `paths` is a dict associating compiler paths to FakeCompiler
          definitions from above.
        - `results` is a dict associating result variable names from
          toolchain.configure (c_compiler, cxx_compiler, host_c_compiler,
          host_cxx_compiler) with a result.
          The result can either be an error string, or a CompilerResult
          corresponding to the object returned by toolchain.configure checks.
          When the results for host_c_compiler are identical to c_compiler,
          they can be omitted. Likewise for host_cxx_compiler vs.
          cxx_compiler.
        """
        environ = dict(environ)
        if "PATH" not in environ:
            environ["PATH"] = os.pathsep.join(
                mozpath.abspath(p) for p in ("/bin", "/usr/bin")
            )

        args = args + ["--enable-release"]

        sandbox = self.get_sandbox(paths, {}, args, environ, logger=self.logger)

        for var in (
            "c_compiler",
            "cxx_compiler",
            "host_c_compiler",
            "host_cxx_compiler",
        ):
            if var in results:
                result = results[var]
            elif var.startswith("host_"):
                result = results.get(var[5:], {})
            else:
                result = {}
            try:
                self.out.truncate(0)
                self.out.seek(0)
                compiler = sandbox._value_for(sandbox[var])
                # Add var on both ends to make it clear which of the
                # variables is failing the test when that happens.
                self.assertEquals((var, compiler), (var, result))
            except SystemExit:
                self.assertEquals((var, result), (var, self.out.getvalue().strip()))
                return

        # Normalize the target os to match what we have as keys in
        # LIBRARY_NAME_INFOS.
        target_os = getattr(self, "TARGET", self.HOST).split("-", 2)[2]
        if target_os == "mingw32":
            compiler_type = sandbox._value_for(sandbox["c_compiler"]).type
            if compiler_type == "clang-cl":
                target_os = "msvc"
        elif target_os == "linux-gnuabi64":
            target_os = "linux-gnu"

        self.do_library_name_info_test(target_os, sandbox)

        # Try again on artifact builds. In that case, we always get library
        # name info for msvc on Windows
        if target_os == "mingw32":
            target_os = "msvc"

        sandbox = self.get_sandbox(
            paths, {}, args + ["--enable-artifact-builds"], environ, logger=self.logger
        )

        self.do_library_name_info_test(target_os, sandbox)

    def do_library_name_info_test(self, target_os, sandbox):
        library_name_info = LIBRARY_NAME_INFOS[target_os]
        for k in (
            "DLL_PREFIX",
            "DLL_SUFFIX",
            "LIB_PREFIX",
            "LIB_SUFFIX",
            "IMPORT_LIB_SUFFIX",
            "OBJ_SUFFIX",
        ):
            self.assertEquals(
                "%s=%s" % (k, sandbox.get_config(k)),
                "%s=%s" % (k, library_name_info[k]),
            )


def old_gcc_message(old_ver):
    return "Only GCC 7.1 or newer is supported (found version {}).".format(old_ver)


class LinuxToolchainTest(BaseToolchainTest):
    PATHS = {
        "/usr/bin/gcc": DEFAULT_GCC + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++": DEFAULT_GXX + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/gcc-4.9": GCC_4_9 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++-4.9": GXX_4_9 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/gcc-5": GCC_5 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++-5": GXX_5 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/gcc-6": GCC_6 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++-6": GXX_6 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/gcc-7": GCC_7 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++-7": GXX_7 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/gcc-8": GCC_8 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/g++-8": GXX_8 + GCC_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang": DEFAULT_CLANG + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang++": DEFAULT_CLANGXX + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang-5.0": CLANG_5_0 + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang++-5.0": CLANGXX_5_0 + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang-4.0": CLANG_4_0 + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang++-4.0": CLANGXX_4_0 + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang-3.3": CLANG_3_3 + CLANG_PLATFORM_X86_64_LINUX,
        "/usr/bin/clang++-3.3": CLANGXX_3_3 + CLANG_PLATFORM_X86_64_LINUX,
    }

    GCC_4_7_RESULT = old_gcc_message("4.7.3")
    GXX_4_7_RESULT = GCC_4_7_RESULT
    GCC_4_9_RESULT = old_gcc_message("4.9.3")
    GXX_4_9_RESULT = GCC_4_9_RESULT
    GCC_5_RESULT = old_gcc_message("5.2.1")
    GXX_5_RESULT = GCC_5_RESULT
    GCC_6_RESULT = old_gcc_message("6.4.0")
    GXX_6_RESULT = GCC_6_RESULT
    GCC_7_RESULT = CompilerResult(
        flags=["-std=gnu99"],
        version="7.3.0",
        type="gcc",
        compiler="/usr/bin/gcc-7",
        language="C",
    )
    GXX_7_RESULT = CompilerResult(
        flags=["-std=gnu++17"],
        version="7.3.0",
        type="gcc",
        compiler="/usr/bin/g++-7",
        language="C++",
    )
    GCC_8_RESULT = CompilerResult(
        flags=["-std=gnu99"],
        version="8.3.0",
        type="gcc",
        compiler="/usr/bin/gcc-8",
        language="C",
    )
    GXX_8_RESULT = CompilerResult(
        flags=["-std=gnu++17"],
        version="8.3.0",
        type="gcc",
        compiler="/usr/bin/g++-8",
        language="C++",
    )
    DEFAULT_GCC_RESULT = GCC_7_RESULT + {"compiler": "/usr/bin/gcc"}
    DEFAULT_GXX_RESULT = GXX_7_RESULT + {"compiler": "/usr/bin/g++"}

    CLANG_3_3_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 3.3.0)."
    )
    CLANGXX_3_3_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 3.3.0)."
    )
    CLANG_4_0_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.2)."
    )
    CLANGXX_4_0_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.2)."
    )
    CLANG_5_0_RESULT = CompilerResult(
        flags=["-std=gnu99"],
        version="5.0.1",
        type="clang",
        compiler="/usr/bin/clang-5.0",
        language="C",
    )
    CLANGXX_5_0_RESULT = CompilerResult(
        flags=["-std=gnu++17"],
        version="5.0.1",
        type="clang",
        compiler="/usr/bin/clang++-5.0",
        language="C++",
    )
    DEFAULT_CLANG_RESULT = CLANG_5_0_RESULT + {"compiler": "/usr/bin/clang"}
    DEFAULT_CLANGXX_RESULT = CLANGXX_5_0_RESULT + {"compiler": "/usr/bin/clang++"}

    def test_default(self):
        # We'll try clang and gcc, and find clang first.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )

    def test_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT,
                "cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "gcc",
                "CXX": "g++",
            },
        )

    def test_unsupported_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.GCC_4_9_RESULT,
            },
            environ={
                "CC": "gcc-4.9",
                "CXX": "g++-4.9",
            },
        )

        # Maybe this should be reporting the mismatched version instead.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT,
                "cxx_compiler": self.GXX_4_9_RESULT,
            },
            environ={
                "CC": "gcc",
                "CXX": "g++-4.9",
            },
        )

    def test_overridden_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.GCC_7_RESULT,
                "cxx_compiler": self.GXX_7_RESULT,
            },
            environ={
                "CC": "gcc-7",
                "CXX": "g++-7",
            },
        )

    def test_guess_cxx(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.GCC_7_RESULT,
                "cxx_compiler": self.GXX_7_RESULT,
            },
            environ={
                "CC": "gcc-7",
            },
        )

    def test_mismatched_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT,
                "cxx_compiler": (
                    "The target C compiler is version 7.3.0, while the target "
                    "C++ compiler is version 8.3.0. Need to use the same compiler "
                    "version."
                ),
            },
            environ={
                "CC": "gcc",
                "CXX": "g++-8",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT,
                "cxx_compiler": self.DEFAULT_GXX_RESULT,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": (
                    "The host C compiler is version 7.3.0, while the host "
                    "C++ compiler is version 8.3.0. Need to use the same compiler "
                    "version."
                ),
            },
            environ={
                "CC": "gcc",
                "HOST_CXX": "g++-8",
            },
        )

    def test_mismatched_compiler(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": (
                    "The target C compiler is clang, while the target C++ compiler "
                    "is gcc. Need to use the same compiler suite."
                ),
            },
            environ={
                "CXX": "g++",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": (
                    "The host C compiler is clang, while the host C++ compiler "
                    "is gcc. Need to use the same compiler suite."
                ),
            },
            environ={
                "HOST_CXX": "g++",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": "`%s` is not a C compiler."
                % mozpath.abspath("/usr/bin/g++"),
            },
            environ={
                "CC": "g++",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": "`%s` is not a C++ compiler."
                % mozpath.abspath("/usr/bin/clang"),
            },
            environ={
                "CXX": "clang",
            },
        )

    def test_clang(self):
        # We'll try gcc and clang, but since there is no gcc (gcc-x.y doesn't
        # count), find clang.
        paths = {
            k: v
            for k, v in six.iteritems(self.PATHS)
            if os.path.basename(k) not in ("gcc", "g++")
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )

    def test_guess_cxx_clang(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_5_0_RESULT,
                "cxx_compiler": self.CLANGXX_5_0_RESULT,
            },
            environ={
                "CC": "clang-5.0",
            },
        )

    def test_unsupported_clang(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_3_3_RESULT,
                "cxx_compiler": self.CLANGXX_3_3_RESULT,
            },
            environ={
                "CC": "clang-3.3",
                "CXX": "clang++-3.3",
            },
        )
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_4_0_RESULT,
                "cxx_compiler": self.CLANGXX_4_0_RESULT,
            },
            environ={
                "CC": "clang-4.0",
                "CXX": "clang++-4.0",
            },
        )

    def test_no_supported_compiler(self):
        # Even if there are gcc-x.y or clang-x.y compilers available, we
        # don't try them. This could be considered something to improve.
        paths = {
            k: v
            for k, v in six.iteritems(self.PATHS)
            if os.path.basename(k) not in ("gcc", "g++", "clang", "clang++")
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": "Cannot find the target C compiler",
            },
        )

    def test_absolute_path(self):
        paths = dict(self.PATHS)
        paths.update(
            {
                "/opt/clang/bin/clang": paths["/usr/bin/clang"],
                "/opt/clang/bin/clang++": paths["/usr/bin/clang++"],
            }
        )
        result = {
            "c_compiler": self.DEFAULT_CLANG_RESULT
            + {
                "compiler": "/opt/clang/bin/clang",
            },
            "cxx_compiler": self.DEFAULT_CLANGXX_RESULT
            + {
                "compiler": "/opt/clang/bin/clang++",
            },
        }
        self.do_toolchain_test(
            paths,
            result,
            environ={
                "CC": "/opt/clang/bin/clang",
                "CXX": "/opt/clang/bin/clang++",
            },
        )
        # With CXX guess too.
        self.do_toolchain_test(
            paths,
            result,
            environ={
                "CC": "/opt/clang/bin/clang",
            },
        )

    def test_atypical_name(self):
        paths = dict(self.PATHS)
        paths.update(
            {
                "/usr/bin/afl-clang-fast": paths["/usr/bin/clang"],
                "/usr/bin/afl-clang-fast++": paths["/usr/bin/clang++"],
            }
        )
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT
                + {
                    "compiler": "/usr/bin/afl-clang-fast",
                },
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT
                + {
                    "compiler": "/usr/bin/afl-clang-fast++",
                },
            },
            environ={
                "CC": "afl-clang-fast",
                "CXX": "afl-clang-fast++",
            },
        )

    def test_mixed_compilers(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "clang",
                "HOST_CC": "gcc",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "clang",
                "CXX": "clang++",
                "HOST_CC": "gcc",
            },
        )


class LinuxSimpleCrossToolchainTest(BaseToolchainTest):
    TARGET = "i686-pc-linux-gnu"
    PATHS = LinuxToolchainTest.PATHS
    DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT
    DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT
    DEFAULT_CLANG_RESULT = LinuxToolchainTest.DEFAULT_CLANG_RESULT
    DEFAULT_CLANGXX_RESULT = LinuxToolchainTest.DEFAULT_CLANGXX_RESULT

    def test_cross_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT + {"flags": ["-m32"]},
                "cxx_compiler": self.DEFAULT_GXX_RESULT + {"flags": ["-m32"]},
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={"CC": "gcc"},
        )

    def test_cross_clang(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT + {"flags": ["-m32"]},
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT + {"flags": ["-m32"]},
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )


class LinuxX86_64CrossToolchainTest(BaseToolchainTest):
    HOST = "i686-pc-linux-gnu"
    TARGET = "x86_64-pc-linux-gnu"
    PATHS = {
        "/usr/bin/gcc": DEFAULT_GCC + GCC_PLATFORM_X86_LINUX,
        "/usr/bin/g++": DEFAULT_GXX + GCC_PLATFORM_X86_LINUX,
        "/usr/bin/clang": DEFAULT_CLANG + CLANG_PLATFORM_X86_LINUX,
        "/usr/bin/clang++": DEFAULT_CLANGXX + CLANG_PLATFORM_X86_LINUX,
    }
    DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT
    DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT
    DEFAULT_CLANG_RESULT = LinuxToolchainTest.DEFAULT_CLANG_RESULT
    DEFAULT_CLANGXX_RESULT = LinuxToolchainTest.DEFAULT_CLANGXX_RESULT

    def test_cross_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT + {"flags": ["-m64"]},
                "cxx_compiler": self.DEFAULT_GXX_RESULT + {"flags": ["-m64"]},
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "gcc",
            },
        )

    def test_cross_clang(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT + {"flags": ["-m64"]},
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT + {"flags": ["-m64"]},
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )


def xcrun(stdin, args):
    if args == ("--show-sdk-path",):
        return (
            0,
            os.path.join(os.path.abspath(os.path.dirname(__file__)), "fake_macos_sdk"),
            "",
        )
    raise NotImplementedError()


class OSXToolchainTest(BaseToolchainTest):
    HOST = "x86_64-apple-darwin11.2.0"
    PATHS = {
        "/usr/bin/gcc-5": GCC_5 + GCC_PLATFORM_X86_64_OSX,
        "/usr/bin/g++-5": GXX_5 + GCC_PLATFORM_X86_64_OSX,
        "/usr/bin/gcc-7": GCC_7 + GCC_PLATFORM_X86_64_OSX,
        "/usr/bin/g++-7": GXX_7 + GCC_PLATFORM_X86_64_OSX,
        "/usr/bin/clang": XCODE_CLANG_5_0 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/clang++": XCODE_CLANGXX_5_0 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/clang-4.0": XCODE_CLANG_4_0 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/clang++-4.0": XCODE_CLANGXX_4_0 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/clang-3.3": XCODE_CLANG_3_3 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/clang++-3.3": XCODE_CLANGXX_3_3 + CLANG_PLATFORM_X86_64_OSX,
        "/usr/bin/xcrun": xcrun,
    }
    CLANG_3_3_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.0.or.less)."
    )
    CLANGXX_3_3_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.0.or.less)."
    )
    CLANG_4_0_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.0.or.less)."
    )
    CLANGXX_4_0_RESULT = (
        "Only clang/llvm 5.0 or newer is supported (found version 4.0.0.or.less)."
    )
    DEFAULT_CLANG_RESULT = CompilerResult(
        flags=["-std=gnu99"],
        version="5.0.2",
        type="clang",
        compiler="/usr/bin/clang",
        language="C",
    )
    DEFAULT_CLANGXX_RESULT = CompilerResult(
        flags=["-std=gnu++17"],
        version="5.0.2",
        type="clang",
        compiler="/usr/bin/clang++",
        language="C++",
    )
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT
    GCC_7_RESULT = LinuxToolchainTest.GCC_7_RESULT
    GXX_7_RESULT = LinuxToolchainTest.GXX_7_RESULT
    SYSROOT_FLAGS = {
        "flags": PrependFlags(["-isysroot", xcrun("", ("--show-sdk-path",))[1]]),
    }

    def test_clang(self):
        # We only try clang because gcc is known not to work.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT + self.SYSROOT_FLAGS,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT + self.SYSROOT_FLAGS,
            },
        )

    def test_not_gcc(self):
        # We won't pick GCC if it's the only thing available.
        paths = {
            k: v
            for k, v in six.iteritems(self.PATHS)
            if os.path.basename(k) not in ("clang", "clang++")
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": "Cannot find the target C compiler",
            },
        )

    def test_unsupported_clang(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_3_3_RESULT,
                "cxx_compiler": self.CLANGXX_3_3_RESULT,
            },
            environ={
                "CC": "clang-3.3",
                "CXX": "clang++-3.3",
            },
        )
        # When targeting mac, we require at least version 5.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_4_0_RESULT,
                "cxx_compiler": self.CLANGXX_4_0_RESULT,
            },
            environ={
                "CC": "clang-4.0",
                "CXX": "clang++-4.0",
            },
        )

    def test_forced_gcc(self):
        # GCC can still be forced if the user really wants it.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.GCC_7_RESULT + self.SYSROOT_FLAGS,
                "cxx_compiler": self.GXX_7_RESULT + self.SYSROOT_FLAGS,
            },
            environ={
                "CC": "gcc-7",
                "CXX": "g++-7",
            },
        )

    def test_forced_unsupported_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.GCC_5_RESULT,
            },
            environ={
                "CC": "gcc-5",
                "CXX": "g++-5",
            },
        )


class WindowsToolchainTest(BaseToolchainTest):
    HOST = "i686-pc-mingw32"

    # For the purpose of this test, it doesn't matter that the paths are not
    # real Windows paths.
    PATHS = {
        "/usr/bin/cl": VS_2017u8 + VS_PLATFORM_X86,
        "/usr/bin/clang-cl-3.9": CLANG_CL_3_9 + CLANG_CL_PLATFORM_X86,
        "/usr/bin/clang-cl": CLANG_CL_8_0 + CLANG_CL_PLATFORM_X86,
        "/usr/bin/gcc": DEFAULT_GCC + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/g++": DEFAULT_GXX + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/gcc-4.9": GCC_4_9 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/g++-4.9": GXX_4_9 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/gcc-5": GCC_5 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/g++-5": GXX_5 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/gcc-6": GCC_6 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/g++-6": GXX_6 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/gcc-7": GCC_7 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/g++-7": GXX_7 + GCC_PLATFORM_X86_WIN + MINGW32,
        "/usr/bin/clang": DEFAULT_CLANG + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang++": DEFAULT_CLANGXX + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang-5.0": CLANG_5_0 + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang++-5.0": CLANGXX_5_0 + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang-4.0": CLANG_4_0 + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang++-4.0": CLANGXX_4_0 + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang-3.3": CLANG_3_3 + CLANG_PLATFORM_X86_WIN,
        "/usr/bin/clang++-3.3": CLANGXX_3_3 + CLANG_PLATFORM_X86_WIN,
    }

    CLANG_CL_3_9_RESULT = (
        "Only clang-cl 8.0 or newer is supported (found version 3.9.0)"
    )
    CLANG_CL_8_0_RESULT = CompilerResult(
        version="8.0.0",
        flags=["-Xclang", "-std=gnu99"],
        type="clang-cl",
        compiler="/usr/bin/clang-cl",
        language="C",
    )
    CLANGXX_CL_3_9_RESULT = (
        "Only clang-cl 8.0 or newer is supported (found version 3.9.0)"
    )
    CLANGXX_CL_8_0_RESULT = CompilerResult(
        version="8.0.0",
        flags=["-Xclang", "-std=c++17"],
        type="clang-cl",
        compiler="/usr/bin/clang-cl",
        language="C++",
    )
    CLANG_3_3_RESULT = LinuxToolchainTest.CLANG_3_3_RESULT
    CLANGXX_3_3_RESULT = LinuxToolchainTest.CLANGXX_3_3_RESULT
    CLANG_4_0_RESULT = LinuxToolchainTest.CLANG_4_0_RESULT
    CLANGXX_4_0_RESULT = LinuxToolchainTest.CLANGXX_4_0_RESULT
    DEFAULT_CLANG_RESULT = LinuxToolchainTest.DEFAULT_CLANG_RESULT
    DEFAULT_CLANGXX_RESULT = LinuxToolchainTest.DEFAULT_CLANGXX_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT
    GCC_6_RESULT = LinuxToolchainTest.GCC_6_RESULT
    GXX_6_RESULT = LinuxToolchainTest.GXX_6_RESULT
    GCC_7_RESULT = LinuxToolchainTest.GCC_7_RESULT
    GXX_7_RESULT = LinuxToolchainTest.GXX_7_RESULT
    DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT
    DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT

    def test_unsupported_msvc(self):
        self.do_toolchain_test(
            self.PATHS,
            {"c_compiler": "Unknown compiler or compiler not supported."},
            environ={
                "CC": "/usr/bin/cl",
            },
        )

    def test_unsupported_clang_cl(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_CL_3_9_RESULT,
            },
            environ={
                "CC": "/usr/bin/clang-cl-3.9",
            },
        )

    def test_clang_cl(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_CL_8_0_RESULT,
                "cxx_compiler": self.CLANGXX_CL_8_0_RESULT,
            },
        )

    def test_gcc(self):
        # GCC is unsupported, if you try it should find clang.
        paths = {
            k: v
            for k, v in six.iteritems(self.PATHS)
            if os.path.basename(k) not in ("cl", "clang-cl")
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )

    # This test is not perfect, as the GCC version needs to be updated when we
    # bump the minimum GCC version, but the idea is that even supported GCC
    # on other platforms should not be supported on Windows.
    def test_overridden_supported_elsewhere_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": "Unknown compiler or compiler not supported.",
            },
            environ={
                "CC": "gcc-7",
                "CXX": "g++-7",
            },
        )

    def test_overridden_unsupported_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": "Unknown compiler or compiler not supported.",
            },
            environ={
                "CC": "gcc-5",
                "CXX": "g++-5",
            },
        )

    def test_clang(self):
        # We'll pick clang if nothing else is found.
        paths = {
            k: v
            for k, v in six.iteritems(self.PATHS)
            if os.path.basename(k) not in ("cl", "clang-cl", "gcc")
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT,
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )

    def test_overridden_unsupported_clang(self):
        # clang 3.3 C compiler is perfectly fine, but we need more for C++.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.CLANG_3_3_RESULT,
                "cxx_compiler": self.CLANGXX_3_3_RESULT,
            },
            environ={
                "CC": "clang-3.3",
                "CXX": "clang++-3.3",
            },
        )


class Windows64ToolchainTest(WindowsToolchainTest):
    HOST = "x86_64-pc-mingw32"

    # For the purpose of this test, it doesn't matter that the paths are not
    # real Windows paths.
    PATHS = {
        "/usr/bin/cl": VS_2017u8 + VS_PLATFORM_X86_64,
        "/usr/bin/clang-cl": CLANG_CL_8_0 + CLANG_CL_PLATFORM_X86_64,
        "/usr/bin/clang-cl-3.9": CLANG_CL_3_9 + CLANG_CL_PLATFORM_X86_64,
        "/usr/bin/gcc": DEFAULT_GCC + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/g++": DEFAULT_GXX + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/gcc-4.9": GCC_4_9 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/g++-4.9": GXX_4_9 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/gcc-5": GCC_5 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/g++-5": GXX_5 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/gcc-6": GCC_6 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/g++-6": GXX_6 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/gcc-7": GCC_7 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/g++-7": GXX_7 + GCC_PLATFORM_X86_64_WIN + MINGW32,
        "/usr/bin/clang": DEFAULT_CLANG + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang++": DEFAULT_CLANGXX + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang-5.0": CLANG_5_0 + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang++-5.0": CLANGXX_5_0 + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang-4.0": CLANG_4_0 + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang++-4.0": CLANGXX_4_0 + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang-3.3": CLANG_3_3 + CLANG_PLATFORM_X86_64_WIN,
        "/usr/bin/clang++-3.3": CLANGXX_3_3 + CLANG_PLATFORM_X86_64_WIN,
    }


class LinuxCrossCompileToolchainTest(BaseToolchainTest):
    TARGET = "arm-unknown-linux-gnu"
    PATHS = {
        "/usr/bin/arm-linux-gnu-gcc-4.9": GCC_4_9 + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-g++-4.9": GXX_4_9 + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-gcc-5": GCC_5 + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-g++-5": GXX_5 + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-gcc": DEFAULT_GCC + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-g++": DEFAULT_GXX + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-gcc-7": GCC_7 + GCC_PLATFORM_ARM_LINUX,
        "/usr/bin/arm-linux-gnu-g++-7": GXX_7 + GCC_PLATFORM_ARM_LINUX,
    }
    PATHS.update(LinuxToolchainTest.PATHS)
    ARM_GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    ARM_GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    ARM_GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    ARM_GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT
    ARM_DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT + {
        "compiler": "/usr/bin/arm-linux-gnu-gcc",
    }
    ARM_DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT + {
        "compiler": "/usr/bin/arm-linux-gnu-g++",
    }
    ARM_GCC_7_RESULT = LinuxToolchainTest.GCC_7_RESULT + {
        "compiler": "/usr/bin/arm-linux-gnu-gcc-7",
    }
    ARM_GXX_7_RESULT = LinuxToolchainTest.GXX_7_RESULT + {
        "compiler": "/usr/bin/arm-linux-gnu-g++-7",
    }
    DEFAULT_CLANG_RESULT = LinuxToolchainTest.DEFAULT_CLANG_RESULT
    DEFAULT_CLANGXX_RESULT = LinuxToolchainTest.DEFAULT_CLANGXX_RESULT
    DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT
    DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT

    little_endian = FakeCompiler(GCC_PLATFORM_LINUX, GCC_PLATFORM_LITTLE_ENDIAN)
    big_endian = FakeCompiler(GCC_PLATFORM_LINUX, GCC_PLATFORM_BIG_ENDIAN)

    PLATFORMS = {
        "i686-pc-linux-gnu": GCC_PLATFORM_X86_LINUX,
        "x86_64-pc-linux-gnu": GCC_PLATFORM_X86_64_LINUX,
        "arm-unknown-linux-gnu": GCC_PLATFORM_ARM_LINUX,
        "aarch64-unknown-linux-gnu": little_endian
        + {
            "__aarch64__": 1,
        },
        "ia64-unknown-linux-gnu": little_endian
        + {
            "__ia64__": 1,
        },
        "s390x-unknown-linux-gnu": big_endian
        + {
            "__s390x__": 1,
            "__s390__": 1,
        },
        "s390-unknown-linux-gnu": big_endian
        + {
            "__s390__": 1,
        },
        "powerpc64-unknown-linux-gnu": big_endian
        + {
            None: {
                "__powerpc64__": 1,
                "__powerpc__": 1,
            },
            "-m32": {
                "__powerpc64__": False,
            },
        },
        "powerpc-unknown-linux-gnu": big_endian
        + {
            None: {
                "__powerpc__": 1,
            },
            "-m64": {
                "__powerpc64__": 1,
            },
        },
        "alpha-unknown-linux-gnu": little_endian
        + {
            "__alpha__": 1,
        },
        "hppa-unknown-linux-gnu": big_endian
        + {
            "__hppa__": 1,
        },
        "sparc64-unknown-linux-gnu": big_endian
        + {
            None: {
                "__arch64__": 1,
                "__sparc__": 1,
            },
            "-m32": {
                "__arch64__": False,
            },
        },
        "sparc-unknown-linux-gnu": big_endian
        + {
            None: {
                "__sparc__": 1,
            },
            "-m64": {
                "__arch64__": 1,
            },
        },
        "m68k-unknown-linux-gnu": big_endian
        + {
            "__m68k__": 1,
        },
        "mips64-unknown-linux-gnuabi64": big_endian
        + {
            "__mips64": 1,
            "__mips__": 1,
        },
        "mips-unknown-linux-gnu": big_endian
        + {
            "__mips__": 1,
        },
        "riscv64-unknown-linux-gnu": little_endian
        + {
            "__riscv": 1,
            "__riscv_xlen": 64,
        },
        "sh4-unknown-linux-gnu": little_endian
        + {
            "__sh__": 1,
        },
    }

    PLATFORMS["powerpc64le-unknown-linux-gnu"] = (
        PLATFORMS["powerpc64-unknown-linux-gnu"] + GCC_PLATFORM_LITTLE_ENDIAN
    )
    PLATFORMS["mips64el-unknown-linux-gnuabi64"] = (
        PLATFORMS["mips64-unknown-linux-gnuabi64"] + GCC_PLATFORM_LITTLE_ENDIAN
    )
    PLATFORMS["mipsel-unknown-linux-gnu"] = (
        PLATFORMS["mips-unknown-linux-gnu"] + GCC_PLATFORM_LITTLE_ENDIAN
    )

    def do_test_cross_gcc_32_64(self, host, target):
        self.HOST = host
        self.TARGET = target
        paths = {
            "/usr/bin/gcc": DEFAULT_GCC + self.PLATFORMS[host],
            "/usr/bin/g++": DEFAULT_GXX + self.PLATFORMS[host],
        }
        cross_flags = {"flags": ["-m64" if "64" in target else "-m32"]}
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT + cross_flags,
                "cxx_compiler": self.DEFAULT_GXX_RESULT + cross_flags,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
        )
        self.HOST = LinuxCrossCompileToolchainTest.HOST
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_cross_x86_x64(self):
        self.do_test_cross_gcc_32_64("i686-pc-linux-gnu", "x86_64-pc-linux-gnu")
        self.do_test_cross_gcc_32_64("x86_64-pc-linux-gnu", "i686-pc-linux-gnu")

    def test_cross_sparc_sparc64(self):
        self.do_test_cross_gcc_32_64(
            "sparc-unknown-linux-gnu", "sparc64-unknown-linux-gnu"
        )
        self.do_test_cross_gcc_32_64(
            "sparc64-unknown-linux-gnu", "sparc-unknown-linux-gnu"
        )

    def test_cross_ppc_ppc64(self):
        self.do_test_cross_gcc_32_64(
            "powerpc-unknown-linux-gnu", "powerpc64-unknown-linux-gnu"
        )
        self.do_test_cross_gcc_32_64(
            "powerpc64-unknown-linux-gnu", "powerpc-unknown-linux-gnu"
        )

    def do_test_cross_gcc(self, host, target):
        self.HOST = host
        self.TARGET = target
        host_cpu = host.split("-")[0]
        cpu, manufacturer, os = target.split("-", 2)
        toolchain_prefix = "/usr/bin/%s-%s" % (cpu, os)
        paths = {
            "/usr/bin/gcc": DEFAULT_GCC + self.PLATFORMS[host],
            "/usr/bin/g++": DEFAULT_GXX + self.PLATFORMS[host],
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": (
                    "Target C compiler target CPU (%s) "
                    "does not match --target CPU (%s)" % (host_cpu, cpu)
                ),
            },
        )

        paths.update(
            {
                "%s-gcc" % toolchain_prefix: DEFAULT_GCC + self.PLATFORMS[target],
                "%s-g++" % toolchain_prefix: DEFAULT_GXX + self.PLATFORMS[target],
            }
        )
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT
                + {
                    "compiler": "%s-gcc" % toolchain_prefix,
                },
                "cxx_compiler": self.DEFAULT_GXX_RESULT
                + {
                    "compiler": "%s-g++" % toolchain_prefix,
                },
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
        )
        self.HOST = LinuxCrossCompileToolchainTest.HOST
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_cross_gcc_misc(self):
        for target in self.PLATFORMS:
            if not target.endswith("-pc-linux-gnu"):
                self.do_test_cross_gcc("x86_64-pc-linux-gnu", target)

    def test_cannot_cross(self):
        self.TARGET = "mipsel-unknown-linux-gnu"

        paths = {
            "/usr/bin/gcc": DEFAULT_GCC + self.PLATFORMS["mips-unknown-linux-gnu"],
            "/usr/bin/g++": DEFAULT_GXX + self.PLATFORMS["mips-unknown-linux-gnu"],
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": (
                    "Target C compiler target endianness (big) "
                    "does not match --target endianness (little)"
                ),
            },
        )
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_overridden_cross_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.ARM_GCC_7_RESULT,
                "cxx_compiler": self.ARM_GXX_7_RESULT,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "arm-linux-gnu-gcc-7",
                "CXX": "arm-linux-gnu-g++-7",
            },
        )

    def test_overridden_unsupported_cross_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.ARM_GCC_4_9_RESULT,
            },
            environ={
                "CC": "arm-linux-gnu-gcc-4.9",
                "CXX": "arm-linux-gnu-g++-4.9",
            },
        )

    def test_guess_cross_cxx(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.ARM_GCC_7_RESULT,
                "cxx_compiler": self.ARM_GXX_7_RESULT,
                "host_c_compiler": self.DEFAULT_GCC_RESULT,
                "host_cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
            environ={
                "CC": "arm-linux-gnu-gcc-7",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.ARM_DEFAULT_GCC_RESULT,
                "cxx_compiler": self.ARM_DEFAULT_GXX_RESULT,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
            environ={
                "CC": "arm-linux-gnu-gcc",
                "HOST_CC": "clang",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.ARM_DEFAULT_GCC_RESULT,
                "cxx_compiler": self.ARM_DEFAULT_GXX_RESULT,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
            environ={
                "CC": "arm-linux-gnu-gcc",
                "CXX": "arm-linux-gnu-g++",
                "HOST_CC": "clang",
            },
        )

    def test_cross_clang(self):
        cross_clang_result = self.DEFAULT_CLANG_RESULT + {
            "flags": ["--target=arm-linux-gnu"],
        }
        cross_clangxx_result = self.DEFAULT_CLANGXX_RESULT + {
            "flags": ["--target=arm-linux-gnu"],
        }
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": cross_clang_result,
                "cxx_compiler": cross_clangxx_result,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
            environ={
                "CC": "clang",
                "HOST_CC": "clang",
            },
        )

        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": cross_clang_result,
                "cxx_compiler": cross_clangxx_result,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
            environ={
                "CC": "clang",
            },
        )

    def test_cross_atypical_clang(self):
        paths = dict(self.PATHS)
        paths.update(
            {
                "/usr/bin/afl-clang-fast": paths["/usr/bin/clang"],
                "/usr/bin/afl-clang-fast++": paths["/usr/bin/clang++"],
            }
        )
        afl_clang_result = self.DEFAULT_CLANG_RESULT + {
            "compiler": "/usr/bin/afl-clang-fast",
        }
        afl_clangxx_result = self.DEFAULT_CLANGXX_RESULT + {
            "compiler": "/usr/bin/afl-clang-fast++",
        }
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": afl_clang_result
                + {
                    "flags": ["--target=arm-linux-gnu"],
                },
                "cxx_compiler": afl_clangxx_result
                + {
                    "flags": ["--target=arm-linux-gnu"],
                },
                "host_c_compiler": afl_clang_result,
                "host_cxx_compiler": afl_clangxx_result,
            },
            environ={
                "CC": "afl-clang-fast",
                "CXX": "afl-clang-fast++",
            },
        )


class OSXCrossToolchainTest(BaseToolchainTest):
    TARGET = "i686-apple-darwin11.2.0"
    PATHS = dict(LinuxToolchainTest.PATHS)
    PATHS.update(
        {
            "/usr/bin/clang": CLANG_5_0 + CLANG_PLATFORM_X86_64_LINUX,
            "/usr/bin/clang++": CLANGXX_5_0 + CLANG_PLATFORM_X86_64_LINUX,
        }
    )
    DEFAULT_CLANG_RESULT = CompilerResult(
        flags=["-std=gnu99"],
        version="5.0.1",
        type="clang",
        compiler="/usr/bin/clang",
        language="C",
    )
    DEFAULT_CLANGXX_RESULT = CompilerResult(
        flags=["-std=gnu++17"],
        version="5.0.1",
        type="clang",
        compiler="/usr/bin/clang++",
        language="C++",
    )

    def test_osx_cross(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_CLANG_RESULT
                + OSXToolchainTest.SYSROOT_FLAGS
                + {
                    "flags": ["--target=i686-apple-darwin11.2.0"],
                },
                "cxx_compiler": self.DEFAULT_CLANGXX_RESULT
                + OSXToolchainTest.SYSROOT_FLAGS
                + {
                    "flags": ["--target=i686-apple-darwin11.2.0"],
                },
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
            environ={
                "CC": "clang",
            },
            args=["--with-macos-sdk=%s" % OSXToolchainTest.SYSROOT_FLAGS["flags"][1]],
        )

    def test_cannot_osx_cross(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": "Target C compiler target kernel (Linux) does not "
                "match --target kernel (Darwin)",
            },
            environ={
                "CC": "gcc",
            },
            args=["--with-macos-sdk=%s" % OSXToolchainTest.SYSROOT_FLAGS["flags"][1]],
        )


class WindowsCrossToolchainTest(BaseToolchainTest):
    TARGET = "x86_64-pc-mingw32"
    DEFAULT_CLANG_RESULT = LinuxToolchainTest.DEFAULT_CLANG_RESULT
    DEFAULT_CLANGXX_RESULT = LinuxToolchainTest.DEFAULT_CLANGXX_RESULT

    def test_clang_cl_cross(self):
        paths = {
            "/usr/bin/clang-cl": CLANG_CL_8_0 + CLANG_CL_PLATFORM_X86_64,
        }
        paths.update(LinuxToolchainTest.PATHS)
        self.do_toolchain_test(
            paths,
            {
                "c_compiler": WindowsToolchainTest.CLANG_CL_8_0_RESULT,
                "cxx_compiler": WindowsToolchainTest.CLANGXX_CL_8_0_RESULT,
                "host_c_compiler": self.DEFAULT_CLANG_RESULT,
                "host_cxx_compiler": self.DEFAULT_CLANGXX_RESULT,
            },
        )


class OpenBSDToolchainTest(BaseToolchainTest):
    HOST = "x86_64-unknown-openbsd6.1"
    TARGET = "x86_64-unknown-openbsd6.1"
    PATHS = {
        "/usr/bin/gcc": DEFAULT_GCC + GCC_PLATFORM_X86_64 + GCC_PLATFORM_OPENBSD,
        "/usr/bin/g++": DEFAULT_GXX + GCC_PLATFORM_X86_64 + GCC_PLATFORM_OPENBSD,
    }
    DEFAULT_GCC_RESULT = LinuxToolchainTest.DEFAULT_GCC_RESULT
    DEFAULT_GXX_RESULT = LinuxToolchainTest.DEFAULT_GXX_RESULT

    def test_gcc(self):
        self.do_toolchain_test(
            self.PATHS,
            {
                "c_compiler": self.DEFAULT_GCC_RESULT,
                "cxx_compiler": self.DEFAULT_GXX_RESULT,
            },
        )


@memoize
def gen_invoke_cargo(version, rustup_wrapper=False):
    def invoke_cargo(stdin, args):
        args = tuple(args)
        if not rustup_wrapper and args == ("+stable",):
            return (101, "", "we are the real thing")
        if args == ("--version", "--verbose"):
            return 0, "cargo %s\nrelease: %s" % (version, version), ""
        raise NotImplementedError("unsupported arguments")

    return invoke_cargo


@memoize
def gen_invoke_rustc(version, rustup_wrapper=False):
    def invoke_rustc(stdin, args):
        args = tuple(args)
        # TODO: we don't have enough machinery set up to test the `rustup which`
        # fallback yet.
        if not rustup_wrapper and args == ("+stable",):
            return (1, "", "error: couldn't read +stable: No such file or directory")
        if args == ("--version", "--verbose"):
            return (
                0,
                "rustc %s\nrelease: %s\nhost: x86_64-unknown-linux-gnu"
                % (version, version),
                "",
            )
        if args == ("--print", "target-list"):
            # Raw list returned by rustc version 1.32, + ios, which somehow
            # don't appear in the default list.
            # https://github.com/rust-lang/rust/issues/36156
            rust_targets = [
                "aarch64-apple-ios",
                "aarch64-fuchsia",
                "aarch64-linux-android",
                "aarch64-pc-windows-msvc",
                "aarch64-unknown-cloudabi",
                "aarch64-unknown-freebsd",
                "aarch64-unknown-hermit",
                "aarch64-unknown-linux-gnu",
                "aarch64-unknown-linux-musl",
                "aarch64-unknown-netbsd",
                "aarch64-unknown-none",
                "aarch64-unknown-openbsd",
                "arm-linux-androideabi",
                "arm-unknown-linux-gnueabi",
                "arm-unknown-linux-gnueabihf",
                "arm-unknown-linux-musleabi",
                "arm-unknown-linux-musleabihf",
                "armebv7r-none-eabi",
                "armebv7r-none-eabihf",
                "armv4t-unknown-linux-gnueabi",
                "armv5te-unknown-linux-gnueabi",
                "armv5te-unknown-linux-musleabi",
                "armv6-unknown-netbsd-eabihf",
                "armv7-linux-androideabi",
                "armv7-unknown-cloudabi-eabihf",
                "armv7-unknown-linux-gnueabihf",
                "armv7-unknown-linux-musleabihf",
                "armv7-unknown-netbsd-eabihf",
                "armv7r-none-eabi",
                "armv7r-none-eabihf",
                "armv7s-apple-ios",
                "asmjs-unknown-emscripten",
                "i386-apple-ios",
                "i586-pc-windows-msvc",
                "i586-unknown-linux-gnu",
                "i586-unknown-linux-musl",
                "i686-apple-darwin",
                "i686-linux-android",
                "i686-pc-windows-gnu",
                "i686-pc-windows-msvc",
                "i686-unknown-cloudabi",
                "i686-unknown-dragonfly",
                "i686-unknown-freebsd",
                "i686-unknown-haiku",
                "i686-unknown-linux-gnu",
                "i686-unknown-linux-musl",
                "i686-unknown-netbsd",
                "i686-unknown-openbsd",
                "mips-unknown-linux-gnu",
                "mips-unknown-linux-musl",
                "mips-unknown-linux-uclibc",
                "mips64-unknown-linux-gnuabi64",
                "mips64el-unknown-linux-gnuabi64",
                "mipsel-unknown-linux-gnu",
                "mipsel-unknown-linux-musl",
                "mipsel-unknown-linux-uclibc",
                "msp430-none-elf",
                "powerpc-unknown-linux-gnu",
                "powerpc-unknown-linux-gnuspe",
                "powerpc-unknown-linux-musl",
                "powerpc-unknown-netbsd",
                "powerpc64-unknown-linux-gnu",
                "powerpc64-unknown-linux-musl",
                "powerpc64le-unknown-linux-gnu",
                "powerpc64le-unknown-linux-musl",
                "riscv32imac-unknown-none-elf",
                "riscv32imc-unknown-none-elf",
                "s390x-unknown-linux-gnu",
                "sparc-unknown-linux-gnu",
                "sparc64-unknown-linux-gnu",
                "sparc64-unknown-netbsd",
                "sparcv9-sun-solaris",
                "thumbv6m-none-eabi",
                "thumbv7a-pc-windows-msvc",
                "thumbv7em-none-eabi",
                "thumbv7em-none-eabihf",
                "thumbv7m-none-eabi",
                "thumbv8m.base-none-eabi",
                "wasm32-experimental-emscripten",
                "wasm32-unknown-emscripten",
                "wasm32-unknown-unknown",
                "x86_64-apple-darwin",
                "x86_64-apple-ios",
                "x86_64-fortanix-unknown-sgx",
                "x86_64-fuchsia",
                "x86_64-linux-android",
                "x86_64-pc-windows-gnu",
                "x86_64-pc-windows-msvc",
                "x86_64-rumprun-netbsd",
                "x86_64-sun-solaris",
                "x86_64-unknown-bitrig",
                "x86_64-unknown-cloudabi",
                "x86_64-unknown-dragonfly",
                "x86_64-unknown-freebsd",
                "x86_64-unknown-haiku",
                "x86_64-unknown-hermit",
                "x86_64-unknown-l4re-uclibc",
                "x86_64-unknown-linux-gnu",
                "x86_64-unknown-linux-gnux32",
                "x86_64-unknown-linux-musl",
                "x86_64-unknown-netbsd",
                "x86_64-unknown-openbsd",
                "x86_64-unknown-redox",
            ]
            # Additional targets from 1.33
            if Version(version) >= "1.33.0":
                rust_targets += [
                    "thumbv7neon-linux-androideabi",
                    "thumbv7neon-unknown-linux-gnueabihf",
                    "x86_64-unknown-uefi",
                    "thumbv8m.main-none-eabi",
                    "thumbv8m.main-none-eabihf",
                ]
            # Additional targets from 1.34
            if Version(version) >= "1.34.0":
                rust_targets += [
                    "nvptx64-nvidia-cuda",
                    "powerpc64-unknown-freebsd",
                    "riscv64gc-unknown-none-elf",
                    "riscv64imac-unknown-none-elf",
                ]
            # Additional targets from 1.35
            if Version(version) >= "1.35.0":
                rust_targets += [
                    "armv6-unknown-freebsd",
                    "armv7-unknown-freebsd",
                    "mipsisa32r6-unknown-linux-gnu",
                    "mipsisa32r6el-unknown-linux-gnu",
                    "mipsisa64r6-unknown-linux-gnuabi64",
                    "mipsisa64r6el-unknown-linux-gnuabi64",
                    "wasm32-unknown-wasi",
                ]
            # Additional targets from 1.36
            if Version(version) >= "1.36.0":
                rust_targets += [
                    "wasm32-wasi",
                ]
                rust_targets.remove("wasm32-unknown-wasi")
                rust_targets.remove("x86_64-unknown-bitrig")
            # Additional targets from 1.37
            if Version(version) >= "1.37.0":
                rust_targets += [
                    "x86_64-pc-solaris",
                ]
            # Additional targets from 1.38
            if Version(version) >= "1.38.0":
                rust_targets += [
                    "aarch64-unknown-redox",
                    "aarch64-wrs-vxworks",
                    "armv7-unknown-linux-gnueabi",
                    "armv7-unknown-linux-musleabi",
                    "armv7-wrs-vxworks",
                    "hexagon-unknown-linux-musl",
                    "i586-wrs-vxworks",
                    "i686-uwp-windows-gnu",
                    "i686-wrs-vxworks",
                    "powerpc-wrs-vxworks",
                    "powerpc-wrs-vxworks-spe",
                    "powerpc64-wrs-vxworks",
                    "riscv32i-unknown-none-elf",
                    "x86_64-uwp-windows-gnu",
                    "x86_64-wrs-vxworks",
                ]
            # Additional targets from 1.38
            if Version(version) >= "1.39.0":
                rust_targets += [
                    "aarch64-uwp-windows-msvc",
                    "armv7-wrs-vxworks-eabihf",
                    "i686-unknown-uefi",
                    "i686-uwp-windows-msvc",
                    "mips64-unknown-linux-muslabi64",
                    "mips64el-unknown-linux-muslabi64",
                    "sparc64-unknown-openbsd",
                    "x86_64-linux-kernel",
                    "x86_64-uwp-windows-msvc",
                ]
                rust_targets.remove("armv7-wrs-vxworks")
                rust_targets.remove("i586-wrs-vxworks")

            return 0, "\n".join(sorted(rust_targets)), ""
        if (
            len(args) == 6
            and args[:2] == ("--crate-type", "staticlib")
            and args[2].startswith("--target=")
            and args[3] == "-o"
        ):
            with open(args[4], "w") as fh:
                fh.write("foo")
            return 0, "", ""
        raise NotImplementedError("unsupported arguments")

    return invoke_rustc


class RustTest(BaseConfigureTest):
    def get_rust_target(
        self, target, compiler_type="gcc", version="1.47.0", arm_target=None
    ):
        environ = {
            "PATH": os.pathsep.join(mozpath.abspath(p) for p in ("/bin", "/usr/bin")),
        }

        paths = {
            mozpath.abspath("/usr/bin/cargo"): gen_invoke_cargo(version),
            mozpath.abspath("/usr/bin/rustc"): gen_invoke_rustc(version),
        }

        self.TARGET = target
        sandbox = self.get_sandbox(paths, {}, [], environ)

        # Trick the sandbox into not running the target compiler check
        dep = sandbox._depends[sandbox["c_compiler"]]
        getattr(sandbox, "__value_for_depends")[(dep,)] = CompilerResult(
            type=compiler_type
        )
        # Same for the arm_target checks.
        dep = sandbox._depends[sandbox["arm_target"]]
        getattr(sandbox, "__value_for_depends")[
            (dep,)
        ] = arm_target or ReadOnlyNamespace(
            arm_arch=7, thumb2=False, fpu="vfpv2", float_abi="softfp"
        )
        return sandbox._value_for(sandbox["rust_target_triple"])

    def test_rust_target(self):
        # Cases where the output of config.sub matches a rust target
        for straightforward in (
            "x86_64-unknown-dragonfly",
            "aarch64-unknown-freebsd",
            "i686-unknown-freebsd",
            "x86_64-unknown-freebsd",
            "sparc64-unknown-netbsd",
            "i686-unknown-netbsd",
            "x86_64-unknown-netbsd",
            "i686-unknown-openbsd",
            "x86_64-unknown-openbsd",
            "aarch64-unknown-linux-gnu",
            "sparc64-unknown-linux-gnu",
            "i686-unknown-linux-gnu",
            "i686-apple-darwin",
            "x86_64-apple-darwin",
            "mips-unknown-linux-gnu",
            "mipsel-unknown-linux-gnu",
            "mips64-unknown-linux-gnuabi64",
            "mips64el-unknown-linux-gnuabi64",
            "powerpc64-unknown-linux-gnu",
            "powerpc64le-unknown-linux-gnu",
        ):
            self.assertEqual(self.get_rust_target(straightforward), straightforward)

        # Cases where the output of config.sub is different
        for autoconf, rust in (
            ("aarch64-unknown-linux-android", "aarch64-linux-android"),
            ("arm-unknown-linux-androideabi", "armv7-linux-androideabi"),
            ("armv7-unknown-linux-androideabi", "armv7-linux-androideabi"),
            ("i386-unknown-linux-android", "i686-linux-android"),
            ("i686-unknown-linux-android", "i686-linux-android"),
            ("i686-pc-linux-gnu", "i686-unknown-linux-gnu"),
            ("x86_64-unknown-linux-android", "x86_64-linux-android"),
            ("x86_64-pc-linux-gnu", "x86_64-unknown-linux-gnu"),
            ("sparcv9-sun-solaris2", "sparcv9-sun-solaris"),
            ("x86_64-sun-solaris2", "x86_64-sun-solaris"),
        ):
            self.assertEqual(self.get_rust_target(autoconf), rust)

        # Windows
        for autoconf, building_with_gcc, rust in (
            ("i686-pc-mingw32", "cl", "i686-pc-windows-msvc"),
            ("x86_64-pc-mingw32", "cl", "x86_64-pc-windows-msvc"),
            ("i686-pc-mingw32", "gcc", "i686-pc-windows-gnu"),
            ("x86_64-pc-mingw32", "gcc", "x86_64-pc-windows-gnu"),
            ("i686-pc-mingw32", "clang", "i686-pc-windows-gnu"),
            ("x86_64-pc-mingw32", "clang", "x86_64-pc-windows-gnu"),
            ("i686-w64-mingw32", "clang", "i686-pc-windows-gnu"),
            ("x86_64-w64-mingw32", "clang", "x86_64-pc-windows-gnu"),
            ("aarch64-windows-mingw32", "clang-cl", "aarch64-pc-windows-msvc"),
        ):
            self.assertEqual(self.get_rust_target(autoconf, building_with_gcc), rust)

        # Arm special cases
        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-linux-androideabi",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="neon", thumb2=True, float_abi="softfp"
                ),
            ),
            "thumbv7neon-linux-androideabi",
        )

        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-linux-androideabi",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="neon", thumb2=False, float_abi="softfp"
                ),
            ),
            "armv7-linux-androideabi",
        )

        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-linux-androideabi",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="vfpv2", thumb2=True, float_abi="softfp"
                ),
            ),
            "armv7-linux-androideabi",
        )

        self.assertEqual(
            self.get_rust_target(
                "armv7-unknown-linux-gnueabihf",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="neon", thumb2=True, float_abi="hard"
                ),
            ),
            "thumbv7neon-unknown-linux-gnueabihf",
        )

        self.assertEqual(
            self.get_rust_target(
                "armv7-unknown-linux-gnueabihf",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="neon", thumb2=False, float_abi="hard"
                ),
            ),
            "armv7-unknown-linux-gnueabihf",
        )

        self.assertEqual(
            self.get_rust_target(
                "armv7-unknown-linux-gnueabihf",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="vfpv2", thumb2=True, float_abi="hard"
                ),
            ),
            "armv7-unknown-linux-gnueabihf",
        )

        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-freebsd13.0-gnueabihf",
                arm_target=ReadOnlyNamespace(
                    arm_arch=7, fpu="vfpv2", thumb2=True, float_abi="hard"
                ),
            ),
            "armv7-unknown-freebsd",
        )

        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-freebsd13.0-gnueabihf",
                arm_target=ReadOnlyNamespace(
                    arm_arch=6, fpu=None, thumb2=False, float_abi="hard"
                ),
            ),
            "armv6-unknown-freebsd",
        )

        self.assertEqual(
            self.get_rust_target(
                "arm-unknown-linux-gnueabi",
                arm_target=ReadOnlyNamespace(
                    arm_arch=4, fpu=None, thumb2=False, float_abi="softfp"
                ),
            ),
            "armv4t-unknown-linux-gnueabi",
        )


if __name__ == "__main__":
    main()
