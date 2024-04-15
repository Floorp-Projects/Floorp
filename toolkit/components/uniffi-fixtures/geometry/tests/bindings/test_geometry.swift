import geometry

let ln1 = Line(start: Point(coordX: 0, coordY: 0), end: Point(coordX: 1, coordY: 2))
let ln2 = Line(start: Point(coordX: 1, coordY: 1), end: Point(coordX: 2, coordY: 2))

assert(gradient(ln: ln1) == 2.0)
assert(gradient(ln: ln2) == 1.0)

assert(intersection(ln1: ln1, ln2: ln2) == Point(coordX: 0, coordY: 0))
assert(intersection(ln1: ln1, ln2: ln1) == nil)
