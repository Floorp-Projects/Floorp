/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;
import java.util.LinkedList;
import java.util.Locale;
import java.util.Map;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.collection.ArrayMap;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

public class Autofill {
    private static final boolean DEBUG = false;

    public static final class Notify {
        private Notify() {}

        /**
         * An autofill session has started.
         * Usually triggered by page load.
         */
        public static final int SESSION_STARTED = 0;

        /**
         * An autofill session has been committed.
         * Triggered by form submission or navigation.
         */
        public static final int SESSION_COMMITTED = 1;

        /**
         * An autofill session has been canceled.
         * Triggered by page unload.
         */
        public static final int SESSION_CANCELED = 2;

        /**
         * A node within the autofill session has been added.
         */
        public static final int NODE_ADDED = 3;

        /**
         * A node within the autofill session has been removed.
         */
        public static final int NODE_REMOVED = 4;

        /**
         * A node within the autofill session has been updated.
         */
        public static final int NODE_UPDATED = 5;

        /**
         * A node within the autofill session has gained focus.
         */
        public static final int NODE_FOCUSED = 6;

        /**
         * A node within the autofill session has lost focus.
         */
        public static final int NODE_BLURRED = 7;

        @AnyThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public static @Nullable String toString(
                final @AutofillNotify int notification) {
            final String[] map = new String[] {
                "SESSION_STARTED", "SESSION_COMMITTED", "SESSION_CANCELED",
                "NODE_ADDED", "NODE_REMOVED", "NODE_UPDATED", "NODE_FOCUSED",
                "NODE_BLURRED" };
            if (notification < 0 || notification >= map.length) {
                return null;
            }
            return map[notification];
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            Notify.SESSION_STARTED,
            Notify.SESSION_COMMITTED,
            Notify.SESSION_CANCELED,
            Notify.NODE_ADDED,
            Notify.NODE_REMOVED,
            Notify.NODE_UPDATED,
            Notify.NODE_FOCUSED,
            Notify.NODE_BLURRED})
    /* package */ @interface AutofillNotify {}

    public static final class Hint {
        private Hint() {}

        /**
         * Hint indicating that no special handling is required.
         */
        public static final int NONE = -1;

        /**
         * Hint indicating that a node represents an email address.
         */
        public static final int EMAIL_ADDRESS = 0;

        /**
         * Hint indicating that a node represents a password.
         */
        public static final int PASSWORD = 1;

        /**
         * Hint indicating that a node represents an URI.
         */
        public static final int URI = 2;

        /**
         * Hint indicating that a node represents a username.
         */
        public static final int USERNAME = 3;

        @AnyThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public static @Nullable String toString(final @AutofillHint int hint) {
            final int idx = hint + 1;
            final String[] map = new String[] {
                "NONE", "EMAIL", "PASSWORD", "URI", "USERNAME" };

            if (idx < 0 || idx >= map.length) {
                return null;
            }
            return map[idx];
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ Hint.NONE, Hint.EMAIL_ADDRESS, Hint.PASSWORD, Hint.URI,
              Hint.USERNAME })
    /* package */ @interface AutofillHint {}

    public static final class InputType {
        private InputType() {}

        /**
         * Indicates that a node is not a known input type.
         */
        public static final int NONE = -1;

        /**
         * Indicates that a node is a text input type.
         * Example: {@code <input type="text">}
         */
        public static final int TEXT = 0;

        /**
         * Indicates that a node is a number input type.
         * Example: {@code <input type="number">}
         */
        public static final int NUMBER = 1;

        /**
         * Indicates that a node is a phone input type.
         * Example: {@code <input type="tel">}
         */
        public static final int PHONE = 2;

        @AnyThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public static @Nullable String toString(
                final @AutofillInputType int type) {
            final int idx = type + 1;
            final String[] map = new String[] {
                "NONE", "TEXT", "NUMBER", "PHONE" };

            if (idx < 0 || idx >= map.length) {
                return null;
            }
            return map[idx];
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ InputType.NONE, InputType.TEXT, InputType.NUMBER,
              InputType.PHONE })
    /* package */ @interface AutofillInputType {}

    /**
     * Represents an autofill session.
     * A session holds the autofill nodes and state of a page.
     */
    public static final class Session {
        private static final String LOGTAG = "AutofillSession";

