/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNITarget;

/**
 * NativeJSContainer is a wrapper around the SpiderMonkey JSAPI to make it possible to
 * access Javascript objects in Java.
 *
 * A container must only be used on the thread it is attached to. To use it on another
 * thread, call {@link #clone()} to make a copy, and use the copy on the other thread.
 * When a copy is first used, it becomes attached to the thread using it.
 */
@WrapForJNI
public final class NativeJSContainer extends NativeJSObject
{
    private NativeJSContainer() {
    }

    /**
     * Make a copy of this container for use by another thread. When the copy is first used,
     * it becomes attached to the thread using it.
     */
    @Override
    public native NativeJSContainer clone();

    /**
     * Dispose all associated native objects. Subsequent use of any objects derived from
     * this container will throw a NullPointerException.
     */
    @Override
    public native void disposeNative();
}
