/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import custom_types
import Foundation

// TODO: use an actual test runner.

do {
    // Test simple values.
    var demo = getCustomTypesDemo(demo: nil)
    assert(demo.url == URL(string: "http://example.com/"))
    assert(demo.handle == 123)

    // Change some data and ensure that the round-trip works
    demo.url = URL(string: "http://new.example.com/")!
    demo.handle = 456
    assert(demo == getCustomTypesDemo(demo: demo))
}