        private @NonNull final GeckoSession mGeckoSession;
        private Node mRoot;
        private SparseArray<Node> mNodes;
        // TODO: support session id?
        private int mId = View.NO_ID;
        private int mFocusedId = View.NO_ID;
        private int mFocusedRootId = View.NO_ID;

        /* package */ Session(@NonNull final GeckoSession geckoSession) {
            mGeckoSession = geckoSession;
            clear();
        }

        @AnyThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public @NonNull Rect getDefaultDimensions() {
            return Support.getDummyAutofillRect(mGeckoSession, false, null);
        }

        /* package */ void clear() {
            mId = View.NO_ID;
            mFocusedId = View.NO_ID;
            mFocusedRootId = View.NO_ID;
            mRoot = new Node.Builder(this)
                  .dimensions(getDefaultDimensions())
                  .build();
            mNodes = new SparseArray<>();
        }

        /* package */ boolean isEmpty() {
            return mNodes.size() == 0;
        }

        /* package */ void addNode(@NonNull final Node node) {
            if (DEBUG) {
                Log.d(LOGTAG, "addNode: " + node);
            }
            node.setAutofillSession(this);
            mNodes.put(node.getId(), node);

            if (node.getParentId() == View.NO_ID) {
                mRoot.addChild(node);
            }
        }

        /* package */ void setFocus(final int id, final int rootId) {
            mFocusedId = id;
            mFocusedRootId = rootId;
        }

        /* package */ int getFocusedId() {
            return mFocusedId;
        }

        /* package */ int getFocusedRootId() {
            return mFocusedRootId;
        }

        /* package */ @Nullable Node getNode(final int id) {
            return mNodes.get(id);
        }


        /**
         * Get the root node of the session tree.
         * Each session is managed in a tree with a virtual root node for the
         * document.
         *
         * @return The root {@link Node} for this session.
         */
        @AnyThread
        public @NonNull Node getRoot() {
            return mRoot;
        }

        @Override
        @AnyThread
        public String toString() {
            final StringBuilder builder = new StringBuilder("Session {");
            builder
                .append("id=").append(mId)
                .append(", focusedId=").append(mFocusedId)
                .append(", focusedRootId=").append(mFocusedRootId)
                .append(", root=").append(getRoot())
                .append("}");
            return builder.toString();
        }

        @TargetApi(23)
        @UiThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public void fillViewStructure(
                @NonNull final View view,
                @NonNull final ViewStructure structure,
                final int flags) {
            ThreadUtils.assertOnUiThread();

            getRoot().fillViewStructure(view, structure, flags);
        }
    }

    /**
     * Represents an autofill node.
     * A node is an input element and may contain child nodes forming a tree.
     */
    public static final class Node {
        private static final String LOGTAG = "AutofillNode";

        private int mId;
        private int mRootId;
        private int mParentId;
        private Session mAutofillSession;
        private @NonNull Rect mDimens;
        private @NonNull Collection<Node> mChildren;
        private @NonNull Map<String, String> mAttributes;
        private boolean mEnabled;
        private boolean mFocusable;
        private @AutofillHint int mHint;
        private @AutofillInputType int mInputType;
        private @NonNull String mTag;
        private @NonNull String mDomain;
        private @NonNull String mValue;
        private @Nullable EventCallback mCallback;

        /**
         * Get the unique (within this page) ID for this node.
         *
         * @return The unique ID of this node.
         */
        @AnyThread
        public int getId() {
            return mId;
        }

        /* package */ @NonNull Node setId(final int id) {
            mId = id;
            return this;
        }

        /* package */ @Nullable Node getRoot() {
            return getAutofillSession().getNode(mRootId);
        }

        /* package */ @NonNull Node setRootId(final int rootId) {
            mRootId = rootId;
            return this;
        }

        /* package */ @Nullable Node getParent() {
            return getAutofillSession().getNode(mParentId);
        }

        /* package */ int getParentId() {
            return mParentId;
        }

        /* package */ @NonNull Node setParentId(final int parentId) {
            mParentId = parentId;
            return this;
        }

        /* package */ @NonNull Session getAutofillSession() {
            return mAutofillSession;
        }

        /* package */ @NonNull Node setAutofillSession(
                @Nullable final Session session) {
            mAutofillSession = session;
            return this;
        }


