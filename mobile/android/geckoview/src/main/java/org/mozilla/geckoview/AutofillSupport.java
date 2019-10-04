package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.text.InputType;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;

import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.Locale;

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

    public void setDelegate(final GeckoSession.AutofillDelegate delegate) {
        mDelegate = delegate;
    }

    public GeckoSession.AutofillDelegate getDelegate() {
        return mDelegate;
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


    /**
     * Fill the specified {@link ViewStructure} with auto-fill fields from the current page.
     *
     * @param structure Structure to be filled.
     * @param flags     Zero or a combination of {@link View#AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
     *                  AUTOFILL_FLAG_*} constants.
     */
    @TargetApi(23)
    @UiThread
    public void provideAutofillVirtualStructure(@Nullable final View view,
                                                @NonNull final ViewStructure structure,
                                                final int flags) {
        if (view != null) {
            structure.setClassName(view.getClass().getName());
        }
        structure.setEnabled(true);
        structure.setVisibility(View.VISIBLE);

        final Rect rect = getDummyAutoFillRect(mSession, false, null);
        structure.setDimens(rect.left, rect.top, 0, 0, rect.width(), rect.height());

        if (mAutoFillRoots == null) {
            structure.setChildCount(0);
            return;
        }

        final int size = mAutoFillRoots.size();
        structure.setChildCount(size);

        for (int i = 0; i < size; i++) {
            final int id = mAutoFillRoots.keyAt(i);
            final GeckoBundle root = mAutoFillNodes.get(id);
            fillAutoFillStructure(view, id, root, structure.newChild(i), rect);
        }
    }

    @TargetApi(23)
    private void fillAutoFillStructure(@Nullable final View view, final int id,
                                       @NonNull final GeckoBundle bundle,
                                       @NonNull final ViewStructure structure,
                                       @NonNull final Rect rect) {
        if (mAutoFillRoots == null) {
            return;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "fillAutoFillStructure(" + id + ')');
        }

        if (Build.VERSION.SDK_INT >= 26) {
            if (view != null) {
                structure.setAutofillId(view.getAutofillId(), id);
            }
            structure.setWebDomain(bundle.getString("origin"));
        }
        structure.setId(id, null, null, null);

        if (mAutoFillFocusedRoot != View.NO_ID &&
                mAutoFillFocusedRoot == bundle.getInt("root", View.NO_ID)) {
            structure.setDimens(0, 0, 0, 0, rect.width(), rect.height());
        }

        final GeckoBundle[] children = bundle.getBundleArray("children");
        if (children != null) {
            structure.setChildCount(children.length);
            for (int i = 0; i < children.length; i++) {
                final GeckoBundle childBundle = children[i];
                final int childId = childBundle.getInt("id");
                final ViewStructure childStructure = structure.newChild(i);
                fillAutoFillStructure(view, childId, childBundle, childStructure, rect);
                mAutoFillNodes.append(childId, childBundle);
            }
        }

        String tag = bundle.getString("tag", "");
        final String type = bundle.getString("type", "text");

        if (Build.VERSION.SDK_INT >= 26) {
            final GeckoBundle attrs = bundle.getBundle("attributes");
            final ViewStructure.HtmlInfo.Builder builder =
                    structure.newHtmlInfoBuilder(tag.toLowerCase(Locale.US));
            for (final String key : attrs.keys()) {
                builder.addAttribute(key, String.valueOf(attrs.get(key)));
            }
            structure.setHtmlInfo(builder.build());
        }

        if ("INPUT".equals(tag) && !bundle.getBoolean("editable", false)) {
            tag = ""; // Don't process non-editable inputs (e.g. type="button").
        }
        switch (tag) {
            case "INPUT":
            case "TEXTAREA": {
                final boolean disabled = bundle.getBoolean("disabled");
                structure.setClassName("android.widget.EditText");
                structure.setEnabled(!disabled);
                structure.setFocusable(!disabled);
                structure.setFocused(id == mAutoFillFocusedId);
                structure.setVisibility(View.VISIBLE);

                if (Build.VERSION.SDK_INT >= 26) {
                    structure.setAutofillType(View.AUTOFILL_TYPE_TEXT);
                }
                break;
            }
            default:
                if (children != null) {
                    structure.setClassName("android.view.ViewGroup");
                } else {
                    structure.setClassName("android.view.View");
                }
                break;
        }

        if (Build.VERSION.SDK_INT >= 26 && "INPUT".equals(tag)) {
            // LastPass will fill password to the feild that setAutofillHints is unset and setInputType is set.
            switch (type) {
                case "email":
                    structure.setAutofillHints(new String[] { View.AUTOFILL_HINT_EMAIL_ADDRESS });
                    structure.setInputType(InputType.TYPE_CLASS_TEXT |
                                           InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS);
                    break;
                case "number":
                    structure.setInputType(InputType.TYPE_CLASS_NUMBER);
                    break;
                case "password":
                    structure.setAutofillHints(new String[] { View.AUTOFILL_HINT_PASSWORD });
                    structure.setInputType(InputType.TYPE_CLASS_TEXT |
                                           InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);
                    break;
                case "tel":
                    structure.setAutofillHints(new String[] { View.AUTOFILL_HINT_PHONE });
                    structure.setInputType(InputType.TYPE_CLASS_PHONE);
                    break;
                case "url":
                    structure.setInputType(InputType.TYPE_CLASS_TEXT |
                                           InputType.TYPE_TEXT_VARIATION_URI);
                    break;
                case "text":
                    final String autofillhint = bundle.getString("autofillhint", "");
                    if (autofillhint.equals("username")) {
                        structure.setAutofillHints(new String[] { View.AUTOFILL_HINT_USERNAME });
                        structure.setInputType(InputType.TYPE_CLASS_TEXT |
                                               InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT);
                    }
                    break;
            }
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

    public void onScreenMetricsUpdated() {
        if (mDelegate != null && mAutoFillFocusedId != View.NO_ID) {
            getDelegate().onAutofill(
                    mSession, GeckoSession.AutofillDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED, mAutoFillFocusedId);
        }
    }
}
