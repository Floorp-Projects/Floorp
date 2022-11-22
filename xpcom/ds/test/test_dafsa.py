# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from io import StringIO

import mozunit
from incremental_dafsa import Dafsa, Node


def _node_to_string(node: Node, prefix, buffer, cache):
    if not node.is_end_node:
        prefix += (
            str(ord(node.character)) if ord(node.character) < 10 else node.character
        )
    else:
        prefix += "$"
    cached = cache.get(id(node))
    buffer.write("{}{}".format(prefix, "=" if cached else "").strip() + "\n")

    if not cached:
        cache[id(node)] = node
        if node:
            for node in sorted(node.children.values(), key=lambda n: n.character):
                _node_to_string(node, prefix, buffer, cache)


def _dafsa_to_string(dafsa: Dafsa):
    """Encodes the dafsa into a string notation.

    Each node is printed on its own line with all the nodes that precede it.
    The end node is designated with the "$" character.
    If it joins into an existing node, the end of the line is adorned with a "=".
    Though this doesn't carry information about which other prefix it has joined with,
    it has seemed to be precise enough for testing.

    For example, with the input data of:
    * a1
    * ac1
    * bc1

    [root] --- a ---- 1 --- [end]
       |        |   /
        -- b -- c---

    The output will be:
    a
    a1
    a1$ <- end node was found
    ac
    ac1= <- joins with the "a1" prefix
    b
    bc= <- joins with the "ac" prefix
    """
    buffer = StringIO()
    cache = {}

    for node in sorted(dafsa.root_node.children.values(), key=lambda n: n.character):
        _node_to_string(node, "", buffer, cache)
    return buffer.getvalue().strip()


def _to_words(data):
    return [line.strip() for line in data.strip().split("\n")]


def _assert_dafsa(data, expected):
    words = _to_words(data)
    dafsa = Dafsa.from_tld_data(words)

    expected = expected.strip()
    expected = "\n".join([line.strip() for line in expected.split("\n")])
    as_string = _dafsa_to_string(dafsa)
    assert as_string == expected


