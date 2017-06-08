
# --- ELLIPTIC CURVE MATH ------------------------------------------------------
#
#   curve definition:   y^2 = x^3 - p*x - q
#   over finite field:  Z/nZ* (prime residue classes modulo a prime number n)
#
#
#   COPYRIGHT (c) 2010 by Toni Mattis <solaris@live.de>
# ------------------------------------------------------------------------------

'''
Module for elliptic curve arithmetic over a prime field GF(n).
E(GF(n)) takes the form y**2 == x**3 - p*x - q (mod n) for a prime n.

0. Structures used by this module

    PARAMETERS and SCALARS are non-negative (long) integers.

    A POINT (x, y), usually denoted p1, p2, ...
    is a pair of (long) integers where 0 <= x < n and 0 <= y < n

    A POINT in PROJECTIVE COORDINATES, usually denoted jp1, jp2, ...
    takes the form (X, Y, Z, Z**2, Z**3) where x = X / Z**2
    and y = Y / z**3. This form is called Jacobian coordinates.

    The NEUTRAL element "0" or "O" is represented by None
    in both coordinate systems.

1. Basic Functions

    euclid()            Is the Extended Euclidean Algorithm.
    inv()               Computes the multiplicative inversion modulo n.
    curve_q()           Finds the curve parameter q (mod n)
                        when p and a point are given.
    element()           Tests whether a point (x, y) is on the curve.

2. Point transformations

    to_projective()     Converts a point (x, y) to projective coordinates.
    from_projective()   Converts a point from projective coordinates
                        to (x, y) using the transformation described above.
    neg()               Computes the inverse point -P in both coordinate
                        systems.

3. Slow point arithmetic

    These algorithms make use of basic geometry and modular arithmetic
    thus being suitable for small numbers and academic study.

    add()               Computes the sum of two (x, y)-points
    mul()               Perform scalar multiplication using "double & add"

4. Fast point arithmetic

    These algorithms make use of projective coordinates, signed binary
    expansion and a JSP-like approach (joint sparse form).

    The following functions consume and return projective coordinates:

    addf()              Optimized point addition.
    doublef()           Optimized point doubling.
    mulf()              Highly optimized scalar multiplication.
    muladdf()           Highly optimized addition of two products.
    
    The following functions use the optimized ones above but consume
    and output (x, y)-coordinates for a more convenient usage:

    mulp()              Encapsulates mulf()
    muladdp()           Encapsulates muladdf()

    For single additions add() is generally faster than an encapsulation of
    addf() which would involve expensive coordinate transformations.
    Hence there is no addp() and doublep().
'''

# BASIC MATH -------------------------------------------------------------------

def euclid(a, b):
    '''Solve x*a + y*b = ggt(a, b) and return (x, y, ggt(a, b))'''
    # Non-recursive approach hence suitable for large numbers
    x = yy = 0
    y = xx = 1
    while b:
        q = a // b
        a, b = b, a % b
        x, xx = xx - q * x, x
        y, yy = yy - q * y, y
    return xx, yy, a

def inv(a, n):
    '''Perform inversion 1/a modulo n. a and n should be COPRIME.'''
    # coprimality is not checked here in favour of performance
    i = euclid(a, n)[0]
    while i < 0:
        i += n
    return i

def curve_q(x, y, p, n):
    '''Find curve parameter q mod n having point (x, y) and parameter p'''
    return ((x * x - p) * x - y * y) % n

def element(point, p, q, n):
    '''Test, whether the given point is on the curve (p, q, n)'''
    if point:
        x, y = point
        return (x * x * x - p * x - q) % n == (y * y) % n
    else:
        return True

def to_projective(p):
    '''Transform point p given as (x, y) to projective coordinates'''
    if p:
        return (p[0], p[1], 1, 1, 1)
    else:
        return None     # Identity point (0)

def from_projective(jp, n):
    '''Transform a point from projective coordinates to (x, y) mod n'''
    if jp:
        return (jp[0] * inv(jp[3], n)) % n, (jp[1] * inv(jp[4], n)) % n
    else:
        return None     # Identity point (0)

def neg(p, n):
    '''Compute the inverse point to p in any coordinate system'''
    return (p[0], (n - p[1]) % n) + p[2:] if p else None


# POINT ADDITION ---------------------------------------------------------------

