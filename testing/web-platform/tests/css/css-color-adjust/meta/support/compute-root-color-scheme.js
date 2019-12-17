'use strict';

function assert_root_color_scheme(expected) {
  test(() => {
    assert_equals(getComputedStyle(document.documentElement).colorScheme, expected);
  }, "Computed root color-scheme should be " + expected);
}
