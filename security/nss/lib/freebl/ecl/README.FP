This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

The ECL exposes routines for constructing and converting curve
parameters for internal use.

The floating point code of the ECL provides algorithms for performing
elliptic-curve point multiplications in floating point.

The point multiplication algorithms perform calculations almost
exclusively in floating point for efficiency, but have the same
(integer) interface as the ECL for compatibility and to be easily
wired-in to the ECL.  Please see README file (not this README.FP file)
for information on wiring-in.

This has been implemented for 3 curves as specified in [1]:
	secp160r1
	secp192r1
	secp224r1

RATIONALE
=========
Calculations are done in the  floating-point unit (FPU) since it
gives better performance on the UltraSPARC III chips.  This is
because the FPU allows for faster multiplication than the integer unit.
The integer unit has a longer multiplication instruction latency, and
does not allow full pipelining, as described in [2].
Since performance is an important selling feature of Elliptic Curve
Cryptography (ECC), this implementation was created.

DATA REPRESENTATION
===================
Data is primarily represented in an array of double-precision floating
point numbers.  Generally, each array element has 24 bits of precision
(i.e. be x * 2^y, where x is an integer of at most 24 bits, y some positive
integer), although the actual implementation details are more complicated.

e.g. a way to store an 80 bit number might be:
double p[4] = { 632613 * 2^0, 329841 * 2^24, 9961 * 2^48, 51 * 2^64 };
See section ARITHMETIC OPERATIONS for more details.

This implementation assumes that the floating-point unit rounding mode 
is round-to-even as specified in IEEE 754
(as opposed to chopping, rounding up, or rounding down).
When subtracting integers represented as arrays of floating point 
numbers, some coefficients (array elements) may become negative.
This effectively gives an extra bit of precision that is important
for correctness in some cases.

The described number presentation limits the size of integers to 1023 bits.
This is due to an upper bound of 1024 for the exponent of a double precision
floating point number as specified in IEEE-754.
However, this is acceptable for ECC key sizes of the foreseeable future.

DATA STRUCTURES
===============
For more information on coordinate representations, see [3].

ecfp_aff_pt
-----------
Affine EC Point Representation.  This is the basic 
representation (x, y) of an elliptic curve point.

ecfp_jac_pt
-----------
Jacobian EC Point.  This stores a point as (X, Y, Z), where
the affine point corresponds to (X/Z^2, Y/Z^3).  This allows
for fewer inversions in calculations.

ecfp_chud_pt
------------
Chudnovsky Jacobian Point.  This representation stores a point
as (X, Y, Z, Z^2, Z^3), the same as a Jacobian representation
but also storing Z^2 and Z^3 for faster point additions.

ecfp_jm_pt
----------
Modified Jacobian Point.  This representation stores a point
as (X, Y, Z, a*Z^4), the same as Jacobian representation but
also storing a*Z^4 for faster point doublings.  Here "a" represents
the linear coefficient of x defining the curve.

EC_group_fp
-----------
Stores information on the elliptic curve group for floating
point calculations.  Contains curve specific information, as
well as function pointers to routines, allowing different
optimizations to be easily wired in.
This should be made accessible from an ECGroup for the floating
point implementations of point multiplication.

POINT MULTIPLICATION ALGORITHMS
===============================
Elliptic Curve Point multiplication can be done at a higher level orthogonal
to the implementation of point additions and point doublings.  There
are a variety of algorithms that can be used.

The following algorithms have been implemented:

4-bit Window (Jacobian Coordinates)
Double & Add (Jacobian & Affine Coordinates)
5-bit Non-Adjacent Form (Modified Jacobian & Chudnovsky Jacobian)

Currently, the fastest algorithm for multiplying a generic point
is the 5-bit Non-Adjacent Form.

See comments in ecp_fp.c for more details and references.

SOURCE / HEADER FILES
=====================

ecp_fp.c
--------
Main source file for floating point calculations.  Contains routines
to convert from floating-point to integer (mp_int format), point
multiplication algorithms, and several other routines.

ecp_fp.h
--------
Main header file.  Contains most constants used and function prototypes.

ecp_fp[160, 192, 224].c
-----------------------
Source files for specific curves.  Contains curve specific code such
as specialized reduction based on the field defining prime.  Contains
code wiring-in different algorithms and optimizations.

