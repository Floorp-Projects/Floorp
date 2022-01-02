/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_USER_INTERACTION_ID = "testing.interaction";
const TEST_VALUE_1 = "some value";
const TEST_VALUE_2 = "some other value";
const TEST_INVALID_VALUE =
  "This is a value that is far too long - it has too many characters.";
const TEST_ADDITIONAL_TEXT_1 = "some additional text";
const TEST_ADDITIONAL_TEXT_2 = "some other additional text";

function run_test() {
  let obj1 = {};
  let obj2 = {};

  Assert.ok(UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1));
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj1)
  );
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj2)
  );

  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));

  // Unlike TelemetryStopwatch, we can clobber UserInteractions.
  Assert.ok(UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1));
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj1)
  );
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj2)
  );

  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));

  // Ensure that we can finish a UserInteraction that was accidentally started
  // twice
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID, obj2));

  // Make sure we can't start or finish non-existent UserInteractions.
  Assert.ok(!UserInteraction.start("non-existent.interaction", TEST_VALUE_1));
  Assert.ok(
    !UserInteraction.start("non-existent.interaction", TEST_VALUE_1, obj1)
  );
  Assert.ok(
    !UserInteraction.start("non-existent.interaction", TEST_VALUE_1, obj2)
  );
  Assert.ok(!UserInteraction.running("non-existent.interaction"));
  Assert.ok(!UserInteraction.running("non-existent.interaction", obj1));
  Assert.ok(!UserInteraction.running("non-existent.interaction", obj2));
  Assert.ok(!UserInteraction.finish("non-existent.interaction"));
  Assert.ok(!UserInteraction.finish("non-existent.interaction", obj1));
  Assert.ok(!UserInteraction.finish("non-existent.interaction", obj2));

  // Ensure that we enforce the character limit on value strings.
  Assert.ok(
    !UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_INVALID_VALUE)
  );
  Assert.ok(
    !UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_INVALID_VALUE, obj1)
  );
  Assert.ok(
    !UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_INVALID_VALUE, obj2)
  );
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));

  // Verify that they can be used again
  Assert.ok(UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2));
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj1)
  );
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj2)
  );
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID, obj2));

  Assert.ok(!UserInteraction.finish(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.finish(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(!UserInteraction.finish(TEST_USER_INTERACTION_ID, obj2));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));

  // Verify that they can be used again with different values.
  Assert.ok(UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1));
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_2, obj1)
  );
  Assert.ok(
    UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1, obj2)
  );
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj1));
  Assert.ok(UserInteraction.running(TEST_USER_INTERACTION_ID, obj2));
  Assert.ok(UserInteraction.finish(TEST_USER_INTERACTION_ID));
  Assert.ok(
    UserInteraction.finish(
      TEST_USER_INTERACTION_ID,
      obj1,
      TEST_ADDITIONAL_TEXT_1
    )
  );
  Assert.ok(
    UserInteraction.finish(
      TEST_USER_INTERACTION_ID,
      obj2,
      TEST_ADDITIONAL_TEXT_2
    )
  );

  // Test that they can be cancelled
  Assert.ok(UserInteraction.start(TEST_USER_INTERACTION_ID, TEST_VALUE_1));
  Assert.ok(UserInteraction.cancel(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.running(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.finish(TEST_USER_INTERACTION_ID));

  // Test that they cannot be cancelled twice
  Assert.ok(!UserInteraction.cancel(TEST_USER_INTERACTION_ID));
  Assert.ok(!UserInteraction.cancel(TEST_USER_INTERACTION_ID));
}