        /**
         * Get whether this node is visible.
         * Nodes are visible, when they are part of a focused branch.
         * A focused branch includes the focused node, its siblings, its parent
         * and the session root node.
         *
         * @return True if this node is visible, false otherwise.
         */
        @AnyThread
        public boolean getVisible() {
            final int focusedId = getAutofillSession().getFocusedId();
            final int focusedRootId = getAutofillSession().getFocusedRootId();

            if (focusedId == View.NO_ID) {
                return false;
            }

            final int focusedParentId =
                getAutofillSession().getNode(focusedId).getParentId();

            return mId == View.NO_ID || // The session root node.
                   mParentId == focusedParentId ||
                   mRootId == focusedRootId;
        }

        /**
         * Get the dimensions of this node in CSS coordinates.
         * Note: Invisible nodes will report their proper dimensions, see
         * {@link #getVisible} for details.
         *
         * @return The dimensions of this node.
         */
        @AnyThread
        public @NonNull Rect getDimensions() {
            return mDimens;
        }

        /* package */ @NonNull Node setDimensions(final Rect rect) {
            mDimens = rect;
            return this;
        }

        /**
         * Get the child nodes for this node.
         *
         * @return The collection of child nodes for this node.
         */
        @AnyThread
        public @NonNull Collection<Node> getChildren() {
            return mChildren;
        }

        /* package */ @NonNull Node addChild(@NonNull final Node child) {
            mChildren.add(child);
            return this;
        }

        /**
         * Get HTML attributes for this node.
         *
         * @return The HTML attributes for this node.
         */
        @AnyThread
        public @NonNull Map<String, String> getAttributes() {
            return mAttributes;
        }

        @AnyThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public @Nullable String getAttribute(@NonNull final String key) {
            return mAttributes.get(key);
        }

        /* package */ @NonNull Node setAttributes(
                final Map<String, String> attributes) {
            mAttributes = attributes;
            return this;
        }

        /* package */ @NonNull Node setAttribute(
                final String key, final String value) {
            mAttributes.put(key, value);
            return this;
        }

        /**
         * Get whether or not this node is enabled.
         *
         * @return True if the node is enabled, false otherwise.
         */
        @AnyThread
        public boolean getEnabled() {
            return mEnabled;
        }

        /* package */ @NonNull Node setEnabled(final boolean enabled) {
            mEnabled = enabled;
            return this;
        }

        /**
         * Get whether or not this node is focusable.
         *
         * @return True if the node is focusable, false otherwise.
         */
        @AnyThread
        public boolean getFocusable() {
            return mFocusable;
        }

        /* package */ @NonNull Node setFocusable(final boolean focusable) {
            mFocusable = focusable;
            return this;
        }

        /**
         * Get whether or not this node is focused.
         *
         * @return True if this node is focused, false otherwise.
         */
        @AnyThread
        public boolean getFocused() {
            return getId() != View.NO_ID &&
                   getAutofillSession().getFocusedId() == getId();
        }

        /**
         * Get the hint for the type of data contained in this node.
         *
         * @return The input data hint for this node, one of {@link Hint}.
         */
        @AnyThread
        public @AutofillHint int getHint() {
            return mHint;
        }

        /* package */ @NonNull Node setHint(final @AutofillHint int hint) {
            mHint = hint;
            return this;
        }

        /**
         * Get the input type of this node.
         *
         * @return The input type of this node, one of {@link InputType}.
         */
        @AnyThread
        public @AutofillInputType int getInputType() {
            return mInputType;
        }

        /* package */ @NonNull Node setInputType(
                final @AutofillInputType int inputType) {
            mInputType = inputType;
            return this;
        }

        /**
         * Get the HTML tag of this node.
         *
         * @return The HTML tag of this node.
         */
        @AnyThread
        public @NonNull String getTag() {
            return mTag;
        }

        /* package */ @NonNull Node setTag(final String tag) {
            mTag = tag;
            return this;
        }

        /**
         * Get web domain of this node.
         *
         * @return The domain of this node.
         */
        @AnyThread
        public @NonNull String getDomain() {
            return mDomain;
        }

        /* package */ @NonNull Node setDomain(final String domain) {
            mDomain = domain;
            return this;
        }

        /**
         * Get the value assigned to this node.
         *
         * @return The value of this node.
         */
        @AnyThread
        public @NonNull String getValue() {
            return mValue;
        }

        /* package */ @NonNull Node setValue(final String value) {
            mValue = value;
            return this;
        }