class TestDafsa(unittest.TestCase):
    def test_1(self):
        _assert_dafsa(
            """
        a1
        ac1
        acc1
        bd1
        bc1
        bcc1
        """,
            """
        a
        a1
        a1$
        ac
        ac1=
        acc
        acc1=
        b
        bc=
        bd
        bd1=
        """,
        )

    def test_2(self):
        _assert_dafsa(
            """
        ab1
        b1
        bb1
        bbb1
        """,
            """
        a
        ab
        ab1
        ab1$
        b
        b1=
        bb
        bb1=
        bbb=
        """,
        )

    def test_3(self):
        _assert_dafsa(
            """
        a.ca1
        a.com1
        c.corg1
        b.ca1
        b.com1
        b.corg1
        """,
            """
        a
        a.
        a.c
        a.ca
        a.ca1
        a.ca1$
        a.co
        a.com
        a.com1=
        b
        b.
        b.c
        b.ca=
        b.co
        b.com=
        b.cor
        b.corg
        b.corg1=
        c
        c.
        c.c
        c.co
        c.cor=
        """,
        )

    def test_4(self):
        _assert_dafsa(
            """
        acom1
        bcomcom1
        acomcom1
        """,
            """
        a
        ac
        aco
        acom
        acom1
        acom1$
        acomc
        acomco
        acomcom
        acomcom1=
        b
        bc
        bco
        bcom
        bcomc=
        """,
        )

    def test_5(self):
        _assert_dafsa(
            """
        a.d1
        a.c.d1
        b.d1
        b.c.d1
        """,
            """
        a
        a.
        a.c
        a.c.
        a.c.d
        a.c.d1
        a.c.d1$
        a.d=
        b
        b.=
        """,
        )

    def test_6(self):
        _assert_dafsa(
            """
        a61
        a661
        b61
        b661
        """,
            """
        a
        a6
        a61
        a61$
        a66
        a661=
        b
        b6=
        """,
        )

    def test_7(self):
        _assert_dafsa(
            """
        a61
        a6661
        b61
        b6661
        """,
            """
        a
        a6
        a61
        a61$
        a66
        a666
        a6661=
        b
        b6=
        """,
        )

    def test_8(self):
        _assert_dafsa(
            """
        acc1
        bc1
        bccc1
        """,
            """
        a
        ac
        acc
        acc1
        acc1$
        b
        bc
        bc1=
        bcc=
        """,
        )

    def test_9(self):
        _assert_dafsa(
            """
        acc1
        bc1
        bcc1
        """,
            """
        a
        ac
        acc
        acc1
        acc1$
        b
        bc
        bc1=
        bcc=
        """,
        )

    def test_10(self):
        _assert_dafsa(
            """
        acc1
        cc1
        cccc1
        """,
            """
        a
        ac
        acc
        acc1
        acc1$
        c
        cc
        cc1=
        ccc=
        """,
        )

    def test_11(self):
        _assert_dafsa(
            """
        ac1
        acc1
        bc1
        bcc1
        """,
            """
        a
        ac
        ac1
        ac1$
        acc
        acc1=
        b
        bc=
        """,
        )

    def test_12(self):
        _assert_dafsa(
            """
        acd1
        bcd1
        bcdd1
        """,
            """
        a
        ac
        acd
        acd1
        acd1$
        b
        bc
        bcd
        bcd1=
        bcdd=
        """,
        )

    def test_13(self):
        _assert_dafsa(
            """
        ac1
        acc1
        bc1
        bcc1
        bccc1
        """,
            """
        a
        ac
        ac1
        ac1$
        acc
        acc1=
        b
        bc
        bc1=
        bcc=
        """,
        )

    def test_14(self):
        _assert_dafsa(
            """
        acc1
        acccc1
        bcc1
        bcccc1
        bcccccc1
        """,
            """
        a
        ac
        acc
        acc1
        acc1$
        accc
        acccc
        acccc1=
        b
        bc
        bcc
        bcc1=
        bccc=
        """,
        )

    def test_15(self):
        _assert_dafsa(
            """
        ac1
        bc1
        acac1
        """,
            """
        a
        ac
        ac1
        ac1$
        aca
        acac
        acac1=
        b
        bc=
        """,
        )

    def test_16(self):
        _assert_dafsa(
            """
        bat1
        t1
        tbat1
        """,
            """
        b
        ba
        bat
        bat1
        bat1$
        t
        t1=
        tb=
        """,
        )

    def test_17(self):
        _assert_dafsa(
            """
        acow1
        acat1
        t1
        tcat1
        acatcat1
        """,
            """
        a
        ac
        aca
        acat
        acat1
        acat1$
        acatc
        acatca
        acatcat
        acatcat1=
        aco
        acow
        acow1=
        t=
        """,
        )

    def test_18(self):
        _assert_dafsa(
            """
        bc1
        abc1
        abcxyzc1
        """,
            """
        a
        ab
        abc
        abc1
        abc1$
        abcx
        abcxy
        abcxyz
        abcxyzc
        abcxyzc1=
        b
        bc=
        """,
        )

    def test_19(self):
        _assert_dafsa(
            """
        a.z1
        a.y1
        c.z1
        d.z1
        d.y1
        """,
            """
        a
        a.
        a.y
        a.y1
        a.y1$
        a.z
        a.z1=
        c
        c.
        c.z=
        d
        d.=
        """,
        )

    def test_20(self):
        _assert_dafsa(
            """
        acz1
        acy1
        accz1
        acccz1
        bcz1
        bcy1
        bccz1
        bcccz1
        """,
            """
        a
        ac
        acc
        accc
        acccz
        acccz1
        acccz1$
        accz=
        acy
        acy1=
        acz=
        b
        bc=
        """,
        )


if __name__ == "__main__":
    mozunit.main()
