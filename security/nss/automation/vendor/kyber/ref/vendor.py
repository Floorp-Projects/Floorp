#!/usr/bin/python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import requests
import sys
import tarfile


def remove_include_guard(x: io.StringIO, guard: str) -> io.StringIO:
    out = io.StringIO()
    depth = 0
    inside_guard = False
    for line in x.readlines():
        tokens = line.split()
        if tokens and tokens[0] in ["#if", "#ifdef", "#ifndef"]:
            depth += 1
        if len(tokens) > 1 and tokens[0] == "#ifndef" and tokens[1] == guard:
            assert depth == 1, "error: nested include guard"
            inside_guard = True
            continue
        if len(tokens) > 1 and tokens[0] == "#define" and tokens[1] == guard:
            continue
        if tokens and tokens[0] == "#endif":
            depth -= 1
            if depth == 0 and inside_guard:
                inside_guard = False
                continue
        out.write(line)
    out.seek(0)
    return out


def remove_includes(x: io.StringIO) -> io.StringIO:
    out = io.StringIO()
    for line in x.readlines():
        tokens = line.split()
        if tokens and tokens[0] == "#include":
            continue
        out.write(line)
    out.seek(0)
    return out


# Take the else branch of any #ifdef KYBER90s ... #else ... #endif
def remove_kyber90s(x: io.StringIO) -> io.StringIO:
    out = io.StringIO()
    states = ["before", "during-drop", "during-keep"]
    state = "before"
    current_depth = 0
    kyber90s_depth = None
    for line in x.readlines():
        tokens = line.split()
        if tokens and tokens[0] in ["#if", "#ifdef", "#ifndef"]:
            current_depth += 1
        if len(tokens) > 1 and tokens[0] == "#ifdef" and tokens[1] == "KYBER_90S":
            assert kyber90s_depth == None, "cannot handle nested #ifdef KYBER90S"
            kyber90s_depth = current_depth
            state = "during-drop"
            continue
        if len(tokens) > 1 and tokens[0] == "#ifndef" and tokens[1] == "KYBER_90S":
            assert kyber90s_depth == None, "cannot handle nested #ifndef KYBER90S"
            kyber90s_depth = current_depth
            state = "during-keep"
            continue
        if current_depth == kyber90s_depth and tokens:
            if tokens[0] == "#else":
                assert state != "before"
                state = "during-keep" if state == "during-drop" else "during-drop"
                continue
            if tokens[0] == "#elif":
                assert False, "cannot handle #elif branch of #ifdef KYBER90S"
            if tokens[0] == "#endif":
                assert state != "before"
                state = "before"
                kyber90s_depth = None
                current_depth -= 1
                continue
        if tokens and tokens[0] == "#endif":
            current_depth -= 1
        if state == "during-drop":
            continue
        out.write(line)
    out.seek(0)
    return out


def add_static_to_fns(x: io.StringIO) -> io.StringIO:
    out = io.StringIO()
    depth = 0
    for line in x.readlines():
        tokens = line.split()
        # assumes return type starts on column 0
        if depth == 0 and any(
            line.startswith(typ) for typ in ["void", "uint32_t", "int16_t", "int"]
        ):
            out.write("static " + line)
        else:
            out.write(line)
        if "{" in line:
            depth += 1
        if "}" in line:
            depth -= 1
    out.seek(0)
    return out


def file_block(x: io.StringIO, filename: str) -> io.StringIO:
    out = io.StringIO()
    out.write(f"\n/** begin: {filename} **/\n")
    out.write(x.read().strip())
    out.write(f"\n/** end: {filename} **/\n")
    out.seek(0)
    return out


