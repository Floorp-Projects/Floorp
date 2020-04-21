#! /usr/bin/env python
# -*- coding: utf-8 -*-
#
# Implementation of elliptic curves, for cryptographic applications.
#
# This module doesn't provide any way to choose a random elliptic
# curve, nor to verify that an elliptic curve was chosen randomly,
# because one can simply use NIST's standard curves.
#
# Notes from X9.62-1998 (draft):
#   Nomenclature:
#     - Q is a public key.
#     The "Elliptic Curve Domain Parameters" include:
#     - q is the "field size", which in our case equals p.
#     - p is a big prime.
#     - G is a point of prime order (5.1.1.1).
#     - n is the order of G (5.1.1.1).
#   Public-key validation (5.2.2):
#     - Verify that Q is not the point at infinity.
#     - Verify that X_Q and Y_Q are in [0,p-1].
#     - Verify that Q is on the curve.
#     - Verify that nQ is the point at infinity.
#   Signature generation (5.3):
#     - Pick random k from [1,n-1].
#   Signature checking (5.4.2):
#     - Verify that r and s are in [1,n-1].
#
# Version of 2008.11.25.
#
# Revision history:
#    2005.12.31 - Initial version.
#    2008.11.25 - Change CurveFp.is_on to contains_point.
#
# Written in 2005 by Peter Pearson and placed in the public domain.

from __future__ import division

try:
    from gmpy2 import mpz
    GMPY = True
except ImportError:
    try:
        from gmpy import mpz
        GMPY = True
    except ImportError:
        GMPY = False


from six import python_2_unicode_compatible
from . import numbertheory
from ._rwlock import RWLock


@python_2_unicode_compatible
class CurveFp(object):
  """Elliptic Curve over the field of integers modulo a prime."""

  if GMPY:
      def __init__(self, p, a, b, h=None):
        """
        The curve of points satisfying y^2 = x^3 + a*x + b (mod p).

        h is an integer that is the cofactor of the elliptic curve domain
        parameters; it is the number of points satisfying the elliptic curve
        equation divided by the order of the base point. It is used for selection
        of efficient algorithm for public point verification.
        """
        self.__p = mpz(p)
        self.__a = mpz(a)
        self.__b = mpz(b)
        # h is not used in calculations and it can be None, so don't use
        # gmpy with it
        self.__h = h
  else:
      def __init__(self, p, a, b, h=None):
        """
        The curve of points satisfying y^2 = x^3 + a*x + b (mod p).

        h is an integer that is the cofactor of the elliptic curve domain
        parameters; it is the number of points satisfying the elliptic curve
        equation divided by the order of the base point. It is used for selection
        of efficient algorithm for public point verification.
        """
        self.__p = p
        self.__a = a
        self.__b = b
        self.__h = h

  def __eq__(self, other):
    if isinstance(other, CurveFp):
      """Return True if the curves are identical, False otherwise."""
      return self.__p == other.__p \
          and self.__a == other.__a \
          and self.__b == other.__b
    return NotImplemented

  def __hash__(self):
    return hash((self.__p, self.__a, self.__b))

  def p(self):
    return self.__p

  def a(self):
    return self.__a

  def b(self):
    return self.__b

  def cofactor(self):
    return self.__h

  def contains_point(self, x, y):
    """Is the point (x,y) on this curve?"""
    return (y * y - ((x * x + self.__a) * x + self.__b)) % self.__p == 0

  def __str__(self):
    return "CurveFp(p=%d, a=%d, b=%d, h=%d)" % (
        self.__p, self.__a, self.__b, self.__h)


