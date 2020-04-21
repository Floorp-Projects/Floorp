from __future__ import division

from . import der, ecdsa
from .util import orderlen


# orderlen was defined in this module previously, so keep it in __all__,
# will need to mark it as deprecated later
__all__ = ["UnknownCurveError", "orderlen", "Curve", "NIST192p",
           "NIST224p", "NIST256p", "NIST384p", "NIST521p", "curves",
           "find_curve", "SECP256k1", "BRAINPOOLP160r1", "BRAINPOOLP192r1",
           "BRAINPOOLP224r1", "BRAINPOOLP256r1", "BRAINPOOLP320r1",
           "BRAINPOOLP384r1", "BRAINPOOLP512r1"]


class UnknownCurveError(Exception):
    pass


class Curve:
    def __init__(self, name, curve, generator, oid, openssl_name=None):
        self.name = name
        self.openssl_name = openssl_name  # maybe None
        self.curve = curve
        self.generator = generator
        self.order = generator.order()
        self.baselen = orderlen(self.order)
        self.verifying_key_length = 2*self.baselen
        self.signature_length = 2*self.baselen
        self.oid = oid
        self.encoded_oid = der.encode_oid(*oid)

    def __repr__(self):
        return self.name


# the NIST curves
NIST192p = Curve("NIST192p", ecdsa.curve_192,
                 ecdsa.generator_192,
                 (1, 2, 840, 10045, 3, 1, 1), "prime192v1")


NIST224p = Curve("NIST224p", ecdsa.curve_224,
                 ecdsa.generator_224,
                 (1, 3, 132, 0, 33), "secp224r1")


NIST256p = Curve("NIST256p", ecdsa.curve_256,
                 ecdsa.generator_256,
                 (1, 2, 840, 10045, 3, 1, 7), "prime256v1")


NIST384p = Curve("NIST384p", ecdsa.curve_384,
                 ecdsa.generator_384,
                 (1, 3, 132, 0, 34), "secp384r1")


NIST521p = Curve("NIST521p", ecdsa.curve_521,
                 ecdsa.generator_521,
                 (1, 3, 132, 0, 35), "secp521r1")


SECP256k1 = Curve("SECP256k1", ecdsa.curve_secp256k1,
                  ecdsa.generator_secp256k1,
                  (1, 3, 132, 0, 10), "secp256k1")


BRAINPOOLP160r1 = Curve("BRAINPOOLP160r1",
                        ecdsa.curve_brainpoolp160r1,
                        ecdsa.generator_brainpoolp160r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 1),
                        "brainpoolP160r1")


BRAINPOOLP192r1 = Curve("BRAINPOOLP192r1",
                        ecdsa.curve_brainpoolp192r1,
                        ecdsa.generator_brainpoolp192r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 3),
                        "brainpoolP192r1")


BRAINPOOLP224r1 = Curve("BRAINPOOLP224r1",
                        ecdsa.curve_brainpoolp224r1,
                        ecdsa.generator_brainpoolp224r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 5),
                        "brainpoolP224r1")


BRAINPOOLP256r1 = Curve("BRAINPOOLP256r1",
                        ecdsa.curve_brainpoolp256r1,
                        ecdsa.generator_brainpoolp256r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 7),
                        "brainpoolP256r1")


BRAINPOOLP320r1 = Curve("BRAINPOOLP320r1",
                        ecdsa.curve_brainpoolp320r1,
                        ecdsa.generator_brainpoolp320r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 9),
                        "brainpoolP320r1")


BRAINPOOLP384r1 = Curve("BRAINPOOLP384r1",
                        ecdsa.curve_brainpoolp384r1,
                        ecdsa.generator_brainpoolp384r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 11),
                        "brainpoolP384r1")


BRAINPOOLP512r1 = Curve("BRAINPOOLP512r1",
                        ecdsa.curve_brainpoolp512r1,
                        ecdsa.generator_brainpoolp512r1,
                        (1, 3, 36, 3, 3, 2, 8, 1, 1, 13),
                        "brainpoolP512r1")


curves = [NIST192p, NIST224p, NIST256p, NIST384p, NIST521p, SECP256k1,
          BRAINPOOLP160r1, BRAINPOOLP192r1, BRAINPOOLP224r1, BRAINPOOLP256r1,
          BRAINPOOLP320r1, BRAINPOOLP384r1, BRAINPOOLP512r1]


def find_curve(oid_curve):
    for c in curves:
        if c.oid == oid_curve:
            return c
    raise UnknownCurveError("I don't know about the curve with oid %s."
                            "I only know about these: %s" %
                            (oid_curve, [c.name for c in curves]))
