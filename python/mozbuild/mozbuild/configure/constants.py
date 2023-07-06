# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import OrderedDict

from mozbuild.util import EnumString


class CompilerType(EnumString):
    POSSIBLE_VALUES = (
        "clang",
        "clang-cl",
        "gcc",
        "msvc",
    )


class OS(EnumString):
    POSSIBLE_VALUES = (
        "Android",
        "DragonFly",
        "FreeBSD",
        "GNU",
        "NetBSD",
        "OpenBSD",
        "OSX",
        "SunOS",
        "WINNT",
        "WASI",
    )


class Kernel(EnumString):
    POSSIBLE_VALUES = (
        "Darwin",
        "DragonFly",
        "FreeBSD",
        "kFreeBSD",
        "Linux",
        "NetBSD",
        "OpenBSD",
        "SunOS",
        "WINNT",
        "WASI",
    )


CPU_bitness = {
    "aarch64": 64,
    "Alpha": 64,
    "arm": 32,
    "hppa": 32,
    "ia64": 64,
    "loongarch64": 64,
    "m68k": 32,
    "mips32": 32,
    "mips64": 64,
    "ppc": 32,
    "ppc64": 64,
    "riscv64": 64,
    "s390": 32,
    "s390x": 64,
    "sh4": 32,
    "sparc": 32,
    "sparc64": 64,
    "x86": 32,
    "x86_64": 64,
    "wasm32": 32,
}


class CPU(EnumString):
    POSSIBLE_VALUES = CPU_bitness.keys()


class Endianness(EnumString):
    POSSIBLE_VALUES = (
        "big",
        "little",
    )


class WindowsBinaryType(EnumString):
    POSSIBLE_VALUES = (
        "win32",
        "win64",
    )


class Abi(EnumString):
    POSSIBLE_VALUES = (
        "msvc",
        "mingw",
    )


# The order of those checks matter
CPU_preprocessor_checks = OrderedDict(
    (
        ("x86", "__i386__ || _M_IX86"),
        ("x86_64", "__x86_64__ || _M_X64"),
        ("arm", "__arm__ || _M_ARM"),
        ("aarch64", "__aarch64__ || _M_ARM64"),
        ("ia64", "__ia64__"),
        ("s390x", "__s390x__"),
        ("s390", "__s390__"),
        ("ppc64", "__powerpc64__"),
        ("ppc", "__powerpc__"),
        ("Alpha", "__alpha__"),
        ("hppa", "__hppa__"),
        ("sparc64", "__sparc__ && __arch64__"),
        ("sparc", "__sparc__"),
        ("m68k", "__m68k__"),
        ("mips64", "__mips64"),
        ("mips32", "__mips__"),
        ("riscv64", "__riscv && __riscv_xlen == 64"),
        ("loongarch64", "__loongarch64"),
        ("sh4", "__sh__"),
        ("wasm32", "__wasm32__"),
    )
)

assert sorted(CPU_preprocessor_checks.keys()) == sorted(CPU.POSSIBLE_VALUES)

kernel_preprocessor_checks = {
    "Darwin": "__APPLE__",
    "DragonFly": "__DragonFly__",
    "FreeBSD": "__FreeBSD__",
    "kFreeBSD": "__FreeBSD_kernel__",
    "Linux": "__linux__",
    "NetBSD": "__NetBSD__",
    "OpenBSD": "__OpenBSD__",
    "SunOS": "__sun__",
    "WINNT": "_WIN32 || __CYGWIN__",
    "WASI": "__wasi__",
}

assert sorted(kernel_preprocessor_checks.keys()) == sorted(Kernel.POSSIBLE_VALUES)

OS_preprocessor_checks = {
    "Android": "__ANDROID__",
}

# We intentionally don't include all possible OSes in our checks, because we
# only care about OS mismatches for specific target OSes.
# assert sorted(OS_preprocessor_checks.keys()) == sorted(OS.POSSIBLE_VALUES)
