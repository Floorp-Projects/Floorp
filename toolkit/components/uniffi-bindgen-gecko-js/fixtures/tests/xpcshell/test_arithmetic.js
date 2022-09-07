/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Arithmetic = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustArithmetic.jsm"
);

add_task(async function() {
  Assert.ok(Arithmetic.IntegerOverflow);
  Assert.equal(await Arithmetic.add(2, 4), 6);
  Assert.equal(await Arithmetic.add(4, 8), 12);
  // For other backends we would have this test:
  // await Assert.rejects(
  //     Arithmetic.add(18446744073709551615, 1),
  //     Arithmetic.IntegerOverflow,
  //     "add() should throw IntegerOverflow")
  //
  // However, this doesn't work because JS number values are actually 64-bit
  // floats, and that number is greater than the maximum "safe" integer.
  //
  // Instead, let's test that we reject numbers that are that big
  await Assert.rejects(
    Arithmetic.add(Number.MAX_SAFE_INTEGER + 1, 0),
    /TypeError/,
    "add() should throw TypeError when an input is > MAX_SAFE_INTEGER"
  );

  Assert.equal(await Arithmetic.sub(4, 2), 2);
  Assert.equal(await Arithmetic.sub(8, 4), 4);
  await Assert.rejects(
    Arithmetic.sub(0, 1),
    Arithmetic.IntegerOverflow,
    "sub() should throw IntegerOverflow"
  );

  Assert.equal(await Arithmetic.div(8, 4), 2);
  // Can't test this, because we don't allow Rust panics in FF
  // Assert.rejects(
  //     Arithmetic.div(8, 0),
  //     (e) => Assert.equal(e, Arithmetic.UniFFIInternalError),
  //     "Divide by 0 should throw UniFFIInternalError")
  //
  Assert.ok(await Arithmetic.equal(2, 2));
  Assert.ok(await Arithmetic.equal(4, 4));

  Assert.ok(!(await Arithmetic.equal(2, 4)));
  Assert.ok(!(await Arithmetic.equal(4, 8)));
});
