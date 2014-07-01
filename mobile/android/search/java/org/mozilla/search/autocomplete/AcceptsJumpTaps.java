/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

/**
 * Allows rows to pass a "jump" event to the parent fragment.
 * <p/>
 * A jump event is when a user selects a suggestion, but they'd like to continue
 * searching. Right now, the UI uses an arrow that points up and to the left.
 */
interface AcceptsJumpTaps {
    public void onJumpTap(String suggestion);
}