        /* package */ @Nullable EventCallback getCallback() {
            return mCallback;
        }

        /* package */ @NonNull Node setCallback(final EventCallback callback) {
            mCallback = callback;
            return this;
        }

        /* package */ Node(@NonNull final Session session) {
            mAutofillSession = session;
            mId = View.NO_ID;
            mDimens = new Rect(0, 0, 0, 0);
            mAttributes = new ArrayMap<>();
            mEnabled = false;
            mFocusable = false;
            mHint = Hint.NONE;
            mInputType = InputType.NONE;
            mTag = "";
            mDomain = "";
            mValue = "";
            mChildren = new LinkedList<>();
        }

        @Override
        @AnyThread
        public String toString() {
            final StringBuilder builder = new StringBuilder("Node {");
            builder
                .append("id=").append(mId)
                .append(", parent=").append(mParentId)
                .append(", root=").append(mRootId)
                .append(", dims=").append(getDimensions().toShortString())
                .append(", children=[");

            for (final Node child: mChildren) {
                builder.append(child.getId()).append(", ");
            }

            builder
                .append("]")
                .append(", attrs=").append(mAttributes)
                .append(", enabled=").append(mEnabled)
                .append(", focusable=").append(mFocusable)
                .append(", focused=").append(getFocused())
                .append(", visible=").append(getVisible())
                .append(", hint=").append(Hint.toString(mHint))
                .append(", type=").append(InputType.toString(mInputType))
                .append(", tag=").append(mTag)
                .append(", domain=").append(mDomain)
                .append(", value=").append(mValue)
                .append(", callback=").append(mCallback != null)
                .append("}");

            return builder.toString();
        }

