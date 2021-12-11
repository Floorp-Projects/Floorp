# -*- coding: utf-8 -*-
#
#  Copyright 2011 Sybren A. Stüvel <sybren@stuvel.eu>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

'''Functions for parallel computation on multiple cores.

Introduced in Python-RSA 3.1.

.. note::

    Requires Python 2.6 or newer.

'''

from __future__ import print_function

import multiprocessing as mp

import rsa.prime
import rsa.randnum

def _find_prime(nbits, pipe):
    while True:
        integer = rsa.randnum.read_random_int(nbits)

        # Make sure it's odd
        integer |= 1

        # Test for primeness
        if rsa.prime.is_prime(integer):
            pipe.send(integer)
            return

def getprime(nbits, poolsize):
    '''Returns a prime number that can be stored in 'nbits' bits.

    Works in multiple threads at the same time.

    >>> p = getprime(128, 3)
    >>> rsa.prime.is_prime(p-1)
    False
    >>> rsa.prime.is_prime(p)
    True
    >>> rsa.prime.is_prime(p+1)
    False
    
    >>> from rsa import common
    >>> common.bit_size(p) == 128
    True
    
    '''

    (pipe_recv, pipe_send) = mp.Pipe(duplex=False)

    # Create processes
    procs = [mp.Process(target=_find_prime, args=(nbits, pipe_send))
             for _ in range(poolsize)]
    [p.start() for p in procs]

    result = pipe_recv.recv()

    [p.terminate() for p in procs]

    return result

__all__ = ['getprime']

    
if __name__ == '__main__':
    print('Running doctests 1000x or until failure')
    import doctest
    
    for count in range(100):
        (failures, tests) = doctest.testmod()
        if failures:
            break
        
        if count and count % 10 == 0:
            print('%i times' % count)
    
    print('Doctests done')

