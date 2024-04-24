#!/usr/bin/env sage

# This file recomputes the values in src/fp.rs for each FFT-friendly finite
# field.

import pprint


class Field:
    # The name of the field.
    name: str

    # The prime modulus that defines the field.
    modulus: Integer

    # A generator element that generates a large subgroup with an order that's
    # a power of two. This is _not_ in Montgomery representation.
    generator_element: Integer

    # The base 2 logarithm of the order of the FFT-friendly multiplicative
    # subgroup. The generator element will be a 2^num_roots-th root of unity.
    num_roots: Integer

    def __init__(self, name, modulus, generator_element):
        assert is_prime(modulus)
        self.name = name
        self.modulus = modulus
        self.generator_element = generator_element

        self.num_roots = None
        for (prime, power) in factor(modulus - 1):
            if prime == 2:
                self.num_roots = power
                break
        else:
            raise Exception(
                "Multiplicative subgroup order is not a multiple of two"
            )

    def mu(self):
        """
        Computes mu, a constant used during multiplication. It is defined by
        mu = (-p)^-1 mod r, where r is the modulus implicitly used in wrapping
        machine word operations.
        """
        r = 2 ^ 64
        return (-self.modulus).inverse_mod(r)

    def r2(self):
        """
        Computes R^2 mod p. This constant is used when converting into
        Montgomery representation. R is the machine word-friendly modulus
        used in the Montgomery representation.
        """
        R = 2 ^ 128
        return R ^ 2 % self.modulus

    def to_montgomery(self, element):
        """
        Transforms an element into its Montgomery representation.
        """
        R = 2 ^ 128
        return element * R % self.modulus

    def bit_mask(self):
        """
        An integer with the same bit length as the prime modulus, but with all
        bits set.
        """
        return 2 ^ (self.modulus.nbits()) - 1

    def roots(self):
        """
        Returns a list of roots of unity, in Montgomery representation. The
        value at index i is a 2^i-th root of unity. Note that the first array
        element will thus be the Montgomery representation of one.
        """
        return [
            self.to_montgomery(
                pow(
                    self.generator_element,
                    2 ^ (self.num_roots - i),
                    self.modulus,
                )
            )
            for i in range(min(self.num_roots, 20) + 1)
        ]


FIELDS = [
    Field(
        "FieldPrio2",
        2 ^ 20 * 4095 + 1,
        3925978153,
    ),
    Field(
        "Field64",
        2 ^ 32 * 4294967295 + 1,
        pow(7, 4294967295, 2 ^ 32 * 4294967295 + 1),
    ),
    Field(
        "Field128",
        2 ^ 66 * 4611686018427387897 + 1,
        pow(7, 4611686018427387897, 2 ^ 66 * 4611686018427387897 + 1),
    ),
]
for field in FIELDS:
    print(field.name)
    print(f"p: {field.modulus}")
    print(f"mu: {field.mu()}")
    print(f"r2: {field.r2()}")
    print(f"g: {field.to_montgomery(field.generator_element)}")
    print(f"num_roots: {field.num_roots}")
    print(f"bit_mask: {field.bit_mask()}")
    print("roots:")
    pprint.pprint(field.roots())
    print()
