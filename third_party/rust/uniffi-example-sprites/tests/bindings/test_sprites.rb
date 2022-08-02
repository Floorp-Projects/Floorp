# frozen_string_literal: true

require 'test/unit'
require 'sprites'

include Test::Unit::Assertions
include Sprites

sempty = Sprite.new(nil)
assert_equal sempty.get_position, Point.new(0, 0)

s = Sprite.new(Point.new(0, 1))
assert_equal s.get_position, Point.new(0, 1)

s.move_to(Point.new(1, 2))
assert_equal s.get_position, Point.new(1, 2)

s.move_by(Vector.new(-4, 2))
assert_equal s.get_position, Point.new(-3, 4)

srel = Sprite.new_relative_to(Point.new(0, 1), Vector.new(1, 1.5))
assert_equal srel.get_position, Point.new(1, 2.5)
