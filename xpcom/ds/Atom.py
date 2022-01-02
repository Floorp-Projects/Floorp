# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class Atom:
    def __init__(self, ident, string, ty="nsStaticAtom"):
        self.ident = ident
        self.string = string
        self.ty = ty
        self.atom_type = self.__class__.__name__
        self.hash = hash_string(string)
        self.is_ascii_lowercase = is_ascii_lowercase(string)


class PseudoElementAtom(Atom):
    def __init__(self, ident, string):
        Atom.__init__(self, ident, string, ty="nsCSSPseudoElementStaticAtom")


class AnonBoxAtom(Atom):
    def __init__(self, ident, string):
        Atom.__init__(self, ident, string, ty="nsCSSAnonBoxPseudoStaticAtom")


class NonInheritingAnonBoxAtom(AnonBoxAtom):
    def __init__(self, ident, string):
        AnonBoxAtom.__init__(self, ident, string)


class InheritingAnonBoxAtom(AnonBoxAtom):
    def __init__(self, ident, string):
        AnonBoxAtom.__init__(self, ident, string)


GOLDEN_RATIO_U32 = 0x9E3779B9


def rotate_left_5(value):
    return ((value << 5) | (value >> 27)) & 0xFFFFFFFF


def wrapping_multiply(x, y):
    return (x * y) & 0xFFFFFFFF


# Calculate the precomputed hash of the static atom. This is a port of
# mozilla::HashString(const char16_t*), which is what we use for atomizing
# strings. An assertion in nsAtomTable::RegisterStaticAtoms ensures that
# the value we compute here matches what HashString() would produce.
def hash_string(s):
    h = 0
    for c in s:
        h = wrapping_multiply(GOLDEN_RATIO_U32, rotate_left_5(h) ^ ord(c))
    return h


# Returns true if lowercasing this string in an ascii-case-insensitive way
# would leave the string unchanged.
def is_ascii_lowercase(s):
    for c in s:
        if c >= "A" and c <= "Z":
            return False
    return True
