/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.collection.ArrayMap;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

public class Autofill {
  private static final boolean DEBUG = false;

  public @interface AutofillNotify {}

  public static final class Hint {
    private Hint() {}

    /** Hint indicating that no special handling is required. */
    public static final int NONE = -1;

    /** Hint indicating that a node represents an email address. */
    public static final int EMAIL_ADDRESS = 0;

    /** Hint indicating that a node represents a password. */
    public static final int PASSWORD = 1;

    /** Hint indicating that a node represents an URI. */
    public static final int URI = 2;

    /** Hint indicating that a node represents a username. */
    public static final int USERNAME = 3;

    @AnyThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public static @Nullable String toString(final @AutofillHint int hint) {
      final int idx = hint + 1;
      final String[] map = new String[] {"NONE", "EMAIL", "PASSWORD", "URI", "USERNAME"};

      if (idx < 0 || idx >= map.length) {
        return null;
      }
      return map[idx];
    }
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({Hint.NONE, Hint.EMAIL_ADDRESS, Hint.PASSWORD, Hint.URI, Hint.USERNAME})
  public @interface AutofillHint {}

  public static final class InputType {
    private InputType() {}

    /** Indicates that a node is not a known input type. */
    public static final int NONE = -1;

    /** Indicates that a node is a text input type. Example: {@code <input type="text">} */
    public static final int TEXT = 0;

    /** Indicates that a node is a number input type. Example: {@code <input type="number">} */
    public static final int NUMBER = 1;

    /** Indicates that a node is a phone input type. Example: {@code <input type="tel">} */
    public static final int PHONE = 2;

    @AnyThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public static @Nullable String toString(final @AutofillInputType int type) {
      final int idx = type + 1;
      final String[] map = new String[] {"NONE", "TEXT", "NUMBER", "PHONE"};

      if (idx < 0 || idx >= map.length) {
        return null;
      }
      return map[idx];
    }
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({InputType.NONE, InputType.TEXT, InputType.NUMBER, InputType.PHONE})
  public @interface AutofillInputType {}

  /** Represents autofill data associated to a {@link Node}. */
  public static class NodeData {
    /** Autofill id for this node. */
    final int id;

    String value;
    Node node;
    EventCallback callback;

    NodeData(final int id, final Node node) {
      this.id = id;
      this.node = node;
    }

    /**
     * Gets the value for this node.
     *
     * @return a String representing the value for this node.
     */
    @AnyThread
    public @Nullable String getValue() {
      return value;
    }

    /**
     * Returns the autofill id for this node.
     *
     * @return an int representing the id for this node.
     */
    @AnyThread
    public int getId() {
      return id;
    }
  }

  /** Represents an autofill session. A session holds the autofill nodes and state of a page. */
  public static final class Session {
    private static final String LOGTAG = "AutofillSession";

    private @NonNull final GeckoSession mGeckoSession;
    private Node mRoot;
    private HashMap<String, NodeData> mUuidToNodeData;
    private SparseArray<Node> mIdToNode;
    private int mCurrentIndex = 0;
    private String mId = null;

    // We can't store the Node directly because it might be updated by subsequent NodeAdd calls.
    private String mFocusedUuid = null;

    /* package */ Session(@NonNull final GeckoSession geckoSession) {
      mGeckoSession = geckoSession;
      // Dummy session until a real one gets created
      clear(UUID.randomUUID().toString());
    }

    @UiThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public @NonNull Rect getDefaultDimensions() {
      final Rect rect = new Rect();
      mGeckoSession.getSurfaceBounds(rect);
      return rect;
    }

    /* package */ void clear(final String newSessionId) {
      mId = newSessionId;
      mFocusedUuid = null;
      mRoot = Node.newDummyRoot(getDefaultDimensions(), newSessionId);
      mIdToNode = new SparseArray<>();
      mUuidToNodeData = new HashMap<>();
      addNode(mRoot);
    }

    /* package */ boolean isEmpty() {
      // Root data is always there
      return mUuidToNodeData.size() == 1;
    }

    /**
     * Get data for the given node.
     *
     * @param node the {@link Node} get data for.
     * @return the {@link NodeData} for the given node.
     */
    @UiThread
    public @NonNull NodeData dataFor(final @NonNull Node node) {
      final NodeData data = mUuidToNodeData.get(node.getUuid());
      Objects.requireNonNull(data);
      return data;
    }

