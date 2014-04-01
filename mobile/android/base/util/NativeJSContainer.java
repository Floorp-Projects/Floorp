/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.mozglue.JNITarget;

/**
 * NativeJSContainer is a wrapper around the SpiderMonkey JSAPI to make it possible to
 * access Javascript objects in Java.
 */
@JNITarget
public final class NativeJSContainer extends NativeJSObject
{
    private long mNativeObject;

    private NativeJSContainer(long nativeObject) {
        mNativeObject = nativeObject;
    }

    /**
     * Dispose all associated native objects. Subsequent use of any objects derived from
     * this container will throw a NullPointerException.
     */
    public native void dispose();
}
