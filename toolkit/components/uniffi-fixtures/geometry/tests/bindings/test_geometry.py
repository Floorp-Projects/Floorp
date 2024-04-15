from geometry import Line, Point, gradient, intersection

ln1 = Line(start=Point(coord_x=0, coord_y=0), end=Point(coord_x=1, coord_y=2))
ln2 = Line(start=Point(coord_x=1, coord_y=1), end=Point(coord_x=2, coord_y=2))

assert gradient(ln1) == 2
assert gradient(ln2) == 1

assert intersection(ln1, ln2) == Point(coord_x=0, coord_y=0)
assert intersection(ln1, ln1) is None