    /**
     * Perform auto-fill using the specified values.
     *
     * @param values Map of auto-fill IDs to values.
     */
    @UiThread
    public void autofill(@NonNull final SparseArray<CharSequence> values) {
      ThreadUtils.assertOnUiThread();

      if (isEmpty()) {
        return;
      }

      final HashMap<Node, GeckoBundle> valueBundles = new HashMap<>();

      for (int i = 0; i < values.size(); i++) {
        final int id = values.keyAt(i);
        final Node node = getNode(id);
        if (node == null) {
          Log.w(LOGTAG, "Could not find node id=" + id);
          continue;
        }

        final CharSequence value = values.valueAt(i);

        if (DEBUG) {
          Log.d(LOGTAG, "Process autofill for id=" + id + ", value=" + value);
        }

        if (node == getRoot()) {
          // We cannot autofill the session root as it does not correspond to a
          // real element on the page.
          Log.w(LOGTAG, "Ignoring autofill on session root.");
          continue;
        }

        final Node root = node.getRoot();
        if (!valueBundles.containsKey(root)) {
          valueBundles.put(root, new GeckoBundle());
        }
        valueBundles.get(root).putString(node.getUuid(), String.valueOf(value));
      }

      for (final Node root : valueBundles.keySet()) {
        final NodeData data = dataFor(root);
        Objects.requireNonNull(data);
        final EventCallback callback = data.callback;
        callback.sendSuccess(valueBundles.get(root));
      }
    }

    /* package */ void addRoot(@NonNull final Node node, final EventCallback callback) {
      if (DEBUG) {
        Log.d(LOGTAG, "addRoot: " + node);
      }

      mRoot.addChild(node);
      addNode(node);
      dataFor(node).callback = callback;
    }

    /* package */ void addNode(@NonNull final Node node) {
      if (DEBUG) {
        Log.d(LOGTAG, "addNode: " + node);
      }

      NodeData data = mUuidToNodeData.get(node.getUuid());
      if (data == null) {
        final int nodeId = mCurrentIndex++;
        data = new NodeData(nodeId, node);
        mUuidToNodeData.put(node.getUuid(), data);
      } else {
        data.node = node;
      }

      mIdToNode.put(data.id, node);
      for (final Node child : node.getChildren()) {
        addNode(child);
      }
    }

    /**
     * Returns true if the node is currently visible in the page.
     *
     * @param node the {@link Node} instance
     * @return true if the node is visible, false otherwise.
     */
    @UiThread
    public boolean isVisible(final @NonNull Node node) {
      if (!Objects.equals(node.mSessionId, mId)) {
        Log.w(LOGTAG, "Requesting visibility for older session " + node.mSessionId);
        return false;
      }
      if (mRoot == node) {
        // The root is always visible
        return true;
      }
      final Node focused = getFocused();
      if (focused == null) {
        return false;
      }
      final Node focusedRoot = focused.getRoot();
      final Node focusedParent = focused.getParent();

      final String parentUuid = node.getParent() != null ? node.getParent().getUuid() : null;
      final String rootUuid = node.getRoot() != null ? node.getRoot().getUuid() : null;

      return (focusedParent != null && focusedParent.getUuid().equals(parentUuid))
          || (focusedRoot != null && focusedRoot.getUuid().equals(rootUuid));
    }

    /**
     * Returns the currently focused node.
     *
     * @return a reference to the {@link Node} that is currently focused or null if no node is
     *     currently focused.
     */
    @UiThread
    public @Nullable Node getFocused() {
      return getNode(mFocusedUuid);
    }

    /* package */ void setFocus(final Node node) {
      mFocusedUuid = node != null ? node.getUuid() : null;
    }

    /**
     * Returns the currently focused node data.
     *
     * @return a refernce to {@link NodeData} or null if no node is focused.
     */
    @UiThread
    public @Nullable NodeData getFocusedData() {
      final Node focused = getFocused();
      return focused != null ? dataFor(focused) : null;
    }

