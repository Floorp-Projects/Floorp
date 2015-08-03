'''
This module implements simple prime generation and primality testing.
'''

from random import SystemRandom
random = SystemRandom()
from os import urandom

def exp(x, n, m):
    '''Efficiently compute x ** n mod m'''
    y = 1
    z = x
    while n > 0:
        if n & 1:
            y = (y * z) % m
        z = (z * z) % m
        n //= 2
    return y


# Miller-Rabin-Test

def prime(n, k):
    '''Checks whether n is probably prime (with probability 1 - 4**(-k)'''
    
    if n % 2 == 0:
        return False
    
    d = n - 1
    s = 0
    
    while d % 2 == 0:
        s += 1
        d /= 2
        
    for i in xrange(k):
        
        a = long(2 + random.randint(0, n - 4))
        x = exp(a, d, n)
        if (x == 1) or (x == n - 1):
            continue
        
        for r in xrange(1, s):
            x = (x * x) % n
            
            if x == 1:
                return False
            
            if x == n - 1:
                break
            
        else:
            return False
    return True


# Generate and Test Algorithms

def get_prime(size, accuracy):
    '''Generate a pseudorandom prime number with the specified size (bytes).'''
    
    while 1:

        # read some random data from the operating system
        rstr = urandom(size - 1)
        r = 128 | ord(urandom(1))   # MSB = 1 (not less than size)
        for c in rstr:
            r = (r << 8) | ord(c)
        r |= 1                      # LSB = 1 (odd)

        # test whether this results in a prime number
        if prime(r, accuracy):
            return r


def get_prime_upto(n, accuracy):
    '''Find largest prime less than n'''
    n |= 1
    while n > 0:
        n -= 2
        if prime(n, accuracy):
            return n
