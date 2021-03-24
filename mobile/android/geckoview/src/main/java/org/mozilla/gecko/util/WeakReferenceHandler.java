/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;

import java.lang.ref.WeakReference;

/**
 * A Handler to help prevent memory leaks when using Handlers as inner classes.
 *
 * To use, extend the Handler, if it's an inner class, make it static,
 * and reference `this` via the associated WeakReference.
 *
 * For additional context, see the "HandlerLeak" android lint item and this post by Romain Guy:
 *   https://groups.google.com/forum/#!msg/android-developers/1aPZXZG6kWk/lIYDavGYn5UJ
 */
public class WeakReferenceHandler<T> extends Handler {
    public final WeakReference<T> mTarget;

    public WeakReferenceHandler(final T that) {
        super();
        mTarget = new WeakReference<>(that);
    }
}
