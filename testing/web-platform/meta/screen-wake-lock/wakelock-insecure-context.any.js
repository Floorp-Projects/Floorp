/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[wakelock-type.https.any.html]
  disabled:
    if (os == "win" and debug and bits == "32"): https://bugzilla.mozilla.org/show_bug.cgi?id=1591125
