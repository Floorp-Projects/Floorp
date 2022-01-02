/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.os.Build;
import android.util.Log;
import android.util.SparseArray;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;
import org.json.JSONException;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.MultiMap;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

public class WebExtensionController {
  private static final String LOGTAG = "WebExtension";

  private DebuggerDelegate mDebuggerDelegate;
  private PromptDelegate mPromptDelegate;
  private final WebExtension.Listener<WebExtension.TabDelegate> mListener;

  // Map [ (extensionId, nativeApp, session) -> message ]
  private final MultiMap<MessageRecipient, Message> mPendingMessages;
  private final MultiMap<String, Message> mPendingNewTab;
  private final MultiMap<String, Message> mPendingBrowsingData;
  private final MultiMap<String, Message> mPendingDownload;

  private final SparseArray<WebExtension.Download> mDownloads;

  private static class Message {
    final GeckoBundle bundle;
    final EventCallback callback;
    final String event;
    final GeckoSession session;

    public Message(
        final String event,
        final GeckoBundle bundle,
        final EventCallback callback,
        final GeckoSession session) {
      this.bundle = bundle;
      this.callback = callback;
      this.event = event;
      this.session = session;
    }
  }

  private static class ExtensionStore {
    private final Map<String, WebExtension> mData = new HashMap<>();
    private Observer mObserver;

    interface Observer {
      /**
       * * This event is fired every time a new extension object is created by the store.
       *
       * @param extension the newly-created extension object
       */
      WebExtension onNewExtension(final GeckoBundle extension);
    }

    public GeckoResult<WebExtension> get(final String id) {
      final WebExtension extension = mData.get(id);
      if (extension != null) {
        return GeckoResult.fromValue(extension);
      }

      final GeckoBundle bundle = new GeckoBundle(1);
      bundle.putString("extensionId", id);

      final GeckoResult<WebExtension> pending =
          EventDispatcher.getInstance()
              .queryBundle("GeckoView:WebExtension:Get", bundle)
              .map(
                  extensionBundle -> {
                    final WebExtension ext = mObserver.onNewExtension(extensionBundle);
                    mData.put(ext.id, ext);
                    return ext;
                  });

      return pending;
    }

    public void setObserver(final Observer observer) {
      mObserver = observer;
    }

    public void remove(final String id) {
      mData.remove(id);
    }

    /**
     * Add this extension to the store and update it's current value if it's already present.
     *
     * @param id the {@link WebExtension} id.
     * @param extension the {@link WebExtension} to add to the store.
     */
    public void update(final String id, final WebExtension extension) {
      mData.put(id, extension);
    }
  }

  private ExtensionStore mExtensions = new ExtensionStore();

  private Internals mInternals = new Internals();

  // Avoids exposing listeners to the API
  private class Internals implements BundleEventListener, ExtensionStore.Observer {
    @Override
    // BundleEventListener
    public void handleMessage(
        final String event, final GeckoBundle message, final EventCallback callback) {
      WebExtensionController.this.handleMessage(event, message, callback, null);
    }

    @Override
    public WebExtension onNewExtension(final GeckoBundle bundle) {
      return WebExtension.fromBundle(mDelegateControllerProvider, bundle);
    }
  }

  /* package */ void releasePendingMessages(
      final WebExtension extension, final String nativeApp, final GeckoSession session) {
    Log.i(
        LOGTAG,
        "releasePendingMessages:"
            + " extension="
            + extension.id
            + " nativeApp="
            + nativeApp
            + " session="
            + session);
    final List<Message> messages =
        mPendingMessages.remove(new MessageRecipient(nativeApp, extension.id, session));
    if (messages == null) {
      return;
    }

    for (final Message message : messages) {
      WebExtensionController.this.handleMessage(
          message.event, message.bundle, message.callback, message.session);
    }
  }

  private class DelegateController implements WebExtension.DelegateController {
    private final WebExtension mExtension;

    public DelegateController(final WebExtension extension) {
      mExtension = extension;
    }

    @Override
    public void onMessageDelegate(
        final String nativeApp, final WebExtension.MessageDelegate delegate) {
      mListener.setMessageDelegate(mExtension, delegate, nativeApp);
    }

    @Override
    public void onActionDelegate(final WebExtension.ActionDelegate delegate) {
      mListener.setActionDelegate(mExtension, delegate);
    }

    @Override
    public WebExtension.ActionDelegate getActionDelegate() {
      return mListener.getActionDelegate(mExtension);
    }

    @Override
    public void onBrowsingDataDelegate(final WebExtension.BrowsingDataDelegate delegate) {
      mListener.setBrowsingDataDelegate(mExtension, delegate);

      for (final Message message : mPendingBrowsingData.get(mExtension.id)) {
        WebExtensionController.this.handleMessage(
            message.event, message.bundle, message.callback, message.session);
      }

      mPendingBrowsingData.remove(mExtension.id);
    }

    @Override
    public WebExtension.BrowsingDataDelegate getBrowsingDataDelegate() {
      return mListener.getBrowsingDataDelegate(mExtension);
    }

