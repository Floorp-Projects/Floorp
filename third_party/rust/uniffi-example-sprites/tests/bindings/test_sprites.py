from sprites import *

sempty = Sprite(None)
assert sempty.get_position() == Point(0, 0)

s = Sprite(Point(0, 1))
assert s.get_position() == Point(0, 1)

s.move_to(Point(1, 2))
assert s.get_position() == Point(1, 2)

s.move_by(Vector(-4, 2))
assert s.get_position() == Point(-3, 4)

srel = Sprite.new_relative_to(Point(0, 1), Vector(1, 1.5))
assert srel.get_position() == Point(1, 2.5)

