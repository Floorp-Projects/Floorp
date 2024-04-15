import uniffi.geometry.*;

val ln1 = Line(Point(0.0,0.0), Point(1.0,2.0))
val ln2 = Line(Point(1.0,1.0), Point(2.0,2.0))

assert( gradient(ln1) == 2.0 )
assert( gradient(ln2) == 1.0 )

assert( intersection(ln1, ln2) == Point(0.0, 0.0) )
assert( intersection(ln1, ln1) == null )
