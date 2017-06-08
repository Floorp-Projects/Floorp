#
#   Predefined Elliptic Curves
#   for use in signing and key exchange
#
'''
Predefined elliptic curves for use in signing and key exchange.
This Module implements FIPS approved standard curves P-192, P-224, P-256,
P-384 and P-521 along with two weak non-standard curves of field size 128
and 160 bits.

The weak curves cannot be used for signing but provide a faster way to
obfuscate non-critical transmissions.
'''

# FIPS approved elliptic curves over prime fields 
# (see FIPS 186-3, Appendix D.1.2)
DOMAINS = {
    # Bits : (p, order of E(GF(P)), parameter b, base point x, base point y)
    192 : (0xfffffffffffffffffffffffffffffffeffffffffffffffffL,
           0xffffffffffffffffffffffff99def836146bc9b1b4d22831L,
           0x64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1L,
           0x188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012L,
           0x07192b95ffc8da78631011ed6b24cdd573f977a11e794811L),
    
    224 : (0xffffffffffffffffffffffffffffffff000000000000000000000001L,
           0xffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3dL,
           0xb4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4L,
           0xb70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21L,
           0xbd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34L),

    256 : (0xffffffff00000001000000000000000000000000ffffffffffffffffffffffffL,
           0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551L,
           0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604bL,
           0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296L,
           0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5L),

    384 : (0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffffL,
           0xffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973L,
           0xb3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aefL,
           0xaa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7L,
           0x3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5fL),

    521 : (0x1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffL,
           0x1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409L,
           0x051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00L,
           0x0c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66L,
           0x11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650L)
    }


# Additional non-standard curves for low security but high performance
# (not intended for use in signing, hence the missing group order)

DOMAINS.update({
    128 : (0xffffffffffffffffffffffffffffff61L,
           None,
           0xd83d3eb8266a89927d73d5fe263d5f23L,
           0xa94d2d8531f7af8bde367def12b98eadL,
           0x9f44e1d671beb68fd2df7f877ab13fa6L),
    
    160 : (0xffffffffffffffffffffffffffffffffffffffd1L,
           None,
           0x94bfe70deef7b94742c089ca4db3ca27fbe1f754L,
           0xcc6562c2969ac57524b8d0f300d1f598c908c121L,
           0x952ddde80a252683dd7ba90fb5919899b5af69f5L)
    })     

CURVE_P = 3     # global parameter of all curves (for efficiency reasons)

           
def get_curve(bits):
    '''Get a known curve of the given size => (bits, prime, order, p, q, point).
    Order may be None if unknown.'''
    if bits in DOMAINS:
        p, n, b, x, y = DOMAINS[bits]
        return bits, p, n, CURVE_P, p - b, (x, y)
    else:
        raise KeyError, "Key size not implemented: %s" % bits
 
def implemented_keys(must_sign = False):
    return [k for k in DOMAINS if not must_sign or DOMAINS[k][1]]
