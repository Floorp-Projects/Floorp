package org.mozilla.gecko.delegates;

import android.os.Bundle;
import android.support.annotation.CallSuper;

import org.mozilla.gecko.BrowserApp;

import java.lang.ref.WeakReference;

/**
 * BrowserAppDelegate that stores a reference to the parent BrowserApp.
 */
public abstract class BrowserAppDelegateWithReference extends BrowserAppDelegate {
    private WeakReference<BrowserApp> browserApp;

    @Override
    @CallSuper
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        this.browserApp = new WeakReference<>(browserApp);
    }

    /**
     * Obtain the referenced BrowserApp. May return <code>null</code> if the BrowserApp no longer
     * exists.
     */
    protected BrowserApp getBrowserApp() {
        return browserApp.get();
    }
}