ecp_fpinc.c
-----------
Source file that is included by ecp_fp[160, 192, 224].c.  This generates
functions with different preprocessor-defined names and loop iterations,
allowing for static linking and strong compiler optimizations without
code duplication.

TESTING
=======
The test suite can be found in ecl/tests/ecp_fpt.  This tests and gets
timings of the different algorithms for the curves implemented.

ARITHMETIC OPERATIONS
---------------------
The primary operations in ECC over the prime fields are modular arithmetic:
i.e. n * m (mod p) and n + m (mod p).  In this implementation, multiplication,
addition, and reduction are implemented as separate functions.  This
enables computation of formulae with fewer reductions, e.g.
(a * b) + (c * d) (mod p) rather than:
((a * b) (mod p)) + ((c * d) (mod p)) (mod p)
This takes advantage of the fact that the double precision mantissa in
floating point can hold numbers up to 2^53, i.e. it has some leeway to
store larger intermediate numbers.  See further detail in the section on 
FLOATING POINT PRECISION.

Multiplication
--------------
Multiplication is implemented in a standard polynomial multiplication
fashion.  The terms in opposite factors are pairwise multiplied and
added together appropriately.  Note that the result requires twice 
as many doubles for storage, as the bit size of the product is twice
that of the multiplicands.
e.g. suppose we have double n[3], m[3], r[6], and want to calculate r = n * m
r[0] = n[0] * m[0]
r[1] = n[0] * m[1] + n[1] * m[0]
r[2] = n[0] * m[2] + n[1] * m[1] + n[2] * m[0]
r[3] = n[1] * m[2] + n[2] * m[1]
r[4] = n[2] * m[2]
r[5] = 0 (This is used later to hold spillover from r[4], see tidying in
the reduction section.)

Addition
--------
Addition is done term by term.  The only caveat is to be careful with
the number of terms that need to be added.  When adding results of
multiplication (before reduction), twice as many terms need to be added
together.  This is done in the addLong function.
e.g. for double n[4], m[4], r[4]:  r = n + m
r[0] = n[0] + m[0]
r[1] = n[1] + m[1]
r[2] = n[2] + m[2]
r[3] = n[3] + m[3]

Modular Reduction
-----------------
For the curves implemented, reduction is possible by fast reduction
for Generalized Mersenne Primes, as described in [4].  For the 
floating point implementation, a significant step of the reduction 
process is tidying:  that is, the propagation of carry bits from 
low-order to high-order coefficients to reduce the precision of each 
coefficient to 24 bits.
This is done by adding and then subtracting
ecfp_alpha, a large floating point number that induces precision roundoff.
See [5] for more details on tidying using floating point arithmetic.
e.g. suppose we have r = 961838 * 2^24 + 519308
then if we set alpha = 3 * 2^51 * 2^24,
FP(FP(r + alpha) - alpha) = 961838 * 2^24, because the precision for
the intermediate results is limited.  Our values of alpha are chosen
to truncate to a desired number of bits.

The reduction is then performed as in [4], adding multiples of prime p.
e.g. suppose we are working over a polynomial of 10^2.  Take the number
2 * 10^8 + 11 * 10^6 + 53 * 10^4 + 23 * 10^2 + 95, stored in 5 elements
for coefficients of 10^0, 10^2, ..., 10^8.
We wish to reduce modulo p = 10^6 - 2 * 10^4 + 1
We can subtract off from the higher terms
(2 * 10^8 + 11 * 10^6 + 53 * 10^4 + 23 * 10^2 + 95) - (2 * 10^2) * (10^6 - 2 * 10^4 + 1)
= 15 * 10^6 + 53 * 10^4 + 21 * 10^2 + 95
= 15 * 10^6 + 53 * 10^4 + 21 * 10^2 + 95 - (15) * (10^6 - 2 * 10^4 + 1)
= 83 * 10^4 + 21 * 10^2 + 80

