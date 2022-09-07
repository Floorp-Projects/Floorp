/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Arithmetic = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustArithmetic.jsm"
);
const Geometry = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustGeometry.jsm"
);

const { TodoList } = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustTodolist.jsm"
);
const { Stringifier } = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustRondpoint.jsm"
);

add_task(async function() {
  // Test our "type checking", which at this point is checking that
  // the correct number of arguments are passed and that pointer
  // arguments are of the correct type.

  await Assert.rejects(
    Arithmetic.add(2),
    /TypeError/,
    "add() call missing argument"
  );
  Assert.throws(
    () => Geometry.Point(0.0),
    /TypeError/,
    "Point constructor missing argument"
  );

  const todo = await TodoList.init();
  const stringifier = await Stringifier.init();
  await todo.getEntries(); // OK
  todo.ptr = stringifier.ptr;

  try {
    await todo.getEntries(); // the pointer is incorrect, should throw
    Assert.fail("Should have thrown the pointer was an incorrect pointer");
  } catch (e) {
    // OK
    // For some reason Assert.throws() can't seem to catch the "TypeError"s thrown
    // from C++
  }
});
