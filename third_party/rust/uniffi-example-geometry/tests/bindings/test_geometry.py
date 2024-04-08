from geometry import *

ln1 = Line(Point(0,0), Point(1,2))
ln2 = Line(Point(1,1), Point(2,2))

assert gradient(ln1) == 2
assert gradient(ln2) == 1

assert intersection(ln1, ln2) == Point(0, 0)
assert intersection(ln1, ln1) is None
