/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.mozglue.JNITarget;

/**
 * NativeJSObject is a wrapper around the SpiderMonkey JSAPI to make it possible to
 * access Javascript objects in Java.
 */
@JNITarget
public class NativeJSObject
{
    private final NativeJSContainer mContainer;
    private final int mObjectIndex;

    protected NativeJSObject() {
        mContainer = (NativeJSContainer)this;
        mObjectIndex = -1;
    }

    private NativeJSObject(NativeJSContainer container, int index) {
        mContainer = container;
        mObjectIndex = index;
    }

    /**
     * Returns the value of a boolean property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean getBoolean(String name);

    /**
     * Returns the value of a boolean property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean optBoolean(String name, boolean fallback);

    /**
     * Returns the value of a double property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native double getDouble(String name);

    /**
     * Returns the value of a double property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native double optDouble(String name, double fallback);

    /**
     * Returns the value of an int property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native int getInt(String name);

    /**
     * Returns the value of an int property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native int optInt(String name, int fallback);

    /**
     * Returns the value of an object property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native NativeJSObject getObject(String name);

    /**
     * Returns the value of an object property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native NativeJSObject optObject(String name, NativeJSObject fallback);

    /**
     * Returns the value of a string property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native String getString(String name);

    /**
     * Returns the value of a string property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native String optString(String name, String fallback);

    /**
     * Returns whether a property exists in this object
     *
     * @param name
     *        Property name
     * @throws NullPointerException
     *         If name is null or if this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean has(String name);

    /**
     * Returns the JSON representation of this object.
     *
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    @Override
    public native String toString();
}
