/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

import android.os.Bundle;

/**
 * NativeJSObject is a wrapper around the SpiderMonkey JSAPI to make it possible to
 * access Javascript objects in Java.
 */
@WrapForJNI
public class NativeJSObject extends JNIObject
{
    @SuppressWarnings("serial")
    @JNITarget
    public static final class InvalidPropertyException extends RuntimeException {
        public InvalidPropertyException(final String msg) {
            super(msg);
        }
    }

    protected NativeJSObject() {
    }

    @Override
    protected void disposeNative() {
        // NativeJSObject is disposed as part of NativeJSContainer disposal.
    }

    /**
     * Returns the value of a boolean property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
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
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean optBoolean(String name, boolean fallback);

    /**
     * Returns the value of a boolean array property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean[] getBooleanArray(String name);

    /**
     * Returns the value of a boolean array property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean[] optBooleanArray(String name, boolean[] fallback);

    /**
     * Returns the value of an object property as a Bundle.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native Bundle getBundle(String name);

    /**
     * Returns the value of an object property as a Bundle.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native Bundle optBundle(String name, Bundle fallback);

    /**
     * Returns the value of an object array property as a Bundle array.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native Bundle[] getBundleArray(String name);

    /**
     * Returns the value of an object array property as a Bundle array.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native Bundle[] optBundleArray(String name, Bundle[] fallback);

    /**
     * Returns the value of a double property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
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
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native double optDouble(String name, double fallback);

    /**
     * Returns the value of a double array property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native double[] getDoubleArray(String name);

    /**
     * Returns the value of a double array property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native double[] optDoubleArray(String name, double[] fallback);

    /**
     * Returns the value of an int property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
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
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native int optInt(String name, int fallback);

    /**
     * Returns the value of an int array property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native int[] getIntArray(String name);

    /**
     * Returns the value of an int array property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native int[] optIntArray(String name, int[] fallback);

    /**
     * Returns the value of an object property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
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
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native NativeJSObject optObject(String name, NativeJSObject fallback);

    /**
     * Returns the value of an object array property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native NativeJSObject[] getObjectArray(String name);

    /**
     * Returns the value of an object array property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native NativeJSObject[] optObjectArray(String name, NativeJSObject[] fallback);

    /**
     * Returns the value of a string property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
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
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native String optString(String name, String fallback);

    /**
     * Returns the value of a string array property.
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property does not exist or if its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native String[] getStringArray(String name);

    /**
     * Returns the value of a string array property.
     *
     * @param name
     *        Property name
     * @param fallback
     *        Value to return if property does not exist
     * @throws IllegalArgumentException
     *         If name is null
     * @throws InvalidPropertyException
     *         If the property exists and its type does not match the return type
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native String[] optStringArray(String name, String[] fallback);

    /**
     * Returns whether a property exists in this object
     *
     * @param name
     *        Property name
     * @throws IllegalArgumentException
     *         If name is null
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native boolean has(String name);

    /**
     * Returns the Bundle representation of this object.
     *
     * @throws NullPointerException
     *         If this JS object has been disposed
     * @throws IllegalThreadStateException
     *         If not called on the thread this object is attached to
     * @throws UnsupportedOperationException
     *         If an internal JSAPI call failed
     */
    public native Bundle toBundle();

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
