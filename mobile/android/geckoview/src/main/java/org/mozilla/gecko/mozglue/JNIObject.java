package org.mozilla.gecko.mozglue;

// Class that all classes with native methods extend from.
public abstract class JNIObject
{
    // Pointer to a WeakPtr object that refers to the native object.
    private long mHandle;

    // Dispose of any reference to a native object.
    protected abstract void disposeNative();
}