        @TargetApi(23)
        @UiThread
        @SuppressWarnings("checkstyle:javadocmethod")
        public void fillViewStructure(
                @NonNull final View view,
                @NonNull final ViewStructure structure,
                final int flags) {
            ThreadUtils.assertOnUiThread();

            Log.d(LOGTAG, "fillViewStructure");

            if (Build.VERSION.SDK_INT >= 26) {
                structure.setAutofillId(view.getAutofillId(), getId());
                structure.setWebDomain(getDomain());
                structure.setAutofillValue(AutofillValue.forText(getValue()));
            }

            structure.setId(getId(), null, null, null);
            structure.setDimens(0, 0, 0, 0,
                    getDimensions().width(),
                    getDimensions().height());

            if (Build.VERSION.SDK_INT >= 26) {
                final ViewStructure.HtmlInfo.Builder htmlBuilder =
                        structure.newHtmlInfoBuilder(getTag());
                for (final String key : getAttributes().keySet()) {
                    htmlBuilder.addAttribute(key,
                            String.valueOf(getAttribute(key)));
                }

                structure.setHtmlInfo(htmlBuilder.build());
            }

            structure.setChildCount(getChildren().size());
            int childCount = 0;

            for (final Node child : getChildren()) {
                final ViewStructure childStructure =
                    structure.newChild(childCount);
                child.fillViewStructure(view, childStructure, flags);
                childCount++;
            }

            switch (getTag()) {
                case "input":
                case "textarea":
                    structure.setClassName("android.widget.EditText");
                    structure.setEnabled(getEnabled());
                    structure.setFocusable(getFocusable());
                    structure.setFocused(getFocused());
                    structure.setVisibility(
                            getVisible()
                            ? View.VISIBLE
                            : View.INVISIBLE);

                    if (Build.VERSION.SDK_INT >= 26) {
                        structure.setAutofillType(View.AUTOFILL_TYPE_TEXT);
                    }
                    break;
                default:
                    if (childCount > 0) {
                        structure.setClassName("android.view.ViewGroup");
                    } else {
                        structure.setClassName("android.view.View");
                    }
                    break;
            }

            if (Build.VERSION.SDK_INT < 26 || !"input".equals(getTag())) {
                return;
            }
            // LastPass will fill password to the field where setAutofillHints
            // is unset and setInputType is set.
            switch (getHint()) {
                case Hint.EMAIL_ADDRESS: {
                    structure.setAutofillHints(
                            new String[]{ View.AUTOFILL_HINT_EMAIL_ADDRESS });
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_TEXT |
                            android.text.InputType
                                .TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
                    break;
                }
                case Hint.PASSWORD: {
                    structure.setAutofillHints(
                            new String[]{ View.AUTOFILL_HINT_PASSWORD });
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_TEXT |
                            android.text.InputType
                                .TYPE_TEXT_VARIATION_WEB_PASSWORD);
                    break;
                }
                case Hint.URI: {
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_TEXT |
                            android.text.InputType.TYPE_TEXT_VARIATION_URI);
                    break;
                }
                case Hint.USERNAME: {
                    structure.setAutofillHints(
                            new String[]{ View.AUTOFILL_HINT_USERNAME });
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_TEXT |
                            android.text.InputType
                                .TYPE_TEXT_VARIATION_WEB_EDIT_TEXT);
                    break;
                }
            }

            switch (getInputType()) {
                case InputType.NUMBER: {
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_NUMBER);
                    break;
                }
                case InputType.PHONE: {
                    structure.setAutofillHints(
                            new String[]{ View.AUTOFILL_HINT_PHONE });
                    structure.setInputType(
                            android.text.InputType.TYPE_CLASS_PHONE);
                    break;
                }
                default:
                    break;
            }
        }

        /* package */ static class Builder {
            private Node mNode;

            /* package */ Builder(@NonNull final Session session) {
                mNode = new Node(session);
            }

            public Builder(
                    @NonNull final Session autofillSession,
                    @NonNull final GeckoBundle bundle) {
                this(autofillSession);

                final GeckoBundle bounds = bundle.getBundle("bounds");
                mNode
                    .setAutofillSession(autofillSession)
                    .setId(bundle.getInt("id"))
                    .setParentId(bundle.getInt("parent", View.NO_ID))
                    .setRootId(bundle.getInt("root", View.NO_ID))
                    .setDomain(bundle.getString("origin", ""))
                    .setValue(bundle.getString("value", ""))
                    .setDimensions(
                        new Rect(bounds.getInt("left"),
                                 bounds.getInt("top"),
                                 bounds.getInt("right"),
                                 bounds.getInt("bottom")));

                if (mNode.getDimensions().isEmpty()) {
                    // Some nodes like <html> will have null-dimensions,
                    // we need to set them to the virtual documents dimensions.
                    mNode.setDimensions(autofillSession.getDefaultDimensions());
                }

                final GeckoBundle[] children = bundle.getBundleArray("children");
                if (children != null) {
                    for (final GeckoBundle childBundle: children) {
                        final Node child =
                            new Builder(autofillSession, childBundle).build();
                        mNode.addChild(child);
                        autofillSession.addNode(child);
                    }
                }

                String tag = bundle.getString("tag", "").toLowerCase(Locale.ROOT);
                mNode.setTag(tag);

                final GeckoBundle attrs = bundle.getBundle("attributes");

                for (final String key : attrs.keys()) {
                    mNode.setAttribute(key, String.valueOf(attrs.get(key)));
                }

                if ("input".equals(tag) &&
                    !bundle.getBoolean("editable", false)) {
                    // Don't process non-editable inputs (e.g., type="button").
                    tag = "";
                }

                switch (tag) {
                    case "input":
                    case "textarea": {
                        final boolean disabled = bundle.getBoolean("disabled");
                        mNode
                            .setEnabled(!disabled)
                            .setFocusable(!disabled);
                        break;
                    }
                    default:
                        break;
                }

                final String type =
                    bundle.getString("type", "text").toLowerCase(Locale.ROOT);

                switch (type) {
                    case "email": {
                        mNode
                            .setHint(Hint.EMAIL_ADDRESS)
                            .setInputType(InputType.TEXT);
                        break;
                    }
                    case "number": {
                        mNode.setInputType(InputType.NUMBER);
                        break;
                    }
                    case "password": {
                        mNode
                            .setHint(Hint.PASSWORD)
                            .setInputType(InputType.TEXT);
                        break;
                    }
                    case "tel": {
                        mNode.setInputType(InputType.PHONE);
                        break;
                    }
                    case "url": {
                        mNode
                            .setHint(Hint.URI)
                            .setInputType(InputType.TEXT);
                        break;
                    }
                    case "text": {
                        final String autofillHint =
                            bundle.getString("autofillhint", "").toLowerCase(Locale.ROOT);
                        if (autofillHint.equals("username")) {
                            mNode
                                .setHint(Hint.USERNAME)
                                .setInputType(InputType.TEXT);
                        }
                        break;
                    }
                }
            }

            public @NonNull Builder dimensions(final Rect rect) {
                mNode.setDimensions(rect);
                return this;
            }

            public @NonNull Node build() {
                return mNode;
            }

            public @NonNull Builder id(final int id) {
                mNode.setId(id);
                return this;
            }

            public @NonNull Builder child(@NonNull final Node child) {
                mNode.addChild(child);
                return this;
            }

            public @NonNull Builder attribute(
                    final String key, final String value) {
                mNode.setAttribute(key, value);
                return this;
            }

            public @NonNull Builder enabled(final boolean enabled) {
                mNode.setEnabled(enabled);
                return this;
            }

            public @NonNull Builder focusable(final boolean focusable) {
                mNode.setFocusable(focusable);
                return this;
            }

            public @NonNull Builder hint(final int hint) {
                mNode.setHint(hint);
                return this;
            }

            public @NonNull Builder inputType(final int inputType) {
                mNode.setInputType(inputType);
                return this;
            }

            public @NonNull Builder tag(final String tag) {
                mNode.setTag(tag);
                return this;
            }

            public @NonNull Builder domain(final String domain) {
                mNode.setDomain(domain);
                return this;
            }

            public @NonNull Builder value(final String value) {
                mNode.setValue(value);
                return this;
            }
        }
    }

    public interface Delegate {
        /**
         * Notify that an autofill event has occurred.
         *
         * The default implementation in {@link GeckoView} forwards the
         * notification to the system {@link AutofillManager}.
         * This method is only called on Android 6.0 and above and it is called
         * in viewless mode as well.
         *
         * @param session The {@link GeckoSession} instance.
         * @param notification Notification type, one of {@link Notify}.
         * @param node The target node for this event, or null for
         *             {@link Notify#SESSION_CANCELED}.
         */
        @UiThread
        default void onAutofill(@NonNull final GeckoSession session,
                                @AutofillNotify final int notification,
                                @Nullable final Node node) {}
    }

    /* package */ static final class Support implements BundleEventListener {
        private static final String LOGTAG = "AutofillSupport";

        private @NonNull final GeckoSession mGeckoSession;
        private @NonNull final Session mAutofillSession;
        private Delegate mDelegate;

        public Support(@NonNull final GeckoSession geckoSession) {
            mGeckoSession = geckoSession;
            mAutofillSession = new Session(mGeckoSession);
        }

        public void registerListeners() {
            mGeckoSession.getEventDispatcher().registerUiThreadListener(
                    this,
                    "GeckoView:AddAutofill",
                    "GeckoView:ClearAutofill",
                    "GeckoView:CommitAutofill",
                    "GeckoView:OnAutofillFocus",
                    "GeckoView:UpdateAutofill");

        }

        @Override
        public void handleMessage(
                final String event,
                final GeckoBundle message,
                final EventCallback callback) {
            if ("GeckoView:AddAutofill".equals(event)) {
                addNode(message, callback);
            } else if ("GeckoView:ClearAutofill".equals(event)) {
                clear();
            } else if ("GeckoView:OnAutofillFocus".equals(event)) {
                onFocusChanged(message);
            } else if ("GeckoView:CommitAutofill".equals(event)) {
                commit(message);
            } else if ("GeckoView:UpdateAutofill".equals(event)) {
                update(message);
            }
        }

        /**
         * Perform auto-fill using the specified values.
         *
         * @param values Map of auto-fill IDs to values.
         */
        @UiThread
        public void autofill(final SparseArray<CharSequence> values) {
            ThreadUtils.assertOnUiThread();

            if (getAutofillSession().isEmpty()) {
                return;
            }

            GeckoBundle response = null;
            EventCallback callback = null;

            for (int i = 0; i < values.size(); i++) {
                final int id = values.keyAt(i);
                final CharSequence value = values.valueAt(i);

                if (DEBUG) {
                    Log.d(LOGTAG, "Process autofill for id=" + id + ", value=" + value);
                }

                int rootId = id;
                for (int currentId = id; currentId != View.NO_ID; ) {
                    final Node elem = getAutofillSession().getNode(currentId);

                    if (elem == null) {
                        return;
                    }
                    rootId = currentId;
                    currentId = elem.getParentId();
                }

                final Node root = getAutofillSession().getNode(rootId);
                final EventCallback newCallback =
                    root != null
                    ? root.getCallback()
                    : null;
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

        @UiThread
        public void setDelegate(final @Nullable Delegate delegate) {
            ThreadUtils.assertOnUiThread();

            mDelegate = delegate;
        }

        @UiThread
        public @Nullable Delegate getDelegate() {
            ThreadUtils.assertOnUiThread();

            return mDelegate;
        }

        @UiThread
        public @NonNull Session getAutofillSession() {
            ThreadUtils.assertOnUiThread();

            return mAutofillSession;
        }

        /* package */ void addNode(
                @NonNull final GeckoBundle message,
                @NonNull final EventCallback callback) {
            final boolean initializing = getAutofillSession().isEmpty();
            final int id = message.getInt("id");

            if (DEBUG) {
                Log.d(LOGTAG, "addNode(" + id + ')');
            }

            if (initializing) {
                // TODO: We need this to set the dimensions on the root node.
                // We should find a better way of handling this.
                getAutofillSession().clear();
            }

            final Node node = new Node.Builder(
                    getAutofillSession(), message).build();
            node.setCallback(callback);
            getAutofillSession().addNode(node);
            maybeDispatch(
                    initializing
                    ? Notify.SESSION_STARTED
                    : Notify.NODE_ADDED,
                    node);
        }

        private void maybeDispatch(
                final @AutofillNotify int notification, final Node node) {
            if (mDelegate == null) {
                return;
            }

            mDelegate.onAutofill(mGeckoSession, notification, node);
        }

        /* package */ void commit(@Nullable final GeckoBundle message) {
            if (getAutofillSession().isEmpty()) {
                return;
            }

            final int id = message.getInt("id");

            if (DEBUG) {
                Log.d(LOGTAG, "commit(" + id + ")");
            }

            maybeDispatch(
                    Notify.SESSION_COMMITTED,
                    getAutofillSession().getNode(id));
        }

        /* package */ void update(@Nullable final GeckoBundle message) {
            if (getAutofillSession().isEmpty()) {
                return;
            }

            final int id = message.getInt("id");

            if (DEBUG) {
                Log.d(LOGTAG, "update(" + id + ")");
            }

            final Node node = getAutofillSession().getNode(id);
            final String value = message.getString("value", "");

            if (node == null) {
                Log.d(LOGTAG, "could not find node " + id);
                return;
            }

            if (DEBUG) {
                Log.d(LOGTAG, "updating node " + id + " value from " +
                      node.getValue() + " to " + value);
            }

            node.setValue(value);
            maybeDispatch(Notify.NODE_UPDATED, node);
        }

        /* package */ void clear() {
            if (getAutofillSession().isEmpty()) {
                return;
            }

            if (DEBUG) {
                Log.d(LOGTAG, "clear()");
            }

            getAutofillSession().clear();
            maybeDispatch(Notify.SESSION_CANCELED, null);
        }

        /* package */ void onFocusChanged(
                @Nullable final GeckoBundle message) {
            if (getAutofillSession().isEmpty()) {
                return;
            }

            final int prevId = getAutofillSession().getFocusedId();
            final int id;
            final int root;

            if (message != null) {
                id = message.getInt("id");
                root = message.getInt("root");
            } else {
                id = root = View.NO_ID;
            }

            if (DEBUG) {
                Log.d(LOGTAG, "onFocusChanged(" + prevId + " -> " + id + ')');
            }

            if (prevId == id) {
                return;
            }

            getAutofillSession().setFocus(id, root);

            if (prevId != View.NO_ID) {
                maybeDispatch(
                        Notify.NODE_BLURRED,
                        getAutofillSession().getNode(prevId));
            }

            if (id != View.NO_ID) {
                maybeDispatch(
                        Notify.NODE_FOCUSED,
                        getAutofillSession().getNode(id));
            }
        }

        /* package */ static Rect getDummyAutofillRect(
                @NonNull final GeckoSession geckoSession,
                final boolean screen,
                @Nullable final View view) {
            final Rect rect = new Rect();
            geckoSession.getSurfaceBounds(rect);

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

        @UiThread
        public void onActiveChanged(final boolean active) {
            ThreadUtils.assertOnUiThread();

            final int focusedId = getAutofillSession().getFocusedId();

            if (focusedId == View.NO_ID) {
                return;
            }

            maybeDispatch(
                    active
                    ? Notify.NODE_FOCUSED
                    : Notify.NODE_BLURRED,
                    getAutofillSession().getNode(focusedId));
        }
    }
}
