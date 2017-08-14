/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

/**
 * An "empty" session object that can be used instead of <code>null</code>.
 */
public class NullSession extends Session {
    public NullSession() {
        super(Source.NONE, "focusabout:");
    }
}
