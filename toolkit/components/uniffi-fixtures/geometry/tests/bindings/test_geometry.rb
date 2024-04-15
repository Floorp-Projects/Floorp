# frozen_string_literal: true

require 'test/unit'
require 'geometry'

include Test::Unit::Assertions
include Geometry

ln1 = Line.new(start: Point.new(coord_x: 0.0, coord_y: 0.0), _end: Point.new(coord_x: 1.0, coord_y: 2.0))
ln2 = Line.new(start: Point.new(coord_x: 1.0, coord_y: 1.0), _end: Point.new(coord_x: 2.0, coord_y: 2.0))

assert_equal Geometry.gradient(ln1), 2
assert_equal Geometry.gradient(ln2), 1

assert_equal Geometry.intersection(ln1, ln2), Point.new(coord_x: 0, coord_y: 0)
assert Geometry.intersection(ln1, ln1).nil?