# addition of points in y**2 = x**3 - p*x - q over <Z/nZ*; +>
def add(p, q, n, p1, p2):
    '''Add points p1 and p2 over curve (p, q, n)'''
    if p1 and p2:
        x1, y1 = p1
        x2, y2 = p2
        if (x1 - x2) % n:
            s = ((y1 - y2) * inv(x1 - x2, n)) % n   # slope
            x = (s * s - x1 - x2) % n               # intersection with curve
            return (x, n - (y1 + s * (x - x1)) % n)
        else:
            if (y1 + y2) % n:       # slope s calculated by derivation
                s = ((3 * x1 * x1 - p) * inv(2 * y1, n)) % n
                x = (s * s - 2 * x1) % n            # intersection with curve
                return (x, n - (y1 + s * (x - x1)) % n)
            else:
                return None
    else:   # either p1 is not none -> ret. p1, otherwiese p2, which may be
        return p1 if p1 else p2     # none too.


# faster addition: redundancy in projective coordinates eliminates
# expensive inversions mod n.
def addf(p, q, n, jp1, jp2):
    '''Add jp1 and jp2 in projective (jacobian) coordinates.'''
    if jp1 and jp2:
        
        x1, y1, z1, z1s, z1c = jp1
        x2, y2, z2, z2s, z2c = jp2

        s1 = (y1 * z2c) % n
        s2 = (y2 * z1c) % n

        u1 = (x1 * z2s) % n
        u2 = (x2 * z1s) % n

        if (u1 - u2) % n:

            h = (u2 - u1) % n
            r = (s2 - s1) % n

            hs = (h * h) % n
            hc = (hs * h) % n

            x3 = (-hc - 2 * u1 * hs + r * r) % n
            y3 = (-s1 * hc + r * (u1 * hs - x3)) % n
            z3 = (z1 * z2 * h) % n
            
            z3s = (z3 * z3) % n
            z3c = (z3s * z3) % n
    
            return (x3, y3, z3, z3s, z3c)
        
        else:
            if (s1 + s2) % n:
                return doublef(p, q, n, jp1)
            else:
                return None
    else:
        return jp1 if jp1 else jp2

# explicit point doubling using redundant coordinates
def doublef(p, q, n, jp):
    '''Double jp in projective (jacobian) coordinates'''
    if not jp:
        return None
    x1, y1, z1, z1p2, z1p3 = jp
    
    y1p2 = (y1 * y1) % n
    a = (4 * x1 * y1p2) % n
    b = (3 * x1 * x1 - p * z1p3 * z1) % n
    x3 = (b * b - 2 * a) % n
    y3 = (b * (a - x3) - 8 * y1p2 * y1p2) % n
    z3 = (2 * y1 * z1) % n
    z3p2 = (z3 * z3) % n
    
    return x3, y3, z3, z3p2, (z3p2 * z3) % n


# SCALAR MULTIPLICATION --------------------------------------------------------

# scalar multiplication p1 * c = p1 + p1 + ... + p1 (c times) in O(log(n))
def mul(p, q, n, p1, c):
    '''multiply point p1 by scalar c over curve (p, q, n)'''
    res = None
    while c > 0:
        if c & 1:
            res = add(p, q, n, res, p1)
        c >>= 1                     # c = c / 2
        p1 = add(p, q, n, p1, p1)   # p1 = p1 * 2
    return res


# this method allows _signed_bin() to choose between 1 and -1. It will select
# the sign which leaves the higher number of zeroes in the binary
# representation (the higher GDB).
def _gbd(n):
    '''Compute second greatest base-2 divisor'''
    i = 1
    if n <= 0: return 0
    while not n % i:
        i <<= 1
    return i >> 2


# This method transforms n into a binary representation having signed bits.
# A signed binary expansion contains more zero-bits hence reducing the number
# of additions required by a multiplication algorithm.
#
# Example:  15 ( 0b1111 ) can be written as 16 - 1, resulting in (1,0,0,0,-1)
#           and saving 2 additions. Subtraction can be performed as
#           efficiently as addition.
def _signed_bin(n):
    '''Transform n into an optimized signed binary representation'''
    r = []
    while n > 1:
        if n & 1:
            cp = _gbd(n + 1) 
            cn = _gbd(n - 1)
            if cp > cn:         # -1 leaves more zeroes -> subtract -1 (= +1)
                r.append(-1)
                n += 1
            else:               # +1 leaves more zeroes -> subtract +1 (= -1)
                r.append(+1)
                n -= 1
        else:
            r.append(0)         # be glad about one more zero
        n >>= 1
    r.append(n)
    return r[::-1]


