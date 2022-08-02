import arithmetic

do {
    let _ = try add(a: 18446744073709551615, b: 1)
    fatalError("Should have thrown a IntegerOverflow exception!")
} catch ArithmeticError.IntegerOverflow {
    // It's okay!
}

assert(try! add(a: 2, b: 4) == 6, "add work")
assert(try! add(a: 4, b: 8) == 12, "add work")

do {
    let _ = try sub(a: 0, b: 1)
    fatalError("Should have thrown a IntegerOverflow exception!")
} catch ArithmeticError.IntegerOverflow {
    // It's okay!
}

assert(try! sub(a: 4, b: 2) == 2, "sub work")
assert(try! sub(a: 8, b: 4) == 4, "sub work")

assert(div(dividend: 8, divisor: 4) == 2, "div works")

// We can't test panicking in Swift because we force unwrap the error in
// `div`, which we can't catch.

assert(equal(a: 2, b: 2), "equal works")
assert(equal(a: 4, b: 4), "equal works")

assert(!equal(a: 2, b: 4), "non-equal works")
assert(!equal(a: 4, b: 8), "non-equal works")
