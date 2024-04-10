from sprites import *

sempty = Sprite(None)
assert sempty.get_position() == Point(x=0, y=0)

s = Sprite(Point(x=0, y=1))
assert s.get_position() == Point(x=0, y=1)

s.move_to(Point(x=1, y=2))
assert s.get_position() == Point(x=1, y=2)

s.move_by(Vector(dx=-4, dy=2))
assert s.get_position() == Point(x=-3, y=4)

srel = Sprite.new_relative_to(Point(x=0, y=1), Vector(dx=1, dy=1.5))
assert srel.get_position() == Point(x=1, y=2.5)