    @Override
    public void onTabDelegate(final WebExtension.TabDelegate delegate) {
      mListener.setTabDelegate(mExtension, delegate);

      for (final Message message : mPendingNewTab.get(mExtension.id)) {
        WebExtensionController.this.handleMessage(
            message.event, message.bundle, message.callback, message.session);
      }

      mPendingNewTab.remove(mExtension.id);
    }

    @Override
    public WebExtension.TabDelegate getTabDelegate() {
      return mListener.getTabDelegate(mExtension);
    }

    @Override
    public void onDownloadDelegate(final WebExtension.DownloadDelegate delegate) {
      mListener.setDownloadDelegate(mExtension, delegate);

      for (final Message message : mPendingDownload.get(mExtension.id)) {
        WebExtensionController.this.handleMessage(
            message.event, message.bundle, message.callback, message.session);
      }

      mPendingDownload.remove(mExtension.id);
    }

    @Override
    public WebExtension.DownloadDelegate getDownloadDelegate() {
      return mListener.getDownloadDelegate(mExtension);
    }
  }

  final WebExtension.DelegateControllerProvider mDelegateControllerProvider =
      new WebExtension.DelegateControllerProvider() {
        @Override
        public WebExtension.DelegateController controllerFor(final WebExtension extension) {
          return new DelegateController(extension);
        }
      };

  /**
   * This delegate will be called whenever an extension is about to be installed or it needs new
   * permissions, e.g during an update or because it called <code>permissions.request</code>
   */
  @UiThread
  public interface PromptDelegate {
    /**
     * Called whenever a new extension is being installed. This is intended as an opportunity for
     * the app to prompt the user for the permissions required by this extension.
     *
     * @param extension The {@link WebExtension} that is about to be installed. You can use {@link
     *     WebExtension#metaData} to gather information about this extension when building the user
     *     prompt dialog.
     * @return A {@link GeckoResult} that completes to either {@link AllowOrDeny#ALLOW ALLOW} if
     *     this extension should be installed or {@link AllowOrDeny#DENY DENY} if this extension
     *     should not be installed. A null value will be interpreted as {@link AllowOrDeny#DENY
     *     DENY}.
     */
    @Nullable
    default GeckoResult<AllowOrDeny> onInstallPrompt(final @NonNull WebExtension extension) {
      return null;
    }

    /**
     * Called whenever an updated extension has new permissions. This is intended as an opportunity
     * for the app to prompt the user for the new permissions required by this extension.
     *
     * @param currentlyInstalled The {@link WebExtension} that is currently installed.
     * @param updatedExtension The {@link WebExtension} that will replace the previous extension.
     * @param newPermissions The new permissions that are needed.
     * @param newOrigins The new origins that are needed.
     * @return A {@link GeckoResult} that completes to either {@link AllowOrDeny#ALLOW ALLOW} if
     *     this extension should be update or {@link AllowOrDeny#DENY DENY} if this extension should
     *     not be update. A null value will be interpreted as {@link AllowOrDeny#DENY DENY}.
     */
    @Nullable
    default GeckoResult<AllowOrDeny> onUpdatePrompt(
        @NonNull final WebExtension currentlyInstalled,
        @NonNull final WebExtension updatedExtension,
        @NonNull final String[] newPermissions,
        @NonNull final String[] newOrigins) {
      return null;
    }

    /*
    TODO: Bug 1601420
    default GeckoResult<AllowOrDeny> onOptionalPrompt(
            WebExtension extension,
            String[] optionalPermissions) {
        return null;
    } */
  }

  public interface DebuggerDelegate {
    /**
     * Called whenever the list of installed extensions has been modified using the debugger with
     * tools like web-ext.
     *
     * <p>This is intended as an opportunity to refresh the list of installed extensions using
     * {@link WebExtensionController#list} and to set delegates on the new {@link WebExtension}
     * objects, e.g. using {@link WebExtension#setActionDelegate} and {@link
     * WebExtension#setMessageDelegate}.
     *
     * @see <a
     *     href="https://extensionworkshop.com/documentation/develop/getting-started-with-web-ext">
     *     Getting started with web-ext</a>
     */
    @UiThread
    default void onExtensionListUpdated() {}
  }

  /**
   * @return the current {@link PromptDelegate} instance.
   * @see PromptDelegate
   */
  @UiThread
  @Nullable
  public PromptDelegate getPromptDelegate() {
    return mPromptDelegate;
  }

  /**
   * Set the {@link PromptDelegate} for this instance. This delegate will be used to be notified
   * whenever an extension is being installed or needs new permissions.
   *
   * @param delegate the delegate instance.
   * @see PromptDelegate
   */
  @UiThread
  public void setPromptDelegate(final @Nullable PromptDelegate delegate) {
    if (delegate == null && mPromptDelegate != null) {
      EventDispatcher.getInstance()
          .unregisterUiThreadListener(
              mInternals,
              "GeckoView:WebExtension:InstallPrompt",
              "GeckoView:WebExtension:UpdatePrompt",
              "GeckoView:WebExtension:OptionalPrompt");
    } else if (delegate != null && mPromptDelegate == null) {
      EventDispatcher.getInstance()
          .registerUiThreadListener(
              mInternals,
              "GeckoView:WebExtension:InstallPrompt",
              "GeckoView:WebExtension:UpdatePrompt",
              "GeckoView:WebExtension:OptionalPrompt");
    }

    mPromptDelegate = delegate;
  }

