# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class Atom():
    def __init__(self, ident, string, ty="nsStaticAtom"):
        self.ident = ident
        self.string = string
        self.ty = ty
        self.atom_type = self.__class__.__name__


class PseudoElementAtom(Atom):
    def __init__(self, ident, string):
        Atom.__init__(self, ident, string, ty="nsICSSPseudoElement")


class AnonBoxAtom(Atom):
    def __init__(self, ident, string):
        Atom.__init__(self, ident, string, ty="nsICSSAnonBoxPseudo")


class NonInheritingAnonBoxAtom(AnonBoxAtom):
    def __init__(self, ident, string):
        AnonBoxAtom.__init__(self, ident, string)


class InheritingAnonBoxAtom(AnonBoxAtom):
    def __init__(self, ident, string):
        AnonBoxAtom.__init__(self, ident, string)
