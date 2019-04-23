/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.IGeckoEditableParent;
import org.mozilla.gecko.NativeQueue;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Handler;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.text.Editable;
import android.text.InputType;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillManager;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.util.Locale;

/**
 * {@code SessionTextInput} handles text input for {@code GeckoSession} through key events or input
 * methods. It is typically used to implement certain methods in {@link android.view.View}
 * such as {@link android.view.View#onCreateInputConnection}, by forwarding such calls to
 * corresponding methods in {@code SessionTextInput}.
 * <p>
 * For full functionality, {@code SessionTextInput} requires a {@link android.view.View} to be set
 * first through {@link #setView}. When a {@link android.view.View} is not set or set to null,
 * {@code SessionTextInput} will operate in a reduced functionality mode. See {@link
 * #onCreateInputConnection} and methods in {@link GeckoSession.TextInputDelegate} for changes in
 * behavior in this viewless mode.
 */
public final class SessionTextInput {
    /* package */ static final String LOGTAG = "GeckoSessionTextInput";
    private static final boolean DEBUG = false;

    // Interface to access GeckoInputConnection from SessionTextInput.
    /* package */ interface InputConnectionClient {
        View getView();
        Handler getHandler(Handler defHandler);
        InputConnection onCreateInputConnection(EditorInfo attrs);
    }

    // Interface to access GeckoEditable from GeckoInputConnection.
    /* package */ interface EditableClient {
        // The following value is used by requestCursorUpdates
        // ONE_SHOT calls updateCompositionRects() after getting current composing
        // character rects.
        @WrapForJNI final int ONE_SHOT = 1;
        // START_MONITOR start the monitor for composing character rects.  If is is
        // updaed,  call updateCompositionRects()
        @WrapForJNI final int START_MONITOR = 2;
        // ENDT_MONITOR stops the monitor for composing character rects.
        @WrapForJNI final int END_MONITOR = 3;

        void sendKeyEvent(@Nullable View view, int action, @NonNull KeyEvent event);
        Editable getEditable();
        void setBatchMode(boolean isBatchMode);
        Handler setInputConnectionHandler(@NonNull Handler handler);
        void postToInputConnection(@NonNull Runnable runnable);
        void requestCursorUpdates(int requestMode);
    }

    // Interface to access GeckoInputConnection from GeckoEditable.
    /* package */ interface EditableListener {
        // IME notification type for notifyIME(), corresponding to NotificationToIME enum.
        @WrapForJNI final int NOTIFY_IME_OF_TOKEN = -3;
        @WrapForJNI final int NOTIFY_IME_OPEN_VKB = -2;
        @WrapForJNI final int NOTIFY_IME_REPLY_EVENT = -1;
        @WrapForJNI final int NOTIFY_IME_OF_FOCUS = 1;
        @WrapForJNI final int NOTIFY_IME_OF_BLUR = 2;
        @WrapForJNI final int NOTIFY_IME_TO_COMMIT_COMPOSITION = 8;
        @WrapForJNI final int NOTIFY_IME_TO_CANCEL_COMPOSITION = 9;

        // IME enabled state for notifyIMEContext().
        final int IME_STATE_UNKNOWN = -1;
        final int IME_STATE_DISABLED = 0;
        final int IME_STATE_ENABLED = 1;
        final int IME_STATE_PASSWORD = 2;

        // Flags for notifyIMEContext().
        @WrapForJNI final int IME_FLAG_PRIVATE_BROWSING = 1;
        @WrapForJNI final int IME_FLAG_USER_ACTION = 2;

        void notifyIME(int type);
        void notifyIMEContext(int state, String typeHint, String modeHint,
                              String actionHint, int flag);
        void onSelectionChange();
        void onTextChange();
        void onDiscardComposition();
        void onDefaultKeyEvent(KeyEvent event);
        void updateCompositionRects(final RectF[] aRects);
    }

    private static final class DefaultDelegate implements GeckoSession.TextInputDelegate {
        public static final DefaultDelegate INSTANCE = new DefaultDelegate();

        private InputMethodManager getInputMethodManager(@Nullable final View view) {
            if (view == null) {
                return null;
            }
            return (InputMethodManager) view.getContext()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
        }

