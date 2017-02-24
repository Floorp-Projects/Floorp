/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

/**
 * Thrown when we've hit a self-imposed sync deadline, and decided not to proceed.
 *
 * @author grisha
 */
public class SyncDeadlineReachedException extends ReflowIsNecessaryException {
    private static final long serialVersionUID = 2305367921350245484L;
}
