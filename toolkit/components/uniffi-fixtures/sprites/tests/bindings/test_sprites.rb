# frozen_string_literal: true

require 'test/unit'
require 'sprites'

include Test::Unit::Assertions
include Sprites

sempty = Sprite.new(nil)
assert_equal sempty.get_position, Point.new(x: 0, y: 0)

s = Sprite.new(Point.new(x: 0, y: 1))
assert_equal s.get_position, Point.new(x: 0, y: 1)

s.move_to(Point.new(x: 1, y: 2))
assert_equal s.get_position, Point.new(x: 1, y: 2)

s.move_by(Vector.new(dx: -4, dy: 2))
assert_equal s.get_position, Point.new(x: -3, y: 4)

srel = Sprite.new_relative_to(Point.new(x: 0, y: 1), Vector.new(dx: 1, dy: 1.5))
assert_equal srel.get_position, Point.new(x: 1, y: 2.5)