        @Override
        public void restartInput(@NonNull final GeckoSession session, final int reason) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();

            if (reason == RESTART_REASON_FOCUS) {
                final Context context = (view != null) ? view.getContext() : null;
                if ((context instanceof Activity) &&
                        !ActivityUtils.isFullScreen((Activity) context)) {
                    // Bug 1293463: show the toolbar to prevent spoofing.
                    session.getDynamicToolbarAnimator()
                           .showToolbar(/* immediately */ true);
                }
            }

            final InputMethodManager imm = getInputMethodManager(view);
            if (imm == null) {
                return;
            }

            // InputMethodManager has internal logic to detect if we are restarting input
            // in an already focused View, which is the case here because all content text
            // fields are inside one LayerView. When this happens, InputMethodManager will
            // tell the input method to soft reset instead of hard reset. Stock latin IME
            // on Android 4.2+ has a quirk that when it soft resets, it does not clear the
            // composition. The following workaround tricks the IME into clearing the
            // composition when soft resetting.
            if (InputMethods.needsSoftResetWorkaround(
                    InputMethods.getCurrentInputMethod(view.getContext()))) {
                // Fake a selection change, because the IME clears the composition when
                // the selection changes, even if soft-resetting. Offsets here must be
                // different from the previous selection offsets, and -1 seems to be a
                // reasonable, deterministic value
                imm.updateSelection(view, -1, -1, -1, -1);
            }

            try {
                imm.restartInput(view);
            } catch (RuntimeException e) {
                Log.e(LOGTAG, "Error restarting input", e);
            }
        }