class PointJacobi(object):
  """
  Point on an elliptic curve. Uses Jacobi coordinates.

  In Jacobian coordinates, there are three parameters, X, Y and Z.
  They correspond to affine parameters 'x' and 'y' like so:

  x = X / Z²
  y = Y / Z³
  """
  def __init__(self, curve, x, y, z, order=None, generator=False):
      """
      Initialise a point that uses Jacobi representation internally.

      :param CurveFp curve: curve on which the point resides
      :param int x: the X parameter of Jacobi representation (equal to x when
        converting from affine coordinates
      :param int y: the Y parameter of Jacobi representation (equal to y when
        converting from affine coordinates
      :param int z: the Z parameter of Jacobi representation (equal to 1 when
        converting from affine coordinates
      :param int order: the point order, must be non zero when using
        generator=True
      :param bool generator: the point provided is a curve generator, as
        such, it will be commonly used with scalar multiplication. This will
        cause to precompute multiplication table for it
      """
      self.__curve = curve
      # since it's generally better (faster) to use scaled points vs unscaled
      # ones, use writer-biased RWLock for locking:
      self._scale_lock = RWLock()
      if GMPY:
          self.__x = mpz(x)
          self.__y = mpz(y)
          self.__z = mpz(z)
          self.__order = order and mpz(order)
      else:
          self.__x = x
          self.__y = y
          self.__z = z
          self.__order = order
      self.__precompute = []
      if generator:
          assert order
          i = 1
          order *= 2
          doubler = PointJacobi(curve, x, y, z, order)
          order *= 2
          self.__precompute.append((doubler.x(), doubler.y()))

          while i < order:
              i *= 2
              doubler = doubler.double().scale()
              self.__precompute.append((doubler.x(), doubler.y()))

  def __eq__(self, other):
      """Compare two points with each-other."""
      try:
          self._scale_lock.reader_acquire()
          if other is INFINITY:
              return not self.__y or not self.__z
          x1, y1, z1 = self.__x, self.__y, self.__z
      finally:
          self._scale_lock.reader_release()
      if isinstance(other, Point):
          x2, y2, z2 = other.x(), other.y(), 1
      elif isinstance(other, PointJacobi):
          try:
              other._scale_lock.reader_acquire()
              x2, y2, z2 = other.__x, other.__y, other.__z
          finally:
              other._scale_lock.reader_release()
      else:
          return NotImplemented
      if self.__curve != other.curve():
          return False
      p = self.__curve.p()

      zz1 = z1 * z1 % p
      zz2 = z2 * z2 % p

      # compare the fractions by bringing them to the same denominator
      # depend on short-circuit to save 4 multiplications in case of inequality
      return (x1 * zz2 - x2 * zz1) % p == 0 and \
             (y1 * zz2 * z2 - y2 * zz1 * z1) % p == 0

  def order(self):
      """Return the order of the point.

      None if it is undefined.
      """
      return self.__order

  def curve(self):
      """Return curve over which the point is defined."""
      return self.__curve

  def x(self):
      """
      Return affine x coordinate.

      This method should be used only when the 'y' coordinate is not needed.
      It's computationally more efficient to use `to_affine()` and then
      call x() and y() on the returned instance. Or call `scale()`
      and then x() and y() on the returned instance.
      """
      try:
          self._scale_lock.reader_acquire()
          if self.__z == 1:
              return self.__x
          x = self.__x
          z = self.__z
      finally:
          self._scale_lock.reader_release()
      p = self.__curve.p()
      z = numbertheory.inverse_mod(z, p)
      return x * z**2 % p

  def y(self):
      """
      Return affine y coordinate.

      This method should be used only when the 'x' coordinate is not needed.
      It's computationally more efficient to use `to_affine()` and then
      call x() and y() on the returned instance. Or call `scale()`
      and then x() and y() on the returned instance.
      """
      try:
          self._scale_lock.reader_acquire()
          if self.__z == 1:
              return self.__y
          y = self.__y
          z = self.__z
      finally:
          self._scale_lock.reader_release()
      p = self.__curve.p()
      z = numbertheory.inverse_mod(z, p)
      return y * z**3 % p

  def scale(self):
      """
      Return point scaled so that z == 1.

      Modifies point in place, returns self.
      """
      try:
          self._scale_lock.reader_acquire()
          if self.__z == 1:
              return self
      finally:
          self._scale_lock.reader_release()

      try:
          self._scale_lock.writer_acquire()
          # scaling already scaled point is safe (as inverse of 1 is 1) and
          # quick so we don't need to optimise for the unlikely event when
          # two threads hit the lock at the same time
          p = self.__curve.p()
          z_inv = numbertheory.inverse_mod(self.__z, p)
          zz_inv = z_inv * z_inv % p
          self.__x = self.__x * zz_inv % p
          self.__y = self.__y * zz_inv * z_inv % p
          # we are setting the z last so that the check above will return true
          # only after all values were already updated
          self.__z = 1
      finally:
          self._scale_lock.writer_release()
      return self

  def to_affine(self):
      """Return point in affine form."""
      if not self.__y or not self.__z:
          return INFINITY
      self.scale()
      # after point is scaled, it's immutable, so no need to perform locking
      return Point(self.__curve, self.__x,
                   self.__y, self.__order)

  @staticmethod
  def from_affine(point, generator=False):
      """Create from an affine point.

      :param bool generator: set to True to make the point to precalculate
        multiplication table - useful for public point when verifying many
        signatures (around 100 or so) or for generator points of a curve.
      """
      return PointJacobi(point.curve(), point.x(), point.y(), 1,
                         point.order(), generator)

  # plese note that all the methods that use the equations from hyperelliptic
  # are formatted in a way to maximise performance.
  # Things that make code faster: multiplying instead of taking to the power
  # (`xx = x * x; xxxx = xx * xx % p` is faster than `xxxx = x**4 % p` and
  # `pow(x, 4, p)`),
  # multiple assignments at the same time (`x1, x2 = self.x1, self.x2` is
  # faster than `x1 = self.x1; x2 = self.x2`),
  # similarly, sometimes the `% p` is skipped if it makes the calculation
  # faster and the result of calculation is later reduced modulo `p`

  def _double_with_z_1(self, X1, Y1, p, a):
      """Add a point to itself with z == 1."""
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#doubling-mdbl-2007-bl
      XX, YY = X1 * X1 % p, Y1 * Y1 % p
      if not YY:
          return 0, 0, 1
      YYYY = YY * YY % p
      S = 2 * ((X1 + YY)**2 - XX - YYYY) % p
      M = 3 * XX + a
      T = (M * M - 2 * S) % p
      # X3 = T
      Y3 = (M * (S - T) - 8 * YYYY) % p
      Z3 = 2 * Y1 % p
      return T, Y3, Z3

  def _double(self, X1, Y1, Z1, p, a):
      """Add a point to itself, arbitrary z."""
      if Z1 == 1:
          return self._double_with_z_1(X1, Y1, p, a)
      if not Z1:
          return 0, 0, 1
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#doubling-dbl-2007-bl
      XX, YY = X1 * X1 % p, Y1 * Y1 % p
      if not YY:
          return 0, 0, 1
      YYYY = YY * YY % p
      ZZ = Z1 * Z1 % p
      S = 2 * ((X1 + YY)**2 - XX - YYYY) % p
      M = (3 * XX + a * ZZ * ZZ) % p
      T = (M * M - 2 * S) % p
      # X3 = T
      Y3 = (M * (S - T) - 8 * YYYY) % p
      Z3 = ((Y1 + Z1)**2 - YY - ZZ) % p

      return T, Y3, Z3

  def double(self):
      """Add a point to itself."""
      if not self.__y:
          return INFINITY

      p, a = self.__curve.p(), self.__curve.a()

      try:
          self._scale_lock.reader_acquire()
          X1, Y1, Z1 = self.__x, self.__y, self.__z
      finally:
          self._scale_lock.reader_release()

      X3, Y3, Z3 = self._double(X1, Y1, Z1, p, a)

      if not Y3 or not Z3:
          return INFINITY
      return PointJacobi(self.__curve, X3, Y3, Z3, self.__order)

  def _add_with_z_1(self, X1, Y1, X2, Y2, p):
      """add points when both Z1 and Z2 equal 1"""
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-mmadd-2007-bl
      H = X2 - X1
      HH = H * H
      I = 4 * HH % p
      J = H * I
      r = 2 * (Y2 - Y1)
      if not H and not r:
          return self._double_with_z_1(X1, Y1, p, self.__curve.a())
      V = X1 * I
      X3 = (r**2 - J - 2 * V) % p
      Y3 = (r * (V - X3) - 2 * Y1 * J) % p
      Z3 = 2 * H % p
      return X3, Y3, Z3

  def _add_with_z_eq(self, X1, Y1, Z1, X2, Y2, p):
      """add points when Z1 == Z2"""
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-zadd-2007-m
      A = (X2 - X1)**2 % p
      B = X1 * A % p
      C = X2 * A
      D = (Y2 - Y1)**2 % p
      if not A and not D:
          return self._double(X1, Y1, Z1, p, self.__curve.a())
      X3 = (D - B - C) % p
      Y3 = ((Y2 - Y1) * (B - X3) - Y1 * (C - B)) % p
      Z3 = Z1 * (X2 - X1) % p
      return X3, Y3, Z3

  def _add_with_z2_1(self, X1, Y1, Z1, X2, Y2, p):
      """add points when Z2 == 1"""
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-madd-2007-bl
      Z1Z1 = Z1 * Z1 % p
      U2, S2 = X2 * Z1Z1 % p, Y2 * Z1 * Z1Z1 % p
      H = (U2 - X1) % p
      HH = H * H % p
      I = 4 * HH % p
      J = H * I
      r = 2 * (S2 - Y1) % p
      if not r and not H:
          return self._double_with_z_1(X2, Y2, p, self.__curve.a())
      V = X1 * I
      X3 = (r * r - J - 2 * V) % p
      Y3 = (r * (V - X3) - 2 * Y1 * J) % p
      Z3 = ((Z1 + H)**2 - Z1Z1 - HH) % p
      return X3, Y3, Z3

  def _add_with_z_ne(self, X1, Y1, Z1, X2, Y2, Z2, p):
      """add points with arbitrary z"""
      # after:
      # http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-add-2007-bl
      Z1Z1 = Z1 * Z1 % p
      Z2Z2 = Z2 * Z2 % p
      U1 = X1 * Z2Z2 % p
      U2 = X2 * Z1Z1 % p
      S1 = Y1 * Z2 * Z2Z2 % p
      S2 = Y2 * Z1 * Z1Z1 % p
      H = U2 - U1
      I = 4 * H * H % p
      J = H * I % p
      r = 2 * (S2 - S1) % p
      if not H and not r:
          return self._double(X1, Y1, Z1, p, self.__curve.a())
      V = U1 * I
      X3 = (r * r - J - 2 * V) % p
      Y3 = (r * (V - X3) - 2 * S1 * J) % p
      Z3 = ((Z1 + Z2)**2 - Z1Z1 - Z2Z2) * H % p

      return X3, Y3, Z3

  def __radd__(self, other):
      """Add other to self."""
      return self + other

  def _add(self, X1, Y1, Z1, X2, Y2, Z2, p):
      """add two points, select fastest method."""
      if not Y1 or not Z1:
          return X2, Y2, Z2
      if not Y2 or not Z2:
          return X1, Y1, Z1
      if Z1 == Z2:
          if Z1 == 1:
              return self._add_with_z_1(X1, Y1, X2, Y2, p)
          return self._add_with_z_eq(X1, Y1, Z1, X2, Y2, p)
      if Z1 == 1:
          return self._add_with_z2_1(X2, Y2, Z2, X1, Y1, p)
      if Z2 == 1:
          return self._add_with_z2_1(X1, Y1, Z1, X2, Y2, p)
      return self._add_with_z_ne(X1, Y1, Z1, X2, Y2, Z2, p)

  def __add__(self, other):
      """Add two points on elliptic curve."""
      if self == INFINITY:
          return other
      if other == INFINITY:
          return self
      if isinstance(other, Point):
          other = PointJacobi.from_affine(other)
      if self.__curve != other.__curve:
          raise ValueError("The other point is on different curve")

      p = self.__curve.p()
      try:
          self._scale_lock.reader_acquire()
          X1, Y1, Z1 = self.__x, self.__y, self.__z
      finally:
          self._scale_lock.reader_release()
      try:
          other._scale_lock.reader_acquire()
          X2, Y2, Z2 = other.__x, other.__y, other.__z
      finally:
          other._scale_lock.reader_release()
      X3, Y3, Z3 = self._add(X1, Y1, Z1, X2, Y2, Z2, p)

      if not Y3 or not Z3:
          return INFINITY
      return PointJacobi(self.__curve, X3, Y3, Z3, self.__order)

  def __rmul__(self, other):
      """Multiply point by an integer."""
      return self * other

  def _mul_precompute(self, other):
      """Multiply point by integer with precomputation table."""
      X3, Y3, Z3, p = 0, 0, 1, self.__curve.p()
      _add = self._add
      for X2, Y2 in self.__precompute:
          if other % 2:
              if other % 4 >= 2:
                  other = (other + 1)//2
                  X3, Y3, Z3 = _add(X3, Y3, Z3, X2, -Y2, 1, p)
              else:
                  other = (other - 1)//2
                  X3, Y3, Z3 = _add(X3, Y3, Z3, X2, Y2, 1, p)
          else:
              other //= 2

      if not Y3 or not Z3:
          return INFINITY
      return PointJacobi(self.__curve, X3, Y3, Z3, self.__order)

  @staticmethod
  def _naf(mult):
      """Calculate non-adjacent form of number."""
      ret = []
      while mult:
          if mult % 2:
              nd = mult % 4
              if nd >= 2:
                  nd = nd - 4
              ret += [nd]
              mult -= nd
          else:
              ret += [0]
          mult //= 2
      return ret

  def __mul__(self, other):
      """Multiply point by an integer."""
      if not self.__y or not other:
          return INFINITY
      if other == 1:
          return self
      if self.__order:
          # order*2 as a protection for Minerva
          other = other % (self.__order*2)
      if self.__precompute:
          return self._mul_precompute(other)

      self = self.scale()
      # once scaled, point is immutable, not need to lock
      X2, Y2 = self.__x, self.__y
      X3, Y3, Z3 = 0, 0, 1
      p, a = self.__curve.p(), self.__curve.a()
      _double = self._double
      _add = self._add
      # since adding points when at least one of them is scaled
      # is quicker, reverse the NAF order
      for i in reversed(self._naf(other)):
          X3, Y3, Z3 = _double(X3, Y3, Z3, p, a)
          if i < 0:
              X3, Y3, Z3 = _add(X3, Y3, Z3, X2, -Y2, 1, p)
          elif i > 0:
              X3, Y3, Z3 = _add(X3, Y3, Z3, X2, Y2, 1, p)

      if not Y3 or not Z3:
          return INFINITY

      return PointJacobi(self.__curve, X3, Y3, Z3, self.__order)

  @staticmethod
  def _leftmost_bit(x):
      """Return integer with the same magnitude as x but hamming weight of 1"""
      assert x > 0
      result = 1
      while result <= x:
        result = 2 * result
      return result // 2

  def mul_add(self, self_mul, other, other_mul):
      """
      Do two multiplications at the same time, add results.

      calculates self*self_mul + other*other_mul
      """
      if other is INFINITY or other_mul == 0:
          return self * self_mul
      if self_mul == 0:
          return other * other_mul
      if not isinstance(other, PointJacobi):
          other = PointJacobi.from_affine(other)
      # when the points have precomputed answers, then multiplying them alone
      # is faster (as it uses NAF)
      if self.__precompute and other.__precompute:
          return self * self_mul + other * other_mul

      if self.__order:
          self_mul = self_mul % self.__order
          other_mul = other_mul % self.__order

      i = self._leftmost_bit(max(self_mul, other_mul))*2
      X3, Y3, Z3 = 0, 0, 1
      p, a = self.__curve.p(), self.__curve.a()
      self = self.scale()
      # after scaling, point is immutable, no need for locking
      X1, Y1 = self.__x, self.__y
      other = other.scale()
      X2, Y2 = other.__x, other.__y
      both = (self + other).scale()
      X4, Y4 = both.__x, both.__y
      _double = self._double
      _add = self._add
      while i > 1:
          X3, Y3, Z3 = _double(X3, Y3, Z3, p, a)
          i = i // 2

          if self_mul & i and other_mul & i:
              X3, Y3, Z3 = _add(X3, Y3, Z3, X4, Y4, 1, p)
          elif self_mul & i:
              X3, Y3, Z3 = _add(X3, Y3, Z3, X1, Y1, 1, p)
          elif other_mul & i:
              X3, Y3, Z3 = _add(X3, Y3, Z3, X2, Y2, 1, p)

      if not Y3 or not Z3:
          return INFINITY

      return PointJacobi(self.__curve, X3, Y3, Z3, self.__order)

  def __neg__(self):
      """Return negated point."""
      try:
          self._scale_lock.reader_acquire()
          return PointJacobi(self.__curve, self.__x, -self.__y, self.__z,
                             self.__order)
      finally:
          self._scale_lock.reader_release()


