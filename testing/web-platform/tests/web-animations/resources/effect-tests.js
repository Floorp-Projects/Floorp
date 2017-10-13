// Common utility methods for testing animation effects

// Tests the |property| member of |animation's| target effect's computed timing
// at the various points indicated by |values|.
//
// |values| has the format:
//
//   {
//     before, // value to test during before phase
//     active, // value to test during at the very beginning of the active phase
//             // or undefined if the active duration is zero,
//     after,  // value to test during the after phase or undefined if the
//             // active duration is infinite
//   }
//
function assert_computed_timing_for_each_phase(animation, property, values) {
  const effect = animation.effect;

  // Before phase
  assert_equals(effect.getComputedTiming()[property], values.before,
                `Value of ${property} in the before phase`);

  // Active phase
  if (effect.getComputedTiming().activeDuration > 0) {
    animation.currentTime = effect.getComputedTiming().delay;
    assert_equals(effect.getComputedTiming()[property], values.active,
                  `Value of ${property} in the active phase`);
  } else {
    assert_equals(values.active, undefined,
                  'Test specifies a value to check during the active phase but'
                  + ' the animation has a zero duration');
  }

  // After phase
  if (effect.getComputedTiming().activeDuration !== Infinity) {
    animation.finish();
    assert_equals(effect.getComputedTiming()[property], values.after,
                  `Value of ${property} in the after phase`);
  } else {
    assert_equals(values.after, undefined,
                  'Test specifies a value to check during the after phase but'
                  + ' the animation has an infinite duration');
  }
}
