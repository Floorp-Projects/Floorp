# ------------------------------------------------------------------------------
#
#   SHA-512-BASED FEISTEL CIPHER
#   by Toni Mattis
#
#   Feistel Function:   SHA-512(Block || Key)
#   Key Size:           Fully Dynamic
#   Block Size:         1024 Bits
#   Rounds:             User-Specified
#
# ------------------------------------------------------------------------------

from hashlib import sha512

BPOS = tuple(range(64))

def enc_block(block, key, rounds = 16):
    x = block[:64]
    y = block[64:]
    for i in xrange(rounds):
        h = sha512(x + key).digest()
        y = ''.join([chr(ord(y[k]) ^ ord(h[k])) for k in BPOS])
        h = sha512(y + key).digest()
        x = ''.join([chr(ord(x[k]) ^ ord(h[k])) for k in BPOS])
    return x + y
        
def dec_block(block, key, rounds = 16):
    x = block[:64]
    y = block[64:]
    for i in xrange(rounds):
        h = sha512(y + key).digest()
        x = ''.join([chr(ord(x[k]) ^ ord(h[k])) for k in BPOS])
        h = sha512(x + key).digest()
        y = ''.join([chr(ord(y[k]) ^ ord(h[k])) for k in BPOS])
    return x + y


    
