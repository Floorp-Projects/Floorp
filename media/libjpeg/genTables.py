#!/usr/bin/python

import math

f = open("jpeg_nbits_table.h", "w")

for i in range(65536):
    f.write('%2d' % math.ceil(math.log(i + 1, 2)))
    if i != 65535:
        f.write(',')
    if (i + 1) % 16 == 0:
        f.write('\n')
    else:
        f.write(' ')

f.close()
