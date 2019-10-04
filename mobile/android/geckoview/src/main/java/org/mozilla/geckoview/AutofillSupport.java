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
    private SparseArray<GeckoBundle> mAutoFillNodes;
    private SparseArray<EventCallback> mAutoFillRoots;
    private int mAutoFillFocusedId = View.NO_ID;
    private int mAutoFillFocusedRoot = View.NO_ID;

    public AutofillSupport(final GeckoSession session) {
        mSession = session;

        session.getEventDispatcher().registerUiThreadListener(
                new BundleEventListener() {
                    @Override
                    public void handleMessage(final String event, final GeckoBundle message,
                                              final EventCallback callback) {
                        if ("GeckoView:AddAutoFill".equals(event)) {
                            addAutoFill(message, callback);
                        } else if ("GeckoView:ClearAutoFill".equals(event)) {
                            clearAutoFill();
                        } else if ("GeckoView:OnAutoFillFocus".equals(event)) {
                            onAutoFillFocus(message);
                        }
                    }
                },
                "GeckoView:AddAutoFill",
                "GeckoView:ClearAutoFill",
                "GeckoView:OnAutoFillFocus",
                null);
    }

    /**
     * Perform auto-fill using the specified values.
     *
     * @param values Map of auto-fill IDs to values.
     */
    public void autofill(final SparseArray<CharSequence> values) {
        if (mAutoFillRoots == null) {
            return;
        }

        GeckoBundle response = null;
        EventCallback callback = null;

        for (int i = 0; i < values.size(); i++) {
            final int id = values.keyAt(i);
            final CharSequence value = values.valueAt(i);

            if (DEBUG) {
                Log.d(LOGTAG, "performAutoFill(" + id + ')');
            }
            int rootId = id;
            for (int currentId = id; currentId != View.NO_ID; ) {
                final GeckoBundle bundle = mAutoFillNodes.get(currentId);
                if (bundle == null) {
                    return;
                }
                rootId = currentId;
                currentId = bundle.getInt("parent", View.NO_ID);
            }

            final EventCallback newCallback = mAutoFillRoots.get(rootId);
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

        final Rect rect = getDummyAutoFillRect(mSession, false, null);
        builder.dimensions(rect);

        if (mAutoFillRoots != null) {
            final int size = mAutoFillRoots.size();
            for (int i = 0; i < size; i++) {
                final int id = mAutoFillRoots.keyAt(i);
                final GeckoBundle root = mAutoFillNodes.get(id);
                fillAutofillElement(id, root, rect, builder.child());
            }
        }

        return builder.build();
    }

    private void fillAutofillElement(final int id, final GeckoBundle bundle, final Rect rect, final AutofillElement.Builder builder) {
        builder.id(id);
        builder.domain(bundle.getString("origin"));

        if (mAutoFillFocusedRoot != View.NO_ID && mAutoFillFocusedRoot == bundle.getInt("root", View.NO_ID)) {
            builder.dimensions(rect);
        }

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            for (final GeckoBundle childBundle : children) {
                final int childId = childBundle.getInt("id");
                fillAutofillElement(childId, childBundle, rect, builder.child());
                mAutoFillNodes.append(childId, childBundle);
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
                builder.focused(id == mAutoFillFocusedId);
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

    /* package */ void addAutoFill(@NonNull final GeckoBundle message,
                                   @NonNull final EventCallback callback) {
        final boolean initializing;
        if (mAutoFillRoots == null) {
            mAutoFillRoots = new SparseArray<>();
            mAutoFillNodes = new SparseArray<>();
            initializing = true;
        } else {
            initializing = false;
        }

        final int id = message.getInt("id");
        if (DEBUG) {
            Log.d(LOGTAG, "addAutoFill(" + id + ')');
        }
        mAutoFillRoots.append(id, callback);
        populateAutofillNodes(message);

        if (mDelegate == null) {
            return;
        }

        if (initializing) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_STARTED, id);
        } else {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_ADDED, id);
        }
    }

    private void populateAutofillNodes(final GeckoBundle bundle) {
        final int id = bundle.getInt("id");

        mAutoFillNodes.append(id, bundle);

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            for (GeckoBundle child : children) {
                populateAutofillNodes(child);
            }
        }
    }

    /* package */ void clearAutoFill() {
        if (mAutoFillRoots == null) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "clearAutoFill()");
        }
        mAutoFillRoots = null;
        mAutoFillNodes = null;
        mAutoFillFocusedId = View.NO_ID;
        mAutoFillFocusedRoot = View.NO_ID;

        if (mDelegate != null) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_CANCELED, View.NO_ID);
        }
    }

    /* package */ void onAutoFillFocus(@Nullable final GeckoBundle message) {
        if (mAutoFillRoots == null) {
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
            Log.d(LOGTAG, "onAutoFillFocus(" + id + ')');
        }
        if (mAutoFillFocusedId == id) {
            return;
        }

        if (mDelegate != null && mAutoFillFocusedId != View.NO_ID) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_EXITED,
                    mAutoFillFocusedId);
        }

        mAutoFillFocusedId = id;
        mAutoFillFocusedRoot = root;

        if (mDelegate != null && mAutoFillFocusedId != View.NO_ID) {
            mDelegate.onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED,
                    mAutoFillFocusedId);
        }
    }

    /* package */ static Rect getDummyAutoFillRect(@NonNull final GeckoSession session,
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
        if (mDelegate == null || mAutoFillFocusedId == View.NO_ID) {
            return;
        }

        // We blur/focus the active element (if we have one) when the document is made inactive/active.
        getDelegate().onAutofill(
                mSession,
                active ? GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED
                : GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_EXITED,
                mAutoFillFocusedId);
    }
}
