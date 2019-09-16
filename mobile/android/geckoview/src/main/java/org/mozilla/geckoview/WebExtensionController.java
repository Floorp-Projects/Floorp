package org.mozilla.geckoview;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

public class WebExtensionController {
    public interface TabDelegate {
        /**
         * Called when tabs.create is invoked, this method returns a *newly-created* session
         * that GeckoView will use to load the requested page on. If the returned value
         * is null the page will not be opened.
         *
         * @param source An instance of {@link WebExtension} or null if extension was not registered
         *               with GeckoRuntime.registerWebextension
         * @param uri The URI to be loaded. This is provided for informational purposes only,
         *            do not call {@link GeckoSession#loadUri} on it.
         * @return A {@link GeckoResult} which holds the returned GeckoSession. May be null, in
         *        which case the request for a new tab by the extension will fail.
         *        The implementation of onNewTab is responsible for maintaining a reference
         *        to the returned object, to prevent it from being garbage collected.
         */
        @UiThread
        @Nullable
        default GeckoResult<GeckoSession> onNewTab(@Nullable WebExtension source, @Nullable String uri) {
            return null;
        }
        /**
         * Called when tabs.remove is invoked, this method decides if WebExtension can close the
         * tab. In case WebExtension can close the tab, it should close passed GeckoSession and
         * return GeckoResult.ALLOW or GeckoResult.DENY in case tab cannot be closed.
         *
         * @param source An instance of {@link WebExtension} or null if extension was not registered
         *               with GeckoRuntime.registerWebextension
         * @param session An instance of {@link GeckoSession} to be closed.
         * @return GeckoResult.ALLOW if the tab will be closed, GeckoResult.DENY otherwise
         */
        @UiThread
        @NonNull
        default GeckoResult<AllowOrDeny> onCloseTab(@Nullable WebExtension source, @NonNull GeckoSession session)  {
            return GeckoResult.DENY;
        }
    }

    private GeckoRuntime mRuntime;
    private WebExtensionEventDispatcher mDispatcher;
    private TabDelegate mTabDelegate;
    private final EventListener mEventListener;

    protected WebExtensionController(final GeckoRuntime runtime, final WebExtensionEventDispatcher dispatcher) {
        mRuntime = runtime;
        mDispatcher = dispatcher;
        mEventListener = new EventListener();
    }

    private class EventListener implements BundleEventListener {
        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if ("GeckoView:WebExtension:NewTab".equals(event)) {
                newTab(message, callback);
                return;
            }
        }
    }

    @UiThread
    public void setTabDelegate(final @Nullable TabDelegate delegate) {
        if (delegate == null) {
            if (mTabDelegate != null) {
                EventDispatcher.getInstance().unregisterUiThreadListener(
                        mEventListener,
                        "GeckoView:WebExtension:NewTab"
                );
            }
        } else {
            if (mTabDelegate == null) {
                EventDispatcher.getInstance().registerUiThreadListener(
                        mEventListener,
                        "GeckoView:WebExtension:NewTab"
                );
            }
        }
        mTabDelegate = delegate;
    }

    @UiThread
    public @Nullable TabDelegate getTabDelegate() {
        return mTabDelegate;
    }

    private void newTab(final GeckoBundle message, final EventCallback callback) {
        if (mTabDelegate == null) {
            callback.sendSuccess(null);
            return;
        }

        WebExtension extension = mDispatcher.getWebExtension(message.getString("extensionId"));

        final GeckoResult<GeckoSession> result = mTabDelegate.onNewTab(extension, message.getString("uri"));

        if (result == null) {
            callback.sendSuccess(null);
            return;
        }

        result.accept(session -> {
            if (session == null) {
                callback.sendSuccess(null);
                return;
            }

            if (session.isOpen()) {
                throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
            }

            session.open(mRuntime);

            callback.sendSuccess(session.getId());
        });
    }

    /* package */ void closeTab(final GeckoBundle message, final EventCallback callback, final GeckoSession session) {
        if (mTabDelegate == null) {
            callback.sendError(null);
            return;
        }

        WebExtension extension = mRuntime.getWebExtensionDispatcher().getWebExtension(message.getString("extensionId"));

        GeckoResult<AllowOrDeny> result = mTabDelegate.onCloseTab(extension, session);

        result.accept(value -> {
            if (value == AllowOrDeny.ALLOW) {
                callback.sendSuccess(null);
            } else {
                callback.sendError(null);
            }
        });
    }
}
