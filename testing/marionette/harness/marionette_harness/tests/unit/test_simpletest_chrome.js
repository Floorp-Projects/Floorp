/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 1000;
MARIONETTE_CONTEXT = 'chrome';

is(2, 2, "test for is()");
isnot(2, 3, "test for isnot()");
ok(2 == 2, "test for ok()");
finish();