def test():
    assert 0 == len(remove_includes(io.StringIO("#include <stddef.h>")).read())
    assert 0 == len(remove_kyber90s(io.StringIO("#ifdef KYBER_90S\nx\n#endif")).read())

    test_remove_kyber90s_expect = "#ifdef OTHER\nx\n#else\nx\n#endif"
    test_remove_ifdef_kyber90s = f"""
#ifdef KYBER_90S
x
{test_remove_kyber90s_expect}
x
#else
{test_remove_kyber90s_expect}
#endif
"""
    test_remove_ifdef_kyber90s_actual = (
        remove_kyber90s(io.StringIO(test_remove_ifdef_kyber90s)).read().strip()
    )
    assert (
        test_remove_kyber90s_expect == test_remove_ifdef_kyber90s_actual
    ), "remove_kyber90s unit test"

    test_remove_ifndef_kyber90s = f"""
#ifndef KYBER_90S
{test_remove_kyber90s_expect}
#else
x
{test_remove_kyber90s_expect}
x
#endif
"""
    test_remove_ifndef_kyber90s_actual = (
        remove_kyber90s(io.StringIO(test_remove_ifndef_kyber90s)).read().strip()
    )
    assert (
        test_remove_kyber90s_expect == test_remove_ifndef_kyber90s_actual
    ), "remove_kyber90s unit test"

    test_add_static_to_fns = """\
void fn() {
int x[3] = {1,2,3};
}"""
    assert (
        f"static {test_add_static_to_fns}"
        == add_static_to_fns(io.StringIO(test_add_static_to_fns)).read()
    )

    test_remove_include_guard = """\
#ifndef TEST_H
#define TEST_H
#endif"""

    assert 0 == len(
        remove_include_guard(io.StringIO(test_remove_include_guard), "TEST_H").read()
    )
    assert (
        test_remove_include_guard
        == remove_include_guard(
            io.StringIO(test_remove_include_guard), "OTHER_H"
        ).read()
    )


def is_hex(s: str) -> bool:
    try:
        int(s, 16)
    except ValueError:
        return False
    return True