    /* package */ @Nullable
    Node getNode(final String uuid) {
      if (uuid == null) {
        return null;
      }
      final NodeData nodeData = mUuidToNodeData.get(uuid);
      if (nodeData == null) {
        return null;
      }
      return nodeData.node;
    }

    /* package */ Node getNode(final int id) {
      return mIdToNode.get(id);
    }

    /**
     * Get the root node of the session tree. Each session is managed in a tree with a virtual root
     * node for the document.
     *
     * @return The root {@link Node} for this session.
     */
    @AnyThread
    public @NonNull Node getRoot() {
      return mRoot;
    }

    /* package */ String getId() {
      return mId;
    }

    @Override
    @UiThread
    public String toString() {
      final StringBuilder builder = new StringBuilder("Session {");
      final Node focused = getFocused();
      builder
          .append("id=")
          .append(mId)
          .append(", focused=")
          .append(mFocusedUuid)
          .append(", focusedRoot=")
          .append(
              (focused != null && focused.getRoot() != null) ? focused.getRoot().getUuid() : null)
          .append(", root=")
          .append(getRoot())
          .append("}");
      return builder.toString();
    }

    @TargetApi(23)
    @UiThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public void fillViewStructure(
        @NonNull final View view, @NonNull final ViewStructure structure, final int flags) {
      ThreadUtils.assertOnUiThread();
      fillViewStructure(getRoot(), view, structure, flags);
    }