  /**
   * Set the {@link DebuggerDelegate} for this instance. This delegate will receive updates about
   * extension changes using developer tools.
   *
   * @param delegate the Delegate instance
   */
  @UiThread
  public void setDebuggerDelegate(final @NonNull DebuggerDelegate delegate) {
    if (delegate == null && mDebuggerDelegate != null) {
      EventDispatcher.getInstance()
          .unregisterUiThreadListener(mInternals, "GeckoView:WebExtension:DebuggerListUpdated");
    } else if (delegate != null && mDebuggerDelegate == null) {
      EventDispatcher.getInstance()
          .registerUiThreadListener(mInternals, "GeckoView:WebExtension:DebuggerListUpdated");
    }

    mDebuggerDelegate = delegate;
  }

  private static class InstallCanceller implements GeckoResult.CancellationDelegate {
    public final String installId;

    public InstallCanceller() {
      installId = UUID.randomUUID().toString();
    }

    @Override
    public GeckoResult<Boolean> cancel() {
      final GeckoBundle bundle = new GeckoBundle(1);
      bundle.putString("installId", installId);

      return EventDispatcher.getInstance()
          .queryBundle("GeckoView:WebExtension:CancelInstall", bundle)
          .map(response -> response.getBoolean("cancelled"));
    }
  }

