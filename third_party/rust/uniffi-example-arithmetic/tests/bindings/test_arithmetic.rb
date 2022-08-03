# frozen_string_literal: true

require 'test/unit'
require 'arithmetic'

include Test::Unit::Assertions

assert_raise Arithmetic::ArithmeticError::IntegerOverflow do
  Arithmetic.add 18_446_744_073_709_551_615, 1
end

assert_equal Arithmetic.add(2, 4), 6
assert_equal Arithmetic.add(4, 8), 12

assert_raise Arithmetic::ArithmeticError::IntegerOverflow do
  Arithmetic.sub 0, 1
end

assert_equal Arithmetic.sub(4, 2), 2
assert_equal Arithmetic.sub(8, 4), 4
assert_equal Arithmetic.div(8, 4), 2

assert_raise Arithmetic::InternalError do
  Arithmetic.div 8, 0
end

assert Arithmetic.equal(2, 2)
assert Arithmetic.equal(4, 4)

assert !Arithmetic.equal(2, 4)
assert !Arithmetic.equal(4, 8)
