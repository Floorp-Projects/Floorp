import uniffi.sprites.*;

val sempty = Sprite(null)
assert( sempty.getPosition() == Point(0.0, 0.0) )

val s = Sprite(Point(0.0, 1.0))
assert( s.getPosition() == Point(0.0, 1.0) )

s.moveTo(Point(1.0, 2.0))
assert( s.getPosition() == Point(1.0, 2.0) )

s.moveBy(Vector(-4.0, 2.0))
assert( s.getPosition() == Point(-3.0, 4.0) )

s.destroy()
try {
    s.moveBy(Vector(0.0, 0.0))
    assert(false) { "Should not be able to call anything after `destroy`" }
} catch(e: IllegalStateException) {
    assert(true)
}

val srel = Sprite.newRelativeTo(Point(0.0, 1.0), Vector(1.0, 1.5))
assert( srel.getPosition() == Point(1.0, 2.5) )

