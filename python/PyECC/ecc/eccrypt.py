#   Elliptic Curve Hybrid Encryption Scheme
#
#   COPYRIGHT (c) 2010 by Toni Mattis <solaris@live.de>
#

from curves import get_curve
from elliptic import mulp
from encoding import enc_long
from random import SystemRandom
from Rabbit import Rabbit

# important for cryptographically secure random numbers:
random = SystemRandom()

# Encryption Algorithm:
# ---------------------
# Input: Message M, public key Q
#
# 0. retrieve the group from which Q was generated.
# 1. generate random number k between 1 and the group order.
# 2. compute KG = k * G (where G is the base point of the group).
# 3. compute SG = k * Q (where Q is the public key of the receiver).
# 4. symmetrically encrypt M to M' using SG's x-coordinate as key.
#
# Return: Ciphertext M', temporary key KG


def encrypt(message, qk, encrypter = Rabbit):
    '''Encrypt a message using public key qk => (ciphertext, temp. pubkey)'''
    bits, q = qk
    try:
        bits, cn, n, cp, cq, g = get_curve(bits)
        if not n:
            raise ValueError, "Key size %s not suitable for encryption" % bits
    except KeyError:
        raise ValueError, "Key size %s not implemented" % bits
    
    k = random.randint(1, n - 1)        # temporary private key k
    kg = mulp(cp, cq, cn, g, k)         # temporary public key k*G
    sg = mulp(cp, cq, cn, q, k)         # shared secret k*Q = k*d*G

    return encrypter(enc_long(sg[0])).encrypt(message), kg

# Decryption Algorithm:
# ---------------------
# Input: Ciphertext M', temporary key KG, private key d
#
# 0. retrieve the group from which d and KG were generated.
# 1. compute SG = q * KG.
# 2. symmetrically decrypt M' to M using SG's x-coordinate as key.
#
# Return: M

def decrypt(message, kg, dk, decrypter = Rabbit):
    '''Decrypt a message using temp. public key kg and private key dk'''
    bits, d = dk
    try:
        bits, cn, n, cp, cq, g = get_curve(bits)
    except KeyError:
        raise ValueError, "Key size %s not implemented" % bits

    sg = mulp(cp, cq, cn, kg, d)        # shared secret d*(k*G) = k*d*G
    return decrypter(enc_long(sg[0])).decrypt(message)
    
    