class Point(object):
  """A point on an elliptic curve. Altering x and y is forbidding,
     but they can be read by the x() and y() methods."""
  def __init__(self, curve, x, y, order=None):
    """curve, x, y, order; order (optional) is the order of this point."""
    self.__curve = curve
    if GMPY:
        self.__x = x and mpz(x)
        self.__y = y and mpz(y)
        self.__order = order and mpz(order)
    else:
        self.__x = x
        self.__y = y
        self.__order = order
    # self.curve is allowed to be None only for INFINITY:
    if self.__curve:
      assert self.__curve.contains_point(x, y)
    # for curves with cofactor 1, all points that are on the curve are scalar
    # multiples of the base point, so performing multiplication is not
    # necessary to verify that. See Section 3.2.2.1 of SEC 1 v2
    if curve and curve.cofactor() != 1 and order:
      assert self * order == INFINITY

  def __eq__(self, other):
    """Return True if the points are identical, False otherwise."""
    if isinstance(other, Point):
      return self.__curve == other.__curve \
          and self.__x == other.__x \
          and self.__y == other.__y
    return NotImplemented

  def __neg__(self):
    return Point(self.__curve, self.__x, self.__curve.p() - self.__y)

  def __add__(self, other):
    """Add one point to another point."""

    # X9.62 B.3:

    if not isinstance(other, Point):
        return NotImplemented
    if other == INFINITY:
      return self
    if self == INFINITY:
      return other
    assert self.__curve == other.__curve
    if self.__x == other.__x:
      if (self.__y + other.__y) % self.__curve.p() == 0:
        return INFINITY
      else:
        return self.double()

    p = self.__curve.p()

    l = ((other.__y - self.__y) * \
         numbertheory.inverse_mod(other.__x - self.__x, p)) % p

    x3 = (l * l - self.__x - other.__x) % p
    y3 = (l * (self.__x - x3) - self.__y) % p

    return Point(self.__curve, x3, y3)

  def __mul__(self, other):
    """Multiply a point by an integer."""

    def leftmost_bit(x):
      assert x > 0
      result = 1
      while result <= x:
        result = 2 * result
      return result // 2

    e = other
    if e == 0 or (self.__order and e % self.__order == 0):
      return INFINITY
    if self == INFINITY:
      return INFINITY
    if e < 0:
      return (-self) * (-e)

    # From X9.62 D.3.2:

    e3 = 3 * e
    negative_self = Point(self.__curve, self.__x, -self.__y, self.__order)
    i = leftmost_bit(e3) // 2
    result = self
    # print_("Multiplying %s by %d (e3 = %d):" % (self, other, e3))
    while i > 1:
      result = result.double()
      if (e3 & i) != 0 and (e & i) == 0:
        result = result + self
      if (e3 & i) == 0 and (e & i) != 0:
        result = result + negative_self
      # print_(". . . i = %d, result = %s" % ( i, result ))
      i = i // 2

    return result

  def __rmul__(self, other):
    """Multiply a point by an integer."""

    return self * other

  def __str__(self):
    if self == INFINITY:
      return "infinity"
    return "(%d,%d)" % (self.__x, self.__y)

  def double(self):
    """Return a new point that is twice the old."""

    if self == INFINITY:
      return INFINITY

    # X9.62 B.3:

    p = self.__curve.p()
    a = self.__curve.a()

    l = ((3 * self.__x * self.__x + a) * \
         numbertheory.inverse_mod(2 * self.__y, p)) % p

    x3 = (l * l - 2 * self.__x) % p
    y3 = (l * (self.__x - x3) - self.__y) % p

    return Point(self.__curve, x3, y3)

  def x(self):
    return self.__x

  def y(self):
    return self.__y

  def curve(self):
    return self.__curve

  def order(self):
    return self.__order


# This one point is the Point At Infinity for all purposes:
INFINITY = Point(None, None, None)
