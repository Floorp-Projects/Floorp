package org.mozilla.geckoview;

import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;

import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

/* package */ class AutofillSupport {
    private static final String LOGTAG = "AutofillSupport";
    private static final boolean DEBUG = false;

    private final GeckoSession mSession;
    private GeckoSession.AutofillDelegate mDelegate;
    private SparseArray<GeckoBundle> mAutofillNodes;
    private SparseArray<EventCallback> mAutofillRoots;
    private int mAutofillFocusedId = View.NO_ID;
    private int mAutofillFocusedRoot = View.NO_ID;

    public AutofillSupport(final GeckoSession session) {
        mSession = session;

        session.getEventDispatcher().registerUiThreadListener(
                new BundleEventListener() {
                    @Override
                    public void handleMessage(final String event, final GeckoBundle message,
                                              final EventCallback callback) {
                        if ("GeckoView:AddAutofill".equals(event)) {
                            addAutofill(message, callback);
                        } else if ("GeckoView:ClearAutofill".equals(event)) {
                            clearAutofill();
                        } else if ("GeckoView:OnAutofillFocus".equals(event)) {
                            onAutofillFocus(message);
                        }
                    }
                },
                "GeckoView:AddAutofill",
                "GeckoView:ClearAutofill",
                "GeckoView:OnAutofillFocus",
                null);
    }

    /**
     * Perform auto-fill using the specified values.
     *
     * @param values Map of auto-fill IDs to values.
     */
    public void autofill(final SparseArray<CharSequence> values) {
        if (mAutofillRoots == null) {
            return;
        }

        GeckoBundle response = null;
        EventCallback callback = null;

        for (int i = 0; i < values.size(); i++) {
            final int id = values.keyAt(i);
            final CharSequence value = values.valueAt(i);

            if (DEBUG) {
                Log.d(LOGTAG, "autofill(" + id + ')');
            }
            int rootId = id;
            for (int currentId = id; currentId != View.NO_ID; ) {
                final GeckoBundle bundle = mAutofillNodes.get(currentId);
                if (bundle == null) {
                    return;
                }
                rootId = currentId;
                currentId = bundle.getInt("parent", View.NO_ID);
            }

            final EventCallback newCallback = mAutofillRoots.get(rootId);
            if (callback == null || newCallback != callback) {
                if (callback != null) {
                    callback.sendSuccess(response);
                }
                response = new GeckoBundle(values.size() - i);
                callback = newCallback;
            }
            response.putString(String.valueOf(id), String.valueOf(value));
        }

        if (callback != null) {
            callback.sendSuccess(response);
        }
    }

    public void setDelegate(final @Nullable GeckoSession.AutofillDelegate delegate) {
        mDelegate = delegate;
    }

    public @Nullable GeckoSession.AutofillDelegate getDelegate() {
        return mDelegate;
    }

    public @NonNull AutofillElement getAutofillElements() {
        final AutofillElement.Builder builder = new AutofillElement.Builder();

        final Rect rect = getDummyAutofillRect(mSession, false, null);
        builder.dimensions(rect);

        if (mAutofillRoots != null) {
            final int size = mAutofillRoots.size();
            for (int i = 0; i < size; i++) {
                final int id = mAutofillRoots.keyAt(i);
                final GeckoBundle root = mAutofillNodes.get(id);
                fillAutofillElement(id, root, rect, builder.child());
            }
        }

        return builder.build();
    }

    private void fillAutofillElement(final int id, final GeckoBundle bundle, final Rect rect, final AutofillElement.Builder builder) {
        builder.id(id);
        builder.domain(bundle.getString("origin"));

        if (mAutofillFocusedRoot != View.NO_ID && mAutofillFocusedRoot == bundle.getInt("root", View.NO_ID)) {
            builder.dimensions(rect);
        }

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            for (final GeckoBundle childBundle : children) {
                final int childId = childBundle.getInt("id");
                fillAutofillElement(childId, childBundle, rect, builder.child());
                mAutofillNodes.append(childId, childBundle);
            }
        }

        String tag = bundle.getString("tag", "");
        builder.tag(tag.toLowerCase());

        final String type = bundle.getString("type", "text");

        final GeckoBundle attrs = bundle.getBundle("attributes");
        for (final String key : attrs.keys()) {
            builder.attribute(key, String.valueOf(attrs.get(key)));
        }

        if ("INPUT".equals(tag) && !bundle.getBoolean("editable", false)) {
            tag = ""; // Don't process non-editable inputs (e.g. type="button").
        }

        switch (tag) {
            case "INPUT":
            case "TEXTAREA": {
                final boolean disabled = bundle.getBoolean("disabled");

                builder.enabled(!disabled);
                builder.focusable(!disabled);
                builder.focused(id == mAutofillFocusedId);
                break;
            }
            default:
                break;
        }

        switch (type) {
            case "email":
                builder.hint(AutofillElement.HINT_EMAIL_ADDRESS);
                builder.inputType(AutofillElement.INPUT_TYPE_TEXT);
                break;
            case "number":
                builder.inputType(AutofillElement.INPUT_TYPE_NUMBER);
                break;
            case "password":
                builder.hint(AutofillElement.HINT_PASSWORD);
                builder.inputType(AutofillElement.INPUT_TYPE_TEXT);
                break;
            case "tel":
                builder.inputType(AutofillElement.INPUT_TYPE_PHONE);
                break;
            case "url":
                builder.hint(AutofillElement.HINT_URL);
                builder.inputType(AutofillElement.INPUT_TYPE_TEXT);
                break;
            case "text":
                final String autofillhint = bundle.getString("autofillhint", "");
                if (autofillhint.equals("username")) {
                    builder.hint(AutofillElement.HINT_USERNAME);
                    builder.inputType(AutofillElement.INPUT_TYPE_TEXT);
                }
                break;
        }
    }

    /* package */ void addAutofill(@NonNull final GeckoBundle message,
                                   @NonNull final EventCallback callback) {
        final boolean initializing;
        if (mAutofillRoots == null) {
            mAutofillRoots = new SparseArray<>();
            mAutofillNodes = new SparseArray<>();
            initializing = true;
        } else {
            initializing = false;
        }

        final int id = message.getInt("id");
        if (DEBUG) {
            Log.d(LOGTAG, "addAutofill(" + id + ')');
        }
        mAutofillRoots.append(id, callback);
        populateAutofillNodes(message);

        if (mDelegate == null) {
            return;
        }

        if (initializing) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_STARTED, id);
        } else {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ADDED, id);
        }
    }

    private void populateAutofillNodes(final GeckoBundle bundle) {
        final int id = bundle.getInt("id");

        mAutofillNodes.append(id, bundle);

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            for (GeckoBundle child : children) {
                populateAutofillNodes(child);
            }
        }
    }

    /* package */ void clearAutofill() {
        if (mAutofillRoots == null) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "clearAutofill()");
        }
        mAutofillRoots = null;
        mAutofillNodes = null;
        mAutofillFocusedId = View.NO_ID;
        mAutofillFocusedRoot = View.NO_ID;

        if (mDelegate != null) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_CANCELED, View.NO_ID);
        }
    }

    /* package */ void onAutofillFocus(@Nullable final GeckoBundle message) {
        if (mAutofillRoots == null) {
            return;
        }

        final int id;
        final int root;
        if (message != null) {
            id = message.getInt("id");
            root = message.getInt("root");
        } else {
            id = root = View.NO_ID;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "onAutofillFocus(" + id + ')');
        }
        if (mAutofillFocusedId == id) {
            return;
        }

        if (mDelegate != null && mAutofillFocusedId != View.NO_ID) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_EXITED,
                    mAutofillFocusedId);
        }

        mAutofillFocusedId = id;
        mAutofillFocusedRoot = root;

        if (mDelegate != null && mAutofillFocusedId != View.NO_ID) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED,
                    mAutofillFocusedId);
        }
    }

    /* package */ static Rect getDummyAutofillRect(@NonNull final GeckoSession session,
                                                   final boolean screen,
                                                   @Nullable final View view) {
        final Rect rect = new Rect();
        session.getSurfaceBounds(rect);
        if (screen) {
            if (view == null) {
                throw new IllegalArgumentException();
            }
            final int[] offset = new int[2];
            view.getLocationOnScreen(offset);
            rect.offset(offset[0], offset[1]);
        }
        return rect;
    }

    public void onActiveChanged(final boolean active) {
        if (mDelegate == null || mAutofillFocusedId == View.NO_ID) {
            return;
        }

        // We blur/focus the active element (if we have one) when the document is made inactive/active.
        getDelegate().onAutofill(
                mSession,
                active ? GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_ENTERED
                : GeckoSession.AutofillDelegate.AUTOFILL_NOTIFY_VIEW_EXITED,
                mAutofillFocusedId);
    }
}