  /**
   * Install an extension.
   *
   * <p>An installed extension will persist and will be available even when restarting the {@link
   * GeckoRuntime}.
   *
   * <p>Installed extensions through this method need to be signed by Mozilla, see <a
   * href="https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/#distributing-your-addon">
   * Distributing your add-on </a>.
   *
   * <p>When calling this method, the GeckoView library will download the extension, validate its
   * manifest and signature, and give you an opportunity to verify its permissions through {@link
   * PromptDelegate#installPrompt}, you can use this method to prompt the user if appropriate.
   *
   * @param uri URI to the extension's <code>.xpi</code> package. This can be a remote <code>https:
   *     </code> URI or a local <code>file:</code> or <code>resource:</code> URI. Note: the app
   *     needs the appropriate permissions for local URIs.
   * @return A {@link GeckoResult} that will complete when the installation process finishes. For
   *     successful installations, the GeckoResult will return the {@link WebExtension} object that
   *     you can use to set delegates and retrieve information about the WebExtension using {@link
   *     WebExtension#metaData}.
   *     <p>If an error occurs during the installation process, the GeckoResult will complete
   *     exceptionally with a {@link WebExtension.InstallException InstallException} that will
   *     contain the relevant error code in {@link WebExtension.InstallException#code
   *     InstallException#code}.
   * @see PromptDelegate#installPrompt
   * @see WebExtension.InstallException.ErrorCodes
   * @see WebExtension#metaData
   */
  @NonNull
  @AnyThread
  public GeckoResult<WebExtension> install(final @NonNull String uri) {
    final InstallCanceller canceller = new InstallCanceller();
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("locationUri", uri);
    bundle.putString("installId", canceller.installId);

    final GeckoResult<WebExtension> result =
        EventDispatcher.getInstance()
            .queryBundle("GeckoView:WebExtension:Install", bundle)
            .map(
                ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext),
                WebExtension.InstallException::fromQueryException)
            .map(this::registerWebExtension);
    result.setCancellationDelegate(canceller);
    return result;
  }

  /**
   * Set whether an extension should be allowed to run in private browsing or not.
   *
   * @param extension the {@link WebExtension} instance to modify.
   * @param allowed true if this extension should be allowed to run in private browsing pages, false
   *     otherwise.
   * @return the updated {@link WebExtension} instance.
   */
  @NonNull
  @AnyThread
  public GeckoResult<WebExtension> setAllowedInPrivateBrowsing(
      final @NonNull WebExtension extension, final boolean allowed) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("extensionId", extension.id);
    bundle.putBoolean("allowed", allowed);

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:SetPBAllowed", bundle)
        .map(ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext))
        .map(this::registerWebExtension);
  }

  /**
   * Install a built-in extension.
   *
   * <p>Built-in extensions have access to native messaging, don't need to be signed and are
   * installed from a folder in the APK instead of a .xpi bundle.
   *
   * <p>Example:
   *
   * <p><code>
   *    controller.installBuiltIn("resource://android/assets/example/");
   * </code> Will install the built-in extension located at <code>/assets/example/</code> in the
   * app's APK.
   *
   * @param uri Folder where the extension is located. To ensure this folder is inside the APK, only
   *     <code>resource://android</code> URIs are allowed.
   * @see WebExtension.MessageDelegate
   * @return A {@link GeckoResult} that completes with the extension once it's installed.
   */
  @NonNull
  @AnyThread
  public GeckoResult<WebExtension> installBuiltIn(final @NonNull String uri) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("locationUri", uri);

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:InstallBuiltIn", bundle)
        .map(
            ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext),
            WebExtension.InstallException::fromQueryException)
        .map(this::registerWebExtension);
  }

  /**
   * Ensure that a built-in extension is installed.
   *
   * <p>Similar to {@link #installBuiltIn}, except the extension is not re-installed if it's already
   * present and it has the same version.
   *
   * <p>Example:
   *
   * <p><code>
   *    controller.ensureBuiltIn("resource://android/assets/example/", "example@example.com");
   * </code> Will install the built-in extension located at <code>/assets/example/</code> in the
   * app's APK.
   *
   * @param uri Folder where the extension is located. To ensure this folder is inside the APK, only
   *     <code>resource://android</code> URIs are allowed.
   * @param id Extension ID as present in the manifest.json file.
   * @see WebExtension.MessageDelegate
   * @return A {@link GeckoResult} that completes with the extension once it's installed.
   */
  @NonNull
  @AnyThread
  public GeckoResult<WebExtension> ensureBuiltIn(
      final @NonNull String uri, final @Nullable String id) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("locationUri", uri);
    bundle.putString("webExtensionId", id);

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:EnsureBuiltIn", bundle)
        .map(
            ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext),
            WebExtension.InstallException::fromQueryException)
        .map(this::registerWebExtension);
  }

  /**
   * Uninstall an extension.
   *
   * <p>Uninstalling an extension will remove it from the current {@link GeckoRuntime} instance,
   * delete all its data and trigger a request to close all extension pages currently open.
   *
   * @param extension The {@link WebExtension} to be uninstalled.
   * @return A {@link GeckoResult} that will complete when the uninstall process is completed.
   */
  @NonNull
  @AnyThread
  public GeckoResult<Void> uninstall(final @NonNull WebExtension extension) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("webExtensionId", extension.id);

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:Uninstall", bundle)
        .accept(result -> unregisterWebExtension(extension));
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({EnableSource.USER, EnableSource.APP})
  @interface EnableSources {}

  /**
   * Contains the possible values for the <code>source</code> parameter in {@link #enable} and
   * {@link #disable}.
   */
  public static class EnableSource {
    /** Action has been requested by the user. */
    public static final int USER = 1;

    /**
     * Action requested by the app itself, e.g. to disable an extension that is not supported in
     * this version of the app.
     */
    public static final int APP = 2;

    static String toString(final @EnableSources int flag) {
      if (flag == USER) {
        return "user";
      } else if (flag == APP) {
        return "app";
      } else {
        throw new IllegalArgumentException("Value provided in flags is not valid.");
      }
    }
  }

  /**
   * Enable an extension that has been disabled. If the extension is already enabled, this method
   * has no effect.
   *
   * @param extension The {@link WebExtension} to be enabled.
   * @param source The agent that initiated this action, e.g. if the action has been initiated by
   *     the user,use {@link EnableSource#USER}.
   * @return the new {@link WebExtension} instance, updated to reflect the enablement.
   */
  @AnyThread
  @NonNull
  public GeckoResult<WebExtension> enable(
      final @NonNull WebExtension extension, final @EnableSources int source) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("webExtensionId", extension.id);
    bundle.putString("source", EnableSource.toString(source));

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:Enable", bundle)
        .map(ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext))
        .map(this::registerWebExtension);
  }

  /**
   * Disable an extension that is enabled. If the extension is already disabled, this method has no
   * effect.
   *
   * @param extension The {@link WebExtension} to be disabled.
   * @param source The agent that initiated this action, e.g. if the action has been initiated by
   *     the user, use {@link EnableSource#USER}.
   * @return the new {@link WebExtension} instance, updated to reflect the disablement.
   */
  @AnyThread
  @NonNull
  public GeckoResult<WebExtension> disable(
      final @NonNull WebExtension extension, final @EnableSources int source) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("webExtensionId", extension.id);
    bundle.putString("source", EnableSource.toString(source));

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:Disable", bundle)
        .map(ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext))
        .map(this::registerWebExtension);
  }

  private List<WebExtension> listFromBundle(final GeckoBundle response) {
    final GeckoBundle[] bundles = response.getBundleArray("extensions");
    final List<WebExtension> list = new ArrayList<>(bundles.length);

    for (final GeckoBundle bundle : bundles) {
      final WebExtension extension = new WebExtension(mDelegateControllerProvider, bundle);
      list.add(registerWebExtension(extension));
    }

    return list;
  }

  /**
   * List installed extensions for this {@link GeckoRuntime}.
   *
   * <p>The returned list can be used to set delegates on the {@link WebExtension} objects using
   * {@link WebExtension#setActionDelegate}, {@link WebExtension#setMessageDelegate}.
   *
   * @return a {@link GeckoResult} that will resolve when the list of extensions is available.
   */
  @AnyThread
  @NonNull
  public GeckoResult<List<WebExtension>> list() {
    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:List")
        .map(this::listFromBundle);
  }

  /**
   * Update a web extension.
   *
   * <p>When checking for an update, GeckoView will download the update manifest that is defined by
   * the web extension's manifest property <a
   * href="https://extensionworkshop.com/documentation/manage/updating-your-extension/">browser_specific_settings.gecko.update_url</a>.
   * If an update is found it will be downloaded and installed. If the extension needs any new
   * permissions the {@link PromptDelegate#updatePrompt} will be triggered.
   *
   * <p>More information about the update manifest format is available <a
   * href="https://extensionworkshop.com/documentation/manage/updating-your-extension/#manifest-structure">here</a>.
   *
   * @param extension The extension to update.
   * @return A {@link GeckoResult} that will complete when the update process finishes. If an update
   *     is found and installed successfully, the GeckoResult will return the updated {@link
   *     WebExtension}. If no update is available, null will be returned. If the updated extension
   *     requires new permissions, the {@link PromptDelegate#installPrompt} will be called.
   * @see PromptDelegate#updatePrompt
   */
  @AnyThread
  @NonNull
  public GeckoResult<WebExtension> update(final @NonNull WebExtension extension) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("webExtensionId", extension.id);

    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:WebExtension:Update", bundle)
        .map(
            ext -> WebExtension.fromBundle(mDelegateControllerProvider, ext),
            WebExtension.InstallException::fromQueryException)
        .map(this::registerWebExtension);
  }

  /* package */ WebExtensionController(final GeckoRuntime runtime) {
    mListener = new WebExtension.Listener<>(runtime);
    mPendingMessages = new MultiMap<>();
    mPendingNewTab = new MultiMap<>();
    mPendingBrowsingData = new MultiMap<>();
    mPendingDownload = new MultiMap<>();
    mExtensions.setObserver(mInternals);
    mDownloads = new SparseArray<>();
  }

  /* package */ WebExtension registerWebExtension(final WebExtension webExtension) {
    if (webExtension != null) {
      mExtensions.update(webExtension.id, webExtension);
    }
    return webExtension;
  }

  /* package */ void handleMessage(
      final String event,
      final GeckoBundle bundle,
      final EventCallback callback,
      final GeckoSession session) {
    final Message message = new Message(event, bundle, callback, session);

    Log.d(LOGTAG, "handleMessage " + event);

    if ("GeckoView:WebExtension:InstallPrompt".equals(event)) {
      installPrompt(bundle, callback);
      return;
    } else if ("GeckoView:WebExtension:UpdatePrompt".equals(event)) {
      updatePrompt(bundle, callback);
      return;
    } else if ("GeckoView:WebExtension:DebuggerListUpdated".equals(event)) {
      if (mDebuggerDelegate != null) {
        mDebuggerDelegate.onExtensionListUpdated();
      }
      return;
    }

    extensionFromBundle(bundle)
        .accept(
            extension -> {
              if ("GeckoView:WebExtension:NewTab".equals(event)) {
                newTab(message, extension);
                return;
              } else if ("GeckoView:WebExtension:UpdateTab".equals(event)) {
                updateTab(message, extension);
                return;
              } else if ("GeckoView:WebExtension:CloseTab".equals(event)) {
                closeTab(message, extension);
                return;
              } else if ("GeckoView:BrowserAction:Update".equals(event)) {
                actionUpdate(message, extension, WebExtension.Action.TYPE_BROWSER_ACTION);
                return;
              } else if ("GeckoView:PageAction:Update".equals(event)) {
                actionUpdate(message, extension, WebExtension.Action.TYPE_PAGE_ACTION);
                return;
              } else if ("GeckoView:BrowserAction:OpenPopup".equals(event)) {
                openPopup(message, extension, WebExtension.Action.TYPE_BROWSER_ACTION);
                return;
              } else if ("GeckoView:PageAction:OpenPopup".equals(event)) {
                openPopup(message, extension, WebExtension.Action.TYPE_PAGE_ACTION);
                return;
              } else if ("GeckoView:WebExtension:OpenOptionsPage".equals(event)) {
                openOptionsPage(message, extension);
                return;
              } else if ("GeckoView:BrowsingData:GetSettings".equals(event)) {
                getSettings(message, extension);
                return;
              } else if ("GeckoView:BrowsingData:Clear".equals(event)) {
                browsingDataClear(message, extension);
                return;
              } else if ("GeckoView:WebExtension:Download".equals(event)) {
                download(message, extension);
                return;
              }

              // GeckoView:WebExtension:Connect and GeckoView:WebExtension:Message
              // are handled below.
              final String nativeApp = bundle.getString("nativeApp");
              if (nativeApp == null) {
                if (BuildConfig.DEBUG) {
                  throw new RuntimeException("Missing required nativeApp message parameter.");
                }
                callback.sendError("Missing nativeApp parameter.");
                return;
              }

              final GeckoBundle senderBundle = bundle.getBundle("sender");
              final WebExtension.MessageSender sender =
                  fromBundle(extension, senderBundle, session);
              if (sender == null) {
                if (callback != null) {
                  if (BuildConfig.DEBUG) {
                    try {
                      Log.e(
                          LOGTAG, "Could not find recipient for message: " + bundle.toJSONObject());
                    } catch (final JSONException ex) {
                    }
                  }
                  callback.sendError("Could not find recipient for " + bundle.getBundle("sender"));
                }
                return;
              }

              if ("GeckoView:WebExtension:Connect".equals(event)) {
                connect(nativeApp, bundle.getLong("portId", -1), message, sender);
              } else if ("GeckoView:WebExtension:Message".equals(event)) {
                message(nativeApp, message, sender);
              }
            });
  }

  private void installPrompt(final GeckoBundle message, final EventCallback callback) {
    final GeckoBundle extensionBundle = message.getBundle("extension");
    if (extensionBundle == null
        || !extensionBundle.containsKey("webExtensionId")
        || !extensionBundle.containsKey("locationURI")) {
      if (BuildConfig.DEBUG) {
        throw new RuntimeException("Missing webExtensionId or locationURI");
      }

      Log.e(LOGTAG, "Missing webExtensionId or locationURI");
      return;
    }

    final WebExtension extension = new WebExtension(mDelegateControllerProvider, extensionBundle);

    if (mPromptDelegate == null) {
      Log.e(
          LOGTAG, "Tried to install extension " + extension.id + " but no delegate is registered");
      return;
    }

    final GeckoResult<AllowOrDeny> promptResponse = mPromptDelegate.onInstallPrompt(extension);
    if (promptResponse == null) {
      return;
    }

    callback.resolveTo(
        promptResponse.map(
            allowOrDeny -> {
              final GeckoBundle response = new GeckoBundle(1);
              response.putBoolean("allow", AllowOrDeny.ALLOW.equals(allowOrDeny));
              return response;
            }));
  }

  private void updatePrompt(final GeckoBundle message, final EventCallback callback) {
    final GeckoBundle currentBundle = message.getBundle("currentlyInstalled");
    final GeckoBundle updatedBundle = message.getBundle("updatedExtension");
    final String[] newPermissions = message.getStringArray("newPermissions");
    final String[] newOrigins = message.getStringArray("newOrigins");
    if (currentBundle == null || updatedBundle == null) {
      if (BuildConfig.DEBUG) {
        throw new RuntimeException("Missing bundle");
      }

      Log.e(LOGTAG, "Missing bundle");
      return;
    }

    final WebExtension currentExtension =
        new WebExtension(mDelegateControllerProvider, currentBundle);

    final WebExtension updatedExtension =
        new WebExtension(mDelegateControllerProvider, updatedBundle);

    if (mPromptDelegate == null) {
      Log.e(
          LOGTAG,
          "Tried to update extension " + currentExtension.id + " but no delegate is registered");
      return;
    }

    final GeckoResult<AllowOrDeny> promptResponse =
        mPromptDelegate.onUpdatePrompt(
            currentExtension, updatedExtension, newPermissions, newOrigins);
    if (promptResponse == null) {
      return;
    }

    callback.resolveTo(
        promptResponse.map(
            allowOrDeny -> {
              final GeckoBundle response = new GeckoBundle(1);
              response.putBoolean("allow", AllowOrDeny.ALLOW.equals(allowOrDeny));
              return response;
            }));
  }

  private void getSettings(final Message message, final WebExtension extension) {
    final WebExtension.BrowsingDataDelegate delegate = mListener.getBrowsingDataDelegate(extension);
    if (delegate == null) {
      mPendingBrowsingData.add(extension.id, message);
      return;
    }

    final GeckoResult<WebExtension.BrowsingDataDelegate.Settings> settingsResult =
        delegate.onGetSettings();
    if (settingsResult == null) {
      message.callback.sendError("browsingData.settings is not supported");
      return;
    }
    message.callback.resolveTo(settingsResult.map(settings -> settings.toGeckoBundle()));
  }

  private void browsingDataClear(final Message message, final WebExtension extension) {
    final WebExtension.BrowsingDataDelegate delegate = mListener.getBrowsingDataDelegate(extension);
    if (delegate == null) {
      mPendingBrowsingData.add(extension.id, message);
      return;
    }

    final long unixTimestamp = message.bundle.getLong("since");
    final String dataType = message.bundle.getString("dataType");

    final GeckoResult<Void> response;
    if ("downloads".equals(dataType)) {
      response = delegate.onClearDownloads(unixTimestamp);
    } else if ("formData".equals(dataType)) {
      response = delegate.onClearFormData(unixTimestamp);
    } else if ("history".equals(dataType)) {
      response = delegate.onClearHistory(unixTimestamp);
    } else if ("passwords".equals(dataType)) {
      response = delegate.onClearPasswords(unixTimestamp);
    } else {
      throw new IllegalStateException("Illegal clear data type: " + dataType);
    }

    message.callback.resolveTo(response);
  }

  /* package */ void download(final Message message, final WebExtension extension) {
    final WebExtension.DownloadDelegate delegate = mListener.getDownloadDelegate(extension);
    if (delegate == null) {
      mPendingDownload.add(extension.id, message);
      return;
    }

    final GeckoBundle optionsBundle = message.bundle.getBundle("options");

    final WebExtension.DownloadRequest request =
        WebExtension.DownloadRequest.fromBundle(optionsBundle);

    final GeckoResult<WebExtension.DownloadInitData> result =
        delegate.onDownload(extension, request);
    if (result == null) {
      message.callback.sendError("downloads.download is not supported");
      return;
    }

    message.callback.resolveTo(
        result.map(
            value -> {
              if (value == null) {
                Log.e(LOGTAG, "onDownload returned invalid null value");
                throw new IllegalArgumentException("downloads.download is not supported");
              }

              final GeckoBundle returnMessage =
                  WebExtension.Download.downloadInfoToBundle(value.initData);
              returnMessage.putInt("id", value.download.id);

              return returnMessage;
            }));
  }

  /* package */ void openOptionsPage(final Message message, final WebExtension extension) {
    final GeckoBundle bundle = message.bundle;
    final WebExtension.TabDelegate delegate = mListener.getTabDelegate(extension);

    if (delegate != null) {
      delegate.onOpenOptionsPage(extension);
    } else {
      message.callback.sendError("runtime.openOptionsPage is not supported");
    }

    message.callback.sendSuccess(null);
  }

  /* package */ void newTab(final Message message, final WebExtension extension) {
    final GeckoBundle bundle = message.bundle;

    final WebExtension.TabDelegate delegate = mListener.getTabDelegate(extension);
    final WebExtension.CreateTabDetails details =
        new WebExtension.CreateTabDetails(bundle.getBundle("createProperties"));

    final GeckoResult<GeckoSession> result;
    if (delegate != null) {
      result = delegate.onNewTab(extension, details);
    } else {
      mPendingNewTab.add(extension.id, message);
      return;
    }

    if (result == null) {
      message.callback.sendSuccess(false);
      return;
    }

    final String newSessionId = message.bundle.getString("newSessionId");
    message.callback.resolveTo(
        result.map(
            session -> {
              if (session == null) {
                return false;
              }

              if (session.isOpen()) {
                throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
              }

              session.open(mListener.runtime, newSessionId);
              return true;
            }));
  }

  /* package */ void updateTab(final Message message, final WebExtension extension) {
    final WebExtension.SessionTabDelegate delegate =
        message.session.getWebExtensionController().getTabDelegate(extension);
    final EventCallback callback = message.callback;

    if (delegate == null) {
      callback.sendError("tabs.update is not supported");
      return;
    }

    final WebExtension.UpdateTabDetails details =
        new WebExtension.UpdateTabDetails(message.bundle.getBundle("updateProperties"));
    callback.resolveTo(
        delegate
            .onUpdateTab(extension, message.session, details)
            .map(
                value -> {
                  if (value == AllowOrDeny.ALLOW) {
                    return null;
                  } else {
                    throw new Exception("tabs.update is not supported");
                  }
                }));
  }

  /* package */ void closeTab(final Message message, final WebExtension extension) {
    final WebExtension.SessionTabDelegate delegate =
        message.session.getWebExtensionController().getTabDelegate(extension);

    final GeckoResult<AllowOrDeny> result;
    if (delegate != null) {
      result = delegate.onCloseTab(extension, message.session);
    } else {
      result = GeckoResult.fromValue(AllowOrDeny.DENY);
    }

    message.callback.resolveTo(
        result.map(
            value -> {
              if (value == AllowOrDeny.ALLOW) {
                return null;
              } else {
                throw new Exception("tabs.remove is not supported");
              }
            }));
  }

  /**
   * Notifies extensions about a active tab change over the `tabs.onActivated` event.
   *
   * @param session The {@link GeckoSession} of the newly selected session/tab.
   * @param active true if the tab became active, false if the tab became inactive.
   */
  @AnyThread
  public void setTabActive(@NonNull final GeckoSession session, final boolean active) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putBoolean("active", active);
    session.getEventDispatcher().dispatch("GeckoView:WebExtension:SetTabActive", bundle);
  }

  /* package */ void unregisterWebExtension(final WebExtension webExtension) {
    mExtensions.remove(webExtension.id);
    mListener.unregisterWebExtension(webExtension);
  }

  private WebExtension.MessageSender fromBundle(
      final WebExtension extension, final GeckoBundle sender, final GeckoSession session) {
    if (extension == null) {
      // All senders should have an extension
      return null;
    }

    final String envType = sender.getString("envType");
    @WebExtension.MessageSender.EnvType final int environmentType;

    if ("content_child".equals(envType)) {
      environmentType = WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT;
    } else if ("addon_child".equals(envType)) {
      // TODO Bug 1554277: check that this message is coming from the right process
      environmentType = WebExtension.MessageSender.ENV_TYPE_EXTENSION;
    } else {
      environmentType = WebExtension.MessageSender.ENV_TYPE_UNKNOWN;
    }

    if (environmentType == WebExtension.MessageSender.ENV_TYPE_UNKNOWN) {
      if (BuildConfig.DEBUG) {
        throw new RuntimeException("Missing or unknown envType.");
      }

      return null;
    }

    final String url = sender.getString("url");
    final boolean isTopLevel;
    if (session == null) {
      // This message is coming from the background page
      isTopLevel = true;
    } else {
      // If session is present we are either receiving this message from a content script or
      // an extension page, let's make sure we have the proper identification so that
      // embedders can check the origin of this message.
      if (!sender.containsKey("frameId")
          || !sender.containsKey("url")
          ||
          // -1 is an invalid frame id
          sender.getInt("frameId", -1) == -1) {
        if (BuildConfig.DEBUG) {
          throw new RuntimeException("Missing sender information.");
        }

        // This message does not have the proper identification and may be compromised,
        // let's ignore it.
        return null;
      }

      isTopLevel = sender.getInt("frameId", -1) == 0;
    }

    return new WebExtension.MessageSender(extension, session, url, environmentType, isTopLevel);
  }

  private WebExtension.MessageDelegate getDelegate(
      final String nativeApp,
      final WebExtension.MessageSender sender,
      final EventCallback callback) {
    if ((sender.webExtension.flags & WebExtension.Flags.ALLOW_CONTENT_MESSAGING) == 0
        && sender.environmentType == WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT) {
      callback.sendError("This NativeApp can't receive messages from Content Scripts.");
      return null;
    }

    WebExtension.MessageDelegate delegate = null;

    if (sender.session != null) {
      delegate =
          sender
              .session
              .getWebExtensionController()
              .getMessageDelegate(sender.webExtension, nativeApp);
    } else if (sender.environmentType == WebExtension.MessageSender.ENV_TYPE_EXTENSION) {
      delegate = mListener.getMessageDelegate(sender.webExtension, nativeApp);
    }

    return delegate;
  }

  private static class MessageRecipient {
    public final String webExtensionId;
    public final String nativeApp;
    public final GeckoSession session;

    public MessageRecipient(
        final String webExtensionId, final String nativeApp, final GeckoSession session) {
      this.webExtensionId = webExtensionId;
      this.nativeApp = nativeApp;
      this.session = session;
    }

    private static boolean equals(final Object a, final Object b) {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
        return Objects.equals(a, b);
      }

      return (a == b) || (a != null && a.equals(b));
    }

    @Override
    public boolean equals(final Object other) {
      if (!(other instanceof MessageRecipient)) {
        return false;
      }

      final MessageRecipient o = (MessageRecipient) other;
      return equals(webExtensionId, o.webExtensionId)
          && equals(nativeApp, o.nativeApp)
          && equals(session, o.session);
    }

    @Override
    public int hashCode() {
      return Arrays.hashCode(new Object[] {webExtensionId, nativeApp, session});
    }
  }

  private void connect(
      final String nativeApp,
      final long portId,
      final Message message,
      final WebExtension.MessageSender sender) {
    if (portId == -1) {
      message.callback.sendError("Missing portId.");
      return;
    }

    final WebExtension.Port port = new WebExtension.Port(nativeApp, portId, sender);

    final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender, message.callback);
    if (delegate == null) {
      mPendingMessages.add(
          new MessageRecipient(nativeApp, sender.webExtension.id, sender.session), message);
      return;
    }

    delegate.onConnect(port);
    message.callback.sendSuccess(true);
  }

  private void message(
      final String nativeApp, final Message message, final WebExtension.MessageSender sender) {
    final EventCallback callback = message.callback;

    final Object content;
    try {
      content = message.bundle.toJSONObject().get("data");
    } catch (final JSONException ex) {
      callback.sendError(ex.getMessage());
      return;
    }

    final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender, callback);
    if (delegate == null) {
      mPendingMessages.add(
          new MessageRecipient(nativeApp, sender.webExtension.id, sender.session), message);
      return;
    }

    final GeckoResult<Object> response = delegate.onMessage(nativeApp, content, sender);
    if (response == null) {
      callback.sendSuccess(null);
      return;
    }

    callback.resolveTo(response);
  }

  private GeckoResult<WebExtension> extensionFromBundle(final GeckoBundle message) {
    final String extensionId = message.getString("extensionId");
    return mExtensions.get(extensionId);
  }

  private void openPopup(
      final Message message,
      final WebExtension extension,
      final @WebExtension.Action.ActionType int actionType) {
    if (extension == null) {
      return;
    }

    final WebExtension.Action action =
        new WebExtension.Action(actionType, message.bundle.getBundle("action"), extension);
    final String popupUri = message.bundle.getString("popupUri");

    final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, message.session);
    if (delegate == null) {
      return;
    }

    final GeckoResult<GeckoSession> popup = delegate.onOpenPopup(extension, action);
    action.openPopup(popup, popupUri);
  }

  private WebExtension.ActionDelegate actionDelegateFor(
      final WebExtension extension, final GeckoSession session) {
    if (session == null) {
      return mListener.getActionDelegate(extension);
    }

    return session.getWebExtensionController().getActionDelegate(extension);
  }

  private void actionUpdate(
      final Message message,
      final WebExtension extension,
      final @WebExtension.Action.ActionType int actionType) {
    if (extension == null) {
      return;
    }

    final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, message.session);
    if (delegate == null) {
      return;
    }

    final WebExtension.Action action =
        new WebExtension.Action(actionType, message.bundle.getBundle("action"), extension);
    if (actionType == WebExtension.Action.TYPE_BROWSER_ACTION) {
      delegate.onBrowserAction(extension, message.session, action);
    } else if (actionType == WebExtension.Action.TYPE_PAGE_ACTION) {
      delegate.onPageAction(extension, message.session, action);
    }
  }

  // TODO: implement bug 1595822
  /* package */ static GeckoResult<List<WebExtension.Menu>> getMenu(
      final GeckoBundle menuArrayBundle) {
    return null;
  }

  @Nullable
  @UiThread
  public WebExtension.Download createDownload(final int id) {
    if (mDownloads.indexOfKey(id) >= 0) {
      throw new IllegalArgumentException("Download with this id already exists");
    } else {
      final WebExtension.Download download = new WebExtension.Download(id);
      mDownloads.put(id, download);

      return download;
    }
  }
}