Integrated Example
------------------
This example shows how multiplication, addition, tidying, and reduction
work together in our modular arithmetic.  This is simplified from the
actual implementation, but should convey the main concepts.  
Working over polynomials of 10^2 and with p as in the prior example,
Let a = 16 * 10^4 + 53 * 10^2 + 33
let b = 81 * 10^4 + 31 * 10^2 + 49
let c = 22 * 10^4 +  0 * 10^2 + 95
And suppose we want to compute a * b + c mod p.  
We first do a multiplication:  then a * b =
0 * 10^10 + 1296 * 10^8 + 4789 * 10^6 + 5100 * 10^4 + 3620 * 10^2 + 1617
Then we add in c before doing reduction, allowing us to get a * b + c =
0 * 10^10 + 1296 * 10^8 + 4789 * 10^6 + 5122 * 10^4 + 3620 * 10^2 + 1712
We then perform a tidying on the upper half of the terms:
0 * 10^10 + 1296 * 10^8 + 4789 * 10^6
0 * 10^10 + (1296 + 47) * 10^8 + 89 * 10^6
0 * 10^10 + 1343 * 10^8 + 89 * 10^6
13 * 10^10 + 43 * 10^8 + 89 * 10^6
which then gives us
13 * 10^10 + 43 * 10^8 + 89 * 10^6 + 5122 * 10^4 + 3620 * 10^2 + 1712
we then reduce modulo p similar to the reduction example above:
13 * 10^10 + 43 * 10^8 + 89 * 10^6 + 5122 * 10^4 + 3620 * 10^2 + 1712 
	- (13 * 10^4 * p)
69 * 10^8 + 89 * 10^6 + 5109 * 10^4 + 3620 * 10^2 + 1712
	- (69 * 10^2 * p)
227 * 10^6 + 5109 * 10^4 + 3551 * 10^2 + 1712
	- (227 * p)
5563 * 10^4 + 3551 * 10^2 + 1485
finally, we do tidying to get the precision of each term down to 2 digits
5563 * 10^4 + 3565 * 10^2 + 85
5598 * 10^4 + 65 * 10^2 + 85
55 * 10^6 + 98 * 10^4 + 65 * 10^2 + 85
and perform another reduction step
	- (55 * p)
208 * 10^4 + 65 * 10^2 + 30
There may be a small number of further reductions that could be done at
this point, but this is typically done only at the end when converting
from floating point  to an integer unit representation.

FLOATING POINT PRECISION
========================
This section discusses the precision of floating point numbers, which
one writing new formulae or a larger bit size should be aware of.  The
danger is that an intermediate result may be required to store a
mantissa larger than 53 bits, which would cause error by rounding off.

Note that the tidying with IEEE rounding mode set to round-to-even
allows negative numbers, which actually reduces the size of the double
mantissa to 23 bits - since it rounds the mantissa to the nearest number 
modulo 2^24, i.e. roughly between -2^23 and 2^23.
A multiplication increases the bit size to 2^46 * n, where n is the number
of doubles to store a number.  For the 224 bit curve, n = 10.  This gives 
doubles of size 5 * 2^47.  Adding two of these doubles gives a result
of size 5 * 2^48, which is less than 2^53, so this is safe.  
Similar analysis can be done for other formulae to ensure numbers remain
below 2^53.

Extended-Precision Floating Point
---------------------------------
Some platforms, notably x86 Linux, may use an extended-precision floating
point representation that has a 64-bit mantissa.  [6]  Although this
implementation is optimized for the IEEE standard 53-bit mantissa,
it should work with the 64-bit mantissa.  A check is done at run-time
in the function ec_set_fp_precision that detects if the precision is
greater than 53 bits, and runs code for the 64-bit mantissa accordingly.

REFERENCES
==========
[1] Certicom Corp., "SEC 2: Recommended Elliptic Curve Domain Parameters", Sept. 20, 2000.  www.secg.org
[2] Sun Microsystems Inc.  UltraSPARC III Cu User's Manual, Version 1.0, May 2002, Table 4.4
[3] H. Cohen, A. Miyaji, and T. Ono, "Efficient Elliptic Curve Exponentiation Using Mixed Coordinates".
[4] Henk C.A. van Tilborg, Generalized Mersenne Prime.  http://www.win.tue.nl/~henkvt/GenMersenne.pdf
[5] Daniel J. Bernstein, Floating-Point Arithmetic and Message Authentication, Journal of Cryptology, March 2000, Section 2.
[6] Daniel J. Bernstein, Floating-Point Arithmetic and Message Authentication, Journal of Cryptology, March 2000, Section 2 Notes.