    @TargetApi(23)
    @UiThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public void fillViewStructure(
        final @NonNull Node node,
        @NonNull final View view,
        @NonNull final ViewStructure structure,
        final int flags) {
      ThreadUtils.assertOnUiThread();

      if (DEBUG) {
        Log.d(LOGTAG, "fillViewStructure");
      }

      final NodeData data = dataFor(node);
      if (data == null) {
        return;
      }

      if (Build.VERSION.SDK_INT >= 26) {
        structure.setAutofillId(view.getAutofillId(), data.id);
        structure.setWebDomain(node.getDomain());
        structure.setAutofillValue(AutofillValue.forText(data.value));
      }

      structure.setId(data.id, null, null, null);
      structure.setDimens(0, 0, 0, 0, node.getDimensions().width(), node.getDimensions().height());

      if (Build.VERSION.SDK_INT >= 26) {
        final ViewStructure.HtmlInfo.Builder htmlBuilder =
            structure.newHtmlInfoBuilder(node.getTag());
        for (final String key : node.getAttributes().keySet()) {
          htmlBuilder.addAttribute(key, String.valueOf(node.getAttribute(key)));
        }

        structure.setHtmlInfo(htmlBuilder.build());
      }

      structure.setChildCount(node.getChildren().size());
      int childCount = 0;

      for (final Node child : node.getChildren()) {
        final ViewStructure childStructure = structure.newChild(childCount);
        fillViewStructure(child, view, childStructure, flags);
        childCount++;
      }

      switch (node.getTag()) {
        case "input":
        case "textarea":
          structure.setClassName("android.widget.EditText");
          structure.setEnabled(node.getEnabled());
          structure.setFocusable(node.getFocusable());
          structure.setFocused(node.equals(getFocused()));
          structure.setVisibility(isVisible(node) ? View.VISIBLE : View.INVISIBLE);

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

      if (Build.VERSION.SDK_INT < 26 || !"input".equals(node.getTag())) {
        return;
      }
      // LastPass will fill password to the field where setAutofillHints
      // is unset and setInputType is set.
      switch (node.getHint()) {
        case Hint.EMAIL_ADDRESS:
          {
            structure.setAutofillHints(new String[] {View.AUTOFILL_HINT_EMAIL_ADDRESS});
            structure.setInputType(
                android.text.InputType.TYPE_CLASS_TEXT
                    | android.text.InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
            break;
          }
        case Hint.PASSWORD:
          {
            structure.setAutofillHints(new String[] {View.AUTOFILL_HINT_PASSWORD});
            structure.setInputType(
                android.text.InputType.TYPE_CLASS_TEXT
                    | android.text.InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);
            break;
          }
        case Hint.URI:
          {
            structure.setInputType(
                android.text.InputType.TYPE_CLASS_TEXT
                    | android.text.InputType.TYPE_TEXT_VARIATION_URI);
            break;
          }
        case Hint.USERNAME:
          {
            structure.setAutofillHints(new String[] {View.AUTOFILL_HINT_USERNAME});
            structure.setInputType(
                android.text.InputType.TYPE_CLASS_TEXT
                    | android.text.InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT);
            break;
          }
        case Hint.NONE:
          {
            // Nothing to do.
            break;
          }
      }

      switch (node.getInputType()) {
        case InputType.NUMBER:
          {
            structure.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
            break;
          }
        case InputType.PHONE:
          {
            structure.setAutofillHints(new String[] {View.AUTOFILL_HINT_PHONE});
            structure.setInputType(android.text.InputType.TYPE_CLASS_PHONE);
            break;
          }
        case InputType.TEXT:
        case InputType.NONE:
          // Nothing to do.
          break;
      }
    }
  }

  /**
   * Represents an autofill node. A node is an input element and may contain child nodes forming a
   * tree.
   */
  public static final class Node {
    private final String mUuid;
    private final Node mRoot;
    private final Node mParent;
    private final @NonNull Rect mDimens;
    private final @NonNull Map<String, Node> mChildren;
    private final @NonNull Map<String, String> mAttributes;
    private final boolean mEnabled;
    private final boolean mFocusable;
    private final @AutofillHint int mHint;
    private final @AutofillInputType int mInputType;
    private final @NonNull String mTag;
    private final @NonNull String mDomain;
    private final String mSessionId;

    /* package */
    @NonNull
    String getUuid() {
      return mUuid;
    }

    /* package */
    @Nullable
    Node getRoot() {
      return mRoot;
    }

    /* package */
    @Nullable
    Node getParent() {
      return mParent;
    }

    /**
     * Get the dimensions of this node in CSS coordinates. Note: Invisible nodes will report their
     * proper dimensions.
     *
     * @return The dimensions of this node.
     */
    @AnyThread
    public @NonNull Rect getDimensions() {
      return mDimens;
    }

    /**
     * Get the child nodes for this node.
     *
     * @return The collection of child nodes for this node.
     */
    @AnyThread
    public @NonNull Collection<Node> getChildren() {
      return mChildren.values();
    }

    /* package */
    @NonNull
    Node addChild(@NonNull final Node child) {
      mChildren.put(child.getUuid(), child);
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

    /**
     * Get whether or not this node is enabled.
     *
     * @return True if the node is enabled, false otherwise.
     */
    @AnyThread
    public boolean getEnabled() {
      return mEnabled;
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

    /**
     * Get the hint for the type of data contained in this node.
     *
     * @return The input data hint for this node, one of {@link Hint}.
     */
    @AnyThread
    public @AutofillHint int getHint() {
      return mHint;
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

    /**
     * Get the HTML tag of this node.
     *
     * @return The HTML tag of this node.
     */
    @AnyThread
    public @NonNull String getTag() {
      return mTag;
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

    /* package */
    static Node newDummyRoot(final Rect dimensions, final String sessionId) {
      return new Node(dimensions, sessionId);
    }

    /* package */ Node(final Rect dimensions, final String sessionId) {
      mRoot = null;
      mParent = null;
      mUuid = UUID.randomUUID().toString();
      mDimens = dimensions;
      mSessionId = sessionId;
      mAttributes = new ArrayMap<>();
      mEnabled = false;
      mFocusable = false;
      mHint = Hint.NONE;
      mInputType = InputType.NONE;
      mTag = "";
      mDomain = "";
      mChildren = new HashMap<>();
    }

    @Override
    @AnyThread
    public String toString() {
      final StringBuilder builder = new StringBuilder("Node {");
      builder
          .append("uuid=")
          .append(mUuid)
          .append(", sessionId=")
          .append(mSessionId)
          .append(", parent=")
          .append(mParent != null ? mParent.getUuid() : null)
          .append(", root=")
          .append(mRoot != null ? mRoot.getUuid() : null)
          .append(", dims=")
          .append(getDimensions().toShortString())
          .append(", children=[");

      for (final Node child : mChildren.values()) {
        builder.append(child.getUuid()).append(", ");
      }

      builder
          .append("]")
          .append(", attrs=")
          .append(mAttributes)
          .append(", enabled=")
          .append(mEnabled)
          .append(", focusable=")
          .append(mFocusable)
          .append(", hint=")
          .append(Hint.toString(mHint))
          .append(", type=")
          .append(InputType.toString(mInputType))
          .append(", tag=")
          .append(mTag)
          .append(", domain=")
          .append(mDomain)
          .append("}");

      return builder.toString();
    }

    /* package */ Node(
        @NonNull final GeckoBundle bundle, final Rect defaultDimensions, final String sessionId) {
      this(bundle, /* root */ null, /* parent */ null, defaultDimensions, sessionId);
    }

    /* package */ Node(
        @NonNull final GeckoBundle bundle,
        final Node root,
        final Node parent,
        final Rect defaultDimensions,
        final String sessionId) {
      final GeckoBundle bounds = bundle.getBundle("bounds");

      mSessionId = sessionId;
      mUuid = bundle.getString("uuid");
      mDomain = bundle.getString("origin", "");
      final Rect dimens =
          new Rect(
              bounds.getInt("left"),
              bounds.getInt("top"),
              bounds.getInt("right"),
              bounds.getInt("bottom"));
      if (dimens.isEmpty()) {
        // Some nodes like <html> will have null-dimensions,
        // we need to set them to the virtual documents dimensions.
        mDimens = defaultDimensions;
      } else {
        mDimens = dimens;
      }

      mParent = parent;
      // If the root is null, then this object is the root itself
      mRoot = root != null ? root : this;

      final GeckoBundle[] children = bundle.getBundleArray("children");
      final Map<String, Node> childrenMap = new HashMap<>(children != null ? children.length : 0);

      if (children != null) {
        for (final GeckoBundle childBundle : children) {
          final Node child = new Node(childBundle, mRoot, this, defaultDimensions, sessionId);
          childrenMap.put(child.getUuid(), child);
        }
      }

      mChildren = childrenMap;

      mTag = bundle.getString("tag", "").toLowerCase(Locale.ROOT);

      final GeckoBundle attrs = bundle.getBundle("attributes");
      final Map<String, String> attributes = new HashMap<>();

      for (final String key : attrs.keys()) {
        attributes.put(key, String.valueOf(attrs.get(key)));
      }

      mAttributes = attributes;

      mEnabled =
          enabledFromBundle(
              mTag, bundle.getBoolean("editable", false), bundle.getBoolean("disabled", false));
      mFocusable = mEnabled;

      final String type = bundle.getString("type", "text").toLowerCase(Locale.ROOT);
      final String hint = bundle.getString("autofillhint", "").toLowerCase(Locale.ROOT);
      mInputType = typeFromBundle(type, hint);
      mHint = hintFromBundle(type, hint);
    }

    private boolean enabledFromBundle(
        final String tag, final boolean editable, final boolean disabled) {
      switch (tag) {
        case "input":
          {
            if (!editable) {
              // Don't process non-editable inputs (e.g., type="button").
              return false;
            }
            return !disabled;
          }
        case "textarea":
          return !disabled;
        default:
          return false;
      }
    }

    private @AutofillHint int hintFromBundle(final String type, final String hint) {
      switch (type) {
        case "email":
          return Hint.EMAIL_ADDRESS;
        case "password":
          return Hint.PASSWORD;
        case "url":
          return Hint.URI;
        case "text":
          {
            if (hint.equals("username")) {
              return Hint.USERNAME;
            }
            break;
          }
      }

      return Hint.NONE;
    }

    private @AutofillInputType int typeFromBundle(final String type, final String hint) {
      switch (type) {
        case "password":
        case "url":
        case "email":
          return InputType.TEXT;
        case "number":
          return InputType.NUMBER;
        case "tel":
          return InputType.PHONE;
        case "text":
          {
            if (hint.equals("username")) {
              return InputType.TEXT;
            }
            break;
          }
      }

      return InputType.NONE;
    }
  }

  public interface Delegate {

    /**
     * An autofill session has started. Usually triggered by page load.
     *
     * @param session The {@link GeckoSession} instance.
     */
    @UiThread
    default void onSessionStart(@NonNull final GeckoSession session) {}
    /**
     * An autofill session has been committed. Triggered by form submission or navigation.
     *
     * @param session The {@link GeckoSession} instance.
     * @param node the node that is being committed.
     * @param data the node data associated to the node being committed.
     */
    @UiThread
    default void onSessionCommit(
        @NonNull final GeckoSession session,
        @NonNull final Node node,
        @NonNull final NodeData data) {}
    /**
     * An autofill session has been canceled. Triggered by page unload.
     *
     * @param session The {@link GeckoSession} instance.
     */
    @UiThread
    default void onSessionCancel(@NonNull final GeckoSession session) {}
    /**
     * A node within the autofill session has been added.
     *
     * @param session The {@link GeckoSession} instance.
     * @param node The {@link Node} that was added.
     * @param data The {@link NodeData} associated to the note that was added.
     */
    @UiThread
    default void onNodeAdd(
        @NonNull final GeckoSession session,
        @NonNull final Node node,
        @NonNull final NodeData data) {}
    /**
     * A node within the autofill session has been removed.
     *
     * @param session The {@link GeckoSession} instance.
     * @param node The {@link Node} that was removed.
     * @param data The {@link NodeData} associated to the note that was removed.
     */
    @UiThread
    default void onNodeRemove(
        @NonNull final GeckoSession session,
        @NonNull final Node node,
        @NonNull final NodeData data) {}
    /**
     * A node within the autofill session has been updated.
     *
     * @param session The {@link GeckoSession} instance.
     * @param node The {@link Node} that was updated.
     * @param data The {@link NodeData} associated to the note that was updated.
     */
    @UiThread
    default void onNodeUpdate(
        @NonNull final GeckoSession session,
        @NonNull final Node node,
        @NonNull final NodeData data) {}
    /**
     * A node within the autofill session has gained focus.
     *
     * @param session The {@link GeckoSession} instance.
     * @param focused The {@link Node} that is now focused.
     * @param data The {@link NodeData} associated to the note that is now focused.
     */
    @UiThread
    default void onNodeFocus(
        @NonNull final GeckoSession session,
        @NonNull final Node focused,
        @NonNull final NodeData data) {}
    /**
     * A node within the autofill session has lost focus.
     *
     * @param session The {@link GeckoSession} instance.
     * @param prev The {@link Node} that lost focus.
     * @param data The {@link NodeData} associated to the note that lost focus.
     */
    @UiThread
    default void onNodeBlur(
        @NonNull final GeckoSession session,
        @NonNull final Node prev,
        @NonNull final NodeData data) {}
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
      mGeckoSession
          .getEventDispatcher()
          .registerUiThreadListener(
              this,
              "GeckoView:StartAutofill",
              "GeckoView:AddAutofill",
              "GeckoView:ClearAutofill",
              "GeckoView:CommitAutofill",
              "GeckoView:OnAutofillFocus",
              "GeckoView:UpdateAutofill");
    }

    @Override
    public void handleMessage(
        final String event, final GeckoBundle message, final EventCallback callback) {
      Log.d(LOGTAG, "handleMessage " + event);
      if ("GeckoView:AddAutofill".equals(event)) {
        addNode(message.getBundle("node"), callback);
      } else if ("GeckoView:StartAutofill".equals(event)) {
        start(message.getString("sessionId"));
      } else if ("GeckoView:ClearAutofill".equals(event)) {
        clear();
      } else if ("GeckoView:OnAutofillFocus".equals(event)) {
        onFocusChanged(message.getBundle("node"));
      } else if ("GeckoView:CommitAutofill".equals(event)) {
        commit(message.getBundle("node"));
      } else if ("GeckoView:UpdateAutofill".equals(event)) {
        update(message.getBundle("node"));
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
        @NonNull final GeckoBundle message, @NonNull final EventCallback callback) {
      final Session session = getAutofillSession();
      final Node node = new Node(message, session.getDefaultDimensions(), session.getId());

      session.addRoot(node, callback);
      addValues(message);

      if (mDelegate != null) {
        mDelegate.onNodeAdd(mGeckoSession, node, getAutofillSession().dataFor(node));
      }
    }

    private void addValues(final GeckoBundle message) {
      final String uuid = message.getString("uuid");
      if (uuid == null) {
        return;
      }

      final String value = message.getString("value");
      final Node node = getAutofillSession().getNode(uuid);
      if (node == null) {
        Log.w(LOGTAG, "Cannot find node uuid=" + uuid);
        return;
      }
      Objects.requireNonNull(node);
      final NodeData data = getAutofillSession().dataFor(node);
      Objects.requireNonNull(data);
      data.value = value;

      final GeckoBundle[] children = message.getBundleArray("children");
      if (children != null) {
        for (final GeckoBundle child : children) {
          addValues(child);
        }
      }
    }

    /* package */ void start(@Nullable final String sessionId) {
      // Make sure we start with a clean session
      getAutofillSession().clear(sessionId);
      if (mDelegate != null) {
        mDelegate.onSessionStart(mGeckoSession);
      }
    }

    /* package */ void commit(@Nullable final GeckoBundle message) {
      if (getAutofillSession().isEmpty() || message == null) {
        return;
      }

      final String uuid = message.getString("uuid");
      final Node node = getAutofillSession().getNode(uuid);
      if (node == null) {
        Log.w(LOGTAG, "Cannot find node uuid=" + uuid);
        return;
      }

      if (DEBUG) {
        Log.d(LOGTAG, "commit(" + uuid + ")");
      }

      if (mDelegate != null) {
        mDelegate.onSessionCommit(mGeckoSession, node, getAutofillSession().dataFor(node));
      }
    }

    /* package */ void update(@Nullable final GeckoBundle message) {
      if (getAutofillSession().isEmpty() || message == null) {
        return;
      }

      final String uuid = message.getString("uuid");

      if (DEBUG) {
        Log.d(LOGTAG, "update(" + uuid + ")");
      }

      final Node node = getAutofillSession().getNode(uuid);
      final String value = message.getString("value", "");

      if (node == null) {
        Log.d(LOGTAG, "could not find node " + uuid);
        return;
      }

      if (DEBUG) {
        final NodeData data = getAutofillSession().dataFor(node);
        Log.d(
            LOGTAG,
            "updating node " + uuid + " value from " + data != null
                ? data.value
                : null + " to " + value);
      }

      getAutofillSession().dataFor(node).value = value;

      if (mDelegate != null) {
        mDelegate.onNodeUpdate(mGeckoSession, node, getAutofillSession().dataFor(node));
      }
    }

    /* package */ void clear() {
      if (getAutofillSession().isEmpty()) {
        return;
      }

      if (DEBUG) {
        Log.d(LOGTAG, "clear()");
      }

      getAutofillSession().clear(null);
      if (mDelegate != null) {
        mDelegate.onSessionCancel(mGeckoSession);
      }
    }

    /* package */ void onFocusChanged(@Nullable final GeckoBundle message) {
      final Session session = getAutofillSession();
      if (session.isEmpty()) {
        return;
      }

      final Node prev = getAutofillSession().getFocused();
      final String prevUuid = prev != null ? prev.getUuid() : null;
      final String uuid = message != null ? message.getString("uuid") : null;

      final Node focused;
      if (uuid == null) {
        focused = null;
      } else {
        focused = session.getNode(uuid);
        if (focused == null) {
          Log.w(LOGTAG, "Cannot find node uuid=" + uuid);
          return;
        }
      }

      if (DEBUG) {
        Log.d(
            LOGTAG,
            "onFocusChanged(" + (prev != null ? prev.getUuid() : null) + " -> " + uuid + ')');
      }

      if (Objects.equals(uuid, prevUuid)) {
        // Nothing changed, nothing to do.
        return;
      }

      session.setFocus(focused);

      if (mDelegate != null) {
        if (prev != null) {
          mDelegate.onNodeBlur(mGeckoSession, prev, getAutofillSession().dataFor(prev));
        }
        if (uuid != null) {
          mDelegate.onNodeFocus(mGeckoSession, focused, getAutofillSession().dataFor(focused));
        }
      }
    }

    @UiThread
    public void onActiveChanged(final boolean active) {
      ThreadUtils.assertOnUiThread();

      final Node focused = getAutofillSession().getFocused();

      if (focused == null) {
        return;
      }

      if (mDelegate != null) {
        if (active) {
          mDelegate.onNodeFocus(mGeckoSession, focused, getAutofillSession().dataFor(focused));
        } else {
          mDelegate.onNodeBlur(mGeckoSession, focused, getAutofillSession().dataFor(focused));
        }
      }
    }
  }
}
