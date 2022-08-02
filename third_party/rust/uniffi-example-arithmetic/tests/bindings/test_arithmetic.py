from arithmetic import *

try:
    add(18446744073709551615, 1)
    assert(not("Should have thrown a IntegerOverflow exception!"))
except ArithmeticError.IntegerOverflow:
    # It's okay!
    pass

assert add(2, 4) == 6
assert add(4, 8) == 12

try:
    sub(0, 1)
    assert(not("Should have thrown a IntegerOverflow exception!"))
except ArithmeticError.IntegerOverflow:
    # It's okay!
    pass

assert sub(4, 2) == 2
assert sub(8, 4) == 4

assert div(8, 4) == 2

try:
    div(8, 0)
except InternalError:
    # It's okay!
    pass
else:
    assert(not("Should have panicked when dividing by zero"))

assert equal(2, 2)
assert equal(4, 4)

assert not equal(2, 4)
assert not equal(4, 8)
