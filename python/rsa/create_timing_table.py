#!/usr/bin/env python

import time
import rsa

poolsize = 8
accurate = True

def run_speed_test(bitsize):

    iterations = 0
    start = end = time.time()

    # At least a number of iterations, and at least 2 seconds
    while iterations < 10 or end - start < 2:
        iterations += 1
        rsa.newkeys(bitsize, accurate=accurate, poolsize=poolsize)
        end = time.time()

    duration = end - start
    dur_per_call = duration / iterations

    print '%5i bit: %9.3f sec. (%i iterations over %.1f seconds)' % (bitsize,
            dur_per_call, iterations, duration)

for bitsize in (128, 256, 384, 512, 1024, 2048, 3072, 4096):
    run_speed_test(bitsize)


