/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

/**
 * Used by SynchronizerSession to indicate that reflow of a stage is necessary.
 * To reflow a stage is to request that it is synced again. Depending on the stage and its current
 * state (last-synced timestamp, resume context, high-water-mark) we might resume, or sync from a
 * high-water-mark if allowed, or sync regularly from last-synced timestamp.
 * A re-sync of a stage is no different from a regular sync of the same stage.
 *
 * Stages which complete only partially due to hitting a concurrent collection modification error or
 * hitting a sync deadline should be re-synced as soon as possible.
 *
 * @author grisha
 */
public class ReflowIsNecessaryException extends Exception {
    private static final long serialVersionUID = -2614772437814638768L;
}
