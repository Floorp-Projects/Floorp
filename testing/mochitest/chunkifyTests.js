/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function skipTests(tests, startTestPattern, endTestPattern) {
  var startIndex = 0, endIndex = tests.length - 1;
  for (var i = 0; i < tests.length; ++i) {
    var test_path;
    if ((tests[i] instanceof Object) && ("test" in tests[i])) {
      test_path = tests[i].test.url;
    } else if ((tests[i] instanceof Object) && ("url" in tests[i])) {
      test_path = tests[i].url;
    } else {
      test_path = tests[i];
    }
    if (startTestPattern && test_path.endsWith(startTestPattern)) {
      startIndex = i;
    }

    if (endTestPattern && test_path.endsWith(endTestPattern)) {
      endIndex = i;
    }
  }

  return tests.slice(startIndex, endIndex + 1);
}