        @Override
        public void showSoftInput(@NonNull final GeckoSession session) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                if (view.hasFocus() && !imm.isActive(view)) {
                    // Marshmallow workaround: The view has focus but it is not the active
                    // view for the input method. (Bug 1211848)
                    view.clearFocus();
                    view.requestFocus();
                }
                imm.showSoftInput(view, 0);
            }
        }

        @Override
        public void hideSoftInput(@NonNull final GeckoSession session) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        }

        @Override
        public void updateSelection(@NonNull final GeckoSession session,
                                    final int selStart, final int selEnd,
                                    final int compositionStart, final int compositionEnd) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                // When composition start and end is -1,
                // InputMethodManager.updateSelection will remove composition
                // on most IMEs. If not working, we have to add a workaround
                // to EditableListener.onDiscardComposition.
                imm.updateSelection(view, selStart, selEnd, compositionStart, compositionEnd);
            }
        }

        @Override
        public void updateExtractedText(@NonNull final GeckoSession session,
                                        @NonNull final ExtractedTextRequest request,
                                        @NonNull final ExtractedText text) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateExtractedText(view, request.token, text);
            }
        }

        @TargetApi(21)
        @Override
        public void updateCursorAnchorInfo(@NonNull final GeckoSession session,
                                           @NonNull final CursorAnchorInfo info) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            final InputMethodManager imm = getInputMethodManager(view);
            if (imm != null) {
                imm.updateCursorAnchorInfo(view, info);
            }
        }

        private Rect displayRectForId(@NonNull final GeckoSession session,
                                      @NonNull final int virtualId,
                                      @Nullable final View view) {
            final SessionTextInput textInput = session.getTextInput();
            Rect contentRect;
            if (textInput.mAutoFillNodes.indexOfKey(virtualId) >= 0) {
                GeckoBundle element = textInput.mAutoFillNodes
                        .get(virtualId);
                GeckoBundle bounds = element.getBundle("bounds");
                contentRect = new Rect(bounds.getInt("left"),
                        bounds.getInt("top"),
                        bounds.getInt("right"),
                        bounds.getInt("bottom"));

                final Matrix matrix = new Matrix();
                final RectF rectF = new RectF(contentRect);
                if (GeckoAppShell.isFennec()) {
                    session.getClientToScreenMatrix(matrix);
                } else {
                    session.getPageToScreenMatrix(matrix);
                }
                matrix.mapRect(rectF);
                rectF.roundOut(contentRect);
                if (DEBUG) {
                    Log.d(LOGTAG, "Displaying autofill rect at (" + contentRect.left + ", " +
                            contentRect.top + "), (" + contentRect.right + ", " +
                            contentRect.bottom + ")");
                }
            } else {
                contentRect = getDummyAutoFillRect(session, true, view);
            }

            return contentRect;
        }

        @Override
        public void notifyAutoFill(@NonNull final GeckoSession session,
                                   @GeckoSession.AutoFillNotification final int notification,
                                   final int virtualId) {
            ThreadUtils.assertOnUiThread();
            final View view = session.getTextInput().getView();
            if (Build.VERSION.SDK_INT < 26 || view == null) {
                return;
            }

            final AutofillManager manager =
                    view.getContext().getSystemService(AutofillManager.class);
            if (manager == null) {
                return;
            }

            switch (notification) {
                case AUTO_FILL_NOTIFY_STARTED:
                    // This line seems necessary for auto-fill to work on the initial page.
                    manager.cancel();
                    break;
                case AUTO_FILL_NOTIFY_COMMITTED:
                    manager.commit();
                    break;
                case AUTO_FILL_NOTIFY_CANCELED:
                    manager.cancel();
                    break;
                case AUTO_FILL_NOTIFY_VIEW_ENTERED:
                    manager.notifyViewEntered(view, virtualId, displayRectForId(session, virtualId, view));
                    break;
                case AUTO_FILL_NOTIFY_VIEW_EXITED:
                    manager.notifyViewExited(view, virtualId);
                    break;
            }
        }
    }

    private final GeckoSession mSession;
    private final NativeQueue mQueue;
    private final GeckoEditable mEditable;
    private InputConnectionClient mInputConnection;
    private GeckoSession.TextInputDelegate mDelegate;
    // Auto-fill nodes.
    private SparseArray<GeckoBundle> mAutoFillNodes;
    private SparseArray<EventCallback> mAutoFillRoots;
    private int mAutoFillFocusedId = View.NO_ID;
    private int mAutoFillFocusedRoot = View.NO_ID;

    /* package */ SessionTextInput(final @NonNull GeckoSession session,
                                   final @NonNull NativeQueue queue) {
        mSession = session;
        mQueue = queue;
        mEditable = new GeckoEditable(session);

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

    /* package */ void onWindowChanged(final GeckoSession.Window window) {
        if (mQueue.isReady()) {
            window.attachEditable(mEditable);
        } else {
            mQueue.queueUntilReady(window, "attachEditable",
                                   IGeckoEditableParent.class, mEditable);
        }
    }

    /**
     * Get a Handler for the background input method thread. In order to use a background
     * thread for input method operations on systems prior to Nougat, first override
     * {@code View.getHandler()} for the View returning the InputConnection instance, and
     * then call this method from the overridden method.
     *
     * For example:<pre>
     * &#64;Override
     * public Handler getHandler() {
     *     if (Build.VERSION.SDK_INT &gt;= 24) {
     *         return super.getHandler();
     *     }
     *     return getSession().getTextInput().getHandler(super.getHandler());
     * }</pre>
     *
     * @param defHandler Handler returned by the system {@code getHandler} implementation.
     * @return Handler to return to the system through {@code getHandler}.
     */
    @AnyThread
    public synchronized @NonNull Handler getHandler(final @NonNull Handler defHandler) {
        // May be called on any thread.
        if (mInputConnection != null) {
            return mInputConnection.getHandler(defHandler);
        }
        return defHandler;
    }

    /**
     * Get the current {@link android.view.View} for text input.
     *
     * @return Current text input View or null if not set.
     * @see #setView(View)
     */
    @UiThread
    public @Nullable View getView() {
        ThreadUtils.assertOnUiThread();
        return mInputConnection != null ? mInputConnection.getView() : null;
    }

    /**
     * Set the current {@link android.view.View} for text input. The {@link android.view.View}
     * is used to interact with the system input method manager and to display certain text input
     * UI elements. See the {@code SessionTextInput} class documentation for information on
     * viewless mode, when the current {@link android.view.View} is not set or set to null.
     *
     * @param view Text input View or null to clear current View.
     * @see #getView()
     */
    @UiThread
    public synchronized void setView(final @Nullable View view) {
        ThreadUtils.assertOnUiThread();

        if (view == null) {
            mInputConnection = null;
        } else if (mInputConnection == null || mInputConnection.getView() != view) {
            mInputConnection = GeckoInputConnection.create(mSession, view, mEditable);
        }
        mEditable.setListener((EditableListener) mInputConnection);
    }

    /**
     * Get an {@link android.view.inputmethod.InputConnection} instance. In viewless mode,
     * this method still fills out the {@link android.view.inputmethod.EditorInfo} object,
     * but the return value will always be null.
     *
     * @param attrs EditorInfo instance to be filled on return.
     * @return InputConnection instance, or null if there is no active input
     *         (or if in viewless mode).
     */
    @AnyThread
    public synchronized @Nullable InputConnection onCreateInputConnection(
            final @NonNull EditorInfo attrs) {
        // May be called on any thread.
        mEditable.onCreateInputConnection(attrs);

        if (!mQueue.isReady() || mInputConnection == null) {
            return null;
        }
        return mInputConnection.onCreateInputConnection(attrs);
    }

    /**
     * Process a KeyEvent as a pre-IME event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyPreIme(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyPreIme(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-down event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyDown(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyDown(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a key-up event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyUp(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyUp(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a long-press event.
     *
     * @param keyCode Key code.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyLongPress(final int keyCode, final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyLongPress(getView(), keyCode, event);
    }

    /**
     * Process a KeyEvent as a multiple-press event.
     *
     * @param keyCode Key code.
     * @param repeatCount Key repeat count.
     * @param event KeyEvent instance.
     * @return True if the event was handled.
     */
    @UiThread
    public boolean onKeyMultiple(final int keyCode, final int repeatCount,
                                 final @NonNull KeyEvent event) {
        ThreadUtils.assertOnUiThread();
        return mEditable.onKeyMultiple(getView(), keyCode, repeatCount, event);
    }

    /**
     * Set the current text input delegate.
     *
     * @param delegate TextInputDelegate instance or null to restore to default.
     */
    @UiThread
    public void setDelegate(@Nullable final GeckoSession.TextInputDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
    }

    /**
     * Get the current text input delegate.
     *
     * @return TextInputDelegate instance or a default instance if no delegate has been set.
     */
    @UiThread
    public @NonNull GeckoSession.TextInputDelegate getDelegate() {
        ThreadUtils.assertOnUiThread();
        if (mDelegate == null) {
            mDelegate = DefaultDelegate.INSTANCE;
        }
        return mDelegate;
    }

    /*package*/ void onScreenMetricsUpdated() {
        if (mAutoFillFocusedId != View.NO_ID) {
            getDelegate().notifyAutoFill(
                    mSession, GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ENTERED, mAutoFillFocusedId);
        }
    }

    /**
     * Fill the specified {@link ViewStructure} with auto-fill fields from the current page.
     *
     * @param structure Structure to be filled.
     * @param flags Zero or a combination of {@link View#AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
     *              AUTOFILL_FLAG_*} constants.
     */
    @TargetApi(23)
    @UiThread
    public void onProvideAutofillVirtualStructure(@NonNull final ViewStructure structure,
                                                  final int flags) {
        final View view = getView();
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

    /**
     * Perform auto-fill using the specified values.
     *
     * @param values Map of auto-fill IDs to values.
     */
    @UiThread
    public void autofill(final @NonNull SparseArray<CharSequence> values) {
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

    /* package */ void addAutoFill(@NonNull final GeckoBundle message,
                                   @NonNull final EventCallback callback) {
        if (Build.VERSION.SDK_INT < 23) {
            return;
        }

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

        if (initializing) {
            getDelegate().notifyAutoFill(
                    mSession, GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_STARTED, id);
        } else {
            getDelegate().notifyAutoFill(
                    mSession, GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_ADDED, id);
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

        getDelegate().notifyAutoFill(
                mSession, GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_CANCELED, View.NO_ID);
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
        if (mAutoFillFocusedId != View.NO_ID) {
            getDelegate().notifyAutoFill(
                    mSession, GeckoSession.TextInputDelegate.AUTO_FILL_NOTIFY_VIEW_EXITED,
                    mAutoFillFocusedId);
        }

        mAutoFillFocusedId = id;
        mAutoFillFocusedRoot = root;
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
}
