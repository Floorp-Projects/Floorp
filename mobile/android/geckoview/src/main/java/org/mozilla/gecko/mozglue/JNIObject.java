package org.mozilla.gecko.mozglue;

// Class that all classes with native methods extend from.
public abstract class JNIObject
{
    // Pointer to a WeakPtr object that refers to the native object.
    private long mHandle;

    // Dispose of any reference to a native object.
    //
    // If the native instance is destroyed from the native side, this should never be
    // called, so you should throw an UnsupportedOperationException. If instead you
    // want to destroy the native side from the Java end, make override this with
    // a native call, and the right thing will be done in the native code.
    protected abstract void disposeNative();
}