if __name__ == "__main__":
    test()

    repo = f"https://github.com/pq-crystals/kyber"
    out = "kyber-pqcrystals-ref.c"
    out_api = "kyber-pqcrystals-ref.h"
    out_orig = "kyber-pqcrystals-ref.c.orig"

    if len(sys.argv) == 2 and len(sys.argv[1]) >= 6 and is_hex(sys.argv[1]):
        commit = sys.argv[1]
        print(f"* using commit id {commit}")
    else:
        print(
            f"""\
Usage: python3 {sys.argv[0]} [commit]
       where [commit] is an 8+ hex digit commit id from {repo}.
"""
        )
        sys.exit(1)

    short_commit = commit[:8]
    tarball_url = f"{repo}/tarball/{commit}"
    archive = f"kyber-{short_commit}.tar.gz"

    headers = [
        "params.h",
        "reduce.h",
        "ntt.h",
        "poly.h",
        "cbd.h",
        "polyvec.h",
        "indcpa.h",
        "fips202.h",
        "symmetric.h",
        "kem.h",
    ]

    sources = [
        "reduce.c",
        "cbd.c",
        "ntt.c",
        "poly.c",
        "polyvec.c",
        "indcpa.c",
        "fips202.c",
        "symmetric-shake.c",
        "kem.c",
    ]

    if not os.path.isfile(archive):
        print(f"* fetching {tarball_url}")
        req = requests.request(method="GET", url=tarball_url)
        if not req.ok:
            print(f"* failed: {req.reason}")
            sys.exit(1)
        with open(archive, "wb") as f:
            f.write(req.content)

    print(f"* extracting files from {archive}")
    with open(archive, "rb") as f:
        tarball = tarfile.open(mode="r:gz", fileobj=f)

        topdir = tarball.members[0].path
        assert (
            topdir == f"pq-crystals-kyber-{commit[:7]}"
        ), "tarball directory structure changed"

        # Write a single-file copy without modifications for easy diffing
        print(f"* writing unmodified files to {out_orig}")
        with open(out_orig, "w") as f:
            for filename in headers:
                x = tarball.extractfile(f"{topdir}/ref/{filename}")
                x = io.StringIO(x.read().decode("utf-8"))
                x = file_block(x, "ref/" + filename)
                f.write(x.read())

            for filename in sources:
                x = tarball.extractfile(f"{topdir}/ref/{filename}")
                x = io.StringIO(x.read().decode("utf-8"))
                x = file_block(x, "ref/" + filename)
                f.write(x.read())

        comment = io.StringIO()
        comment.write(
            f"""/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file was generated from
 *   https://github.com/pq-crystals/kyber/commit/{short_commit}
 *
 * Files from that repository are listed here surrounded by
 * "* begin: [file] *" and "* end: [file] *" comments.
 *
 * The following changes have been made:
 *  - include guards have been removed,
 *  - include directives have been removed,
 *  - "#ifdef KYBER90S" blocks have been evaluated with "KYBER90S" undefined,
 *  - functions outside of kem.c have been made static.
*/
"""
        )
        for filename in ["LICENSE", "AUTHORS"]:
            comment.write(f"""\n/** begin: ref/{filename} **\n""")
            x = tarball.extractfile(f"{topdir}/{filename}")
            x = io.StringIO(x.read().decode("utf-8"))
            for line in x.readlines():
                comment.write(line)
            comment.write(f"""** end: ref/{filename} **/\n""")
        comment.seek(0)

        print(f"* writing modified files to {out}")
        with open(out, "w") as f:
            f.write(comment.read())
            f.write(
                """
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secport.h"

// We need to provide an implementation of randombytes to avoid an unused
// function warning. We don't use the randomized API in freebl, so we'll make
// calling randombytes an error.
static void randombytes(uint8_t *out, size_t outlen) {
    // this memset is to avoid "maybe-uninitialized" warnings that gcc-11 issues
    // for the (unused) crypto_kem_keypair and crypto_kem_enc functions.
    memset(out, 0, outlen);
    assert(0);
}

/*************************************************
* Name:        verify
*
* Description: Compare two arrays for equality in constant time.
*
* Arguments:   const uint8_t *a: pointer to first byte array
*              const uint8_t *b: pointer to second byte array
*              size_t len:       length of the byte arrays
*
* Returns 0 if the byte arrays are equal, 1 otherwise
**************************************************/
static int verify(const uint8_t *a, const uint8_t *b, size_t len) {
    return NSS_SecureMemcmp(a, b, len);
}

/*************************************************
* Name:        cmov
*
* Description: Copy len bytes from x to r if b is 1;
*              don't modify x if b is 0. Requires b to be in {0,1};
*              assumes two's complement representation of negative integers.
*              Runs in constant time.
*
* Arguments:   uint8_t *r:       pointer to output byte array
*              const uint8_t *x: pointer to input byte array
*              size_t len:       Amount of bytes to be copied
*              uint8_t b:        Condition bit; has to be in {0,1}
**************************************************/
static void cmov(uint8_t *r, const uint8_t *x, size_t len, uint8_t b)
{
    NSS_SecureSelect(r, r, x, len, b);
}
"""
            )
            for filename in headers:
                x = tarball.extractfile(f"{topdir}/ref/{filename}")
                x = io.StringIO(x.read().decode("utf-8"))
                x = remove_include_guard(x, filename.upper().replace(".", "_"))
                x = remove_includes(x)
                x = remove_kyber90s(x)
                if filename not in ["kem.h", "fips202.h"]:
                    x = add_static_to_fns(x)
                x = file_block(x, "ref/" + filename)
                f.write(x.read())

            for filename in sources:
                x = tarball.extractfile(f"{topdir}/ref/{filename}")
                x = io.StringIO(x.read().decode("utf-8"))
                x = remove_includes(x)
                x = remove_kyber90s(x)
                if filename not in ["kem.c", "fips202.c"]:
                    x = add_static_to_fns(x)
                x = file_block(x, "ref/" + filename)
                f.write(x.read())

        print(f"* writing private header to {out_api}")
        with open(out_api, "w") as f:
            filename = "api.h"
            comment.seek(0)
            f.write(comment.read())
            f.write(
                """
#ifndef KYBER_PQCRYSTALS_REF_H
#define KYBER_PQCRYSTALS_REF_H
"""
            )
            x = tarball.extractfile(f"{topdir}/ref/{filename}")
            x = io.StringIO(x.read().decode("utf-8"))
            x = remove_include_guard(x, filename.upper().replace(".", "_"))
            x = file_block(x, "ref/" + filename)
            f.write(x.read())
            f.write(
                f"""
#endif // KYBER_PQCRYSTALS_REF_H
"""
            )
        print(
            f"""* done!

You should now:
    1) Check the output by running `diff {out_orig} {out}`
    2) Move {out} to lib/freebl/{out}
    3) Move {out_api} to lib/freebl/{out_api}
    4) Delete {out_orig} and {archive}.
"""
        )