# This multiplication algorithm combines signed binary expansion and
# fast addition using projective coordinates resulting in 5 to 10 times
# faster multiplication.
def mulf(p, q, n, jp1, c):
    '''Multiply point jp1 by c in projective coordinates'''
    sb = _signed_bin(c)
    res = None
    jp0 = neg(jp1, n)  # additive inverse of jp1 to be used fot bit -1
    for s in sb:
        res = doublef(p, q, n, res)
        if s:
            res = addf(p, q, n, res, jp1) if s > 0 else \
                  addf(p, q, n, res, jp0)
    return res

# Encapsulates mulf() in order to enable flat coordinates (x, y)
def mulp(p, q, n, p1, c):
    '''Multiply point p by c using fast multiplication'''
    return from_projective(mulf(p, q, n, to_projective(p1), c), n)


# Sum of two products using Shamir's trick and signed binary expansion
def muladdf(p, q, n, jp1, c1, jp2, c2):
    '''Efficiently compute c1 * jp1 + c2 * jp2 in projective coordinates'''
    s1 = _signed_bin(c1)
    s2 = _signed_bin(c2)
    diff = len(s2) - len(s1)
    if diff > 0:
        s1 = [0] * diff + s1
    elif diff < 0:
        s2 = [0] * -diff + s2

    jp1p2 = addf(p, q, n, jp1, jp2)
    jp1n2 = addf(p, q, n, jp1, neg(jp2, n))

    precomp = ((None,           jp2,            neg(jp2, n)),
               (jp1,            jp1p2,          jp1n2),
               (neg(jp1, n),    neg(jp1n2, n),  neg(jp1p2, n)))
    res = None

    for i, j in zip(s1, s2):
        res = doublef(p, q, n, res)
        if i or j:
            res = addf(p, q, n, res, precomp[i][j])
    return res

# Encapsulate muladdf()
def muladdp(p, q, n, p1, c1, p2, c2):
    '''Efficiently compute c1 * p1 + c2 * p2 in (x, y)-coordinates'''
    return from_projective(muladdf(p, q, n,
                                   to_projective(p1), c1,
                                   to_projective(p2), c2), n)

# POINT COMPRESSION ------------------------------------------------------------

# Compute the square root modulo n


# Determine the sign-bit of a point allowing to reconstruct y-coordinates
# when x and the sign-bit are given:
def sign_bit(p1):
    '''Return the signedness of a point p1'''
    return p1[1] % 2 if p1 else 0

# Reconstruct the y-coordinate when curve parameters, x and the sign-bit of
# the y coordinate are given:
def y_from_x(x, p, q, n, sign):
    '''Return the y coordinate over curve (p, q, n) for given (x, sign)'''

    # optimized form of (x**3 - p*x - q) % n
    a = (((x * x) % n - p) * x - q) % n
    
    

if __name__ == "__main__":
    import rsa
    import time

    t = time.time()
    n = rsa.get_prime(256/8, 20)
    tp = time.time() - t
    p = rsa.random.randint(1, n)
    p1 = (rsa.random.randint(1, n), rsa.random.randint(1, n))
    q = curve_q(p1[0], p1[1], p, n)
    r1 = rsa.random.randint(1,n)
    r2 = rsa.random.randint(1,n)
    q1 = mulp(p, q, n, p1, r1)
    q2 = mulp(p, q, n, p1, r2)
    s1 = mulp(p, q, n, q1, r2)
    s2 = mulp(p, q, n, q2, r1)
    s1 == s2
    tt = time.time() - t

    def test(tcount, bits = 256):
        n = rsa.get_prime(bits/8, 20)
        p = rsa.random.randint(1, n)
        p1 = (rsa.random.randint(1, n), rsa.random.randint(1, n))
        q = curve_q(p1[0], p1[1], p, n)
        p2 = mulp(p, q, n, p1, rsa.random.randint(1, n))

        c1 = [rsa.random.randint(1, n) for i in xrange(tcount)]
        c2 = [rsa.random.randint(1, n) for i in xrange(tcount)]
        c = zip(c1, c2)

        t = time.time()
        for i, j in c:
            from_projective(addf(p, q, n,
                                 mulf(p, q, n, to_projective(p1), i),
                                 mulf(p, q, n, to_projective(p2), j)), n)
        t1 = time.time() - t
        t = time.time()
        for i, j in c:
            muladdp(p, q, n, p1, i, p2, j)
        t2 = time.time() - t

        return tcount, t1, t2
        

        
