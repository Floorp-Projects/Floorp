/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test_runner;

import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.WebExtension;

// Receives API calls via mobile/shared/modules/test/AppUiTestDelegate.sys.mjs
// and forwards the calls to the Api impl.
//
// This interface allows JS/HTML-based mochitests to invoke test-only logic
// that is implemented by the embedder, e.g. by TestRunnerActivity.
//
// There is no implementation for xpcshell tests because the underlying concepts
// are not available to xpcshell on desktop Firefox. If there is ever a desire
// to create an instance, do so from XpcshellTestRunnerService.java.
//
// The supported messages are documented in AppTestDelegateParent.sys.mjs.
public class TestRunnerApiEngine implements WebExtension.MessageDelegate {
  private static final String LOGTAG = "TestRunnerAPI";

  public interface Api {
    GeckoResult<Void> clickBrowserAction(String extensionId);

    GeckoResult<Void> clickPageAction(String extensionId);

    GeckoResult<Void> closePopup();

    GeckoResult<Void> awaitExtensionPopup(String extensionId);
  }

  private final Api mImpl;

  public TestRunnerApiEngine(final Api impl) {
    mImpl = impl;
  }

  @SuppressWarnings("unchecked")
  private GeckoResult<Object> handleMessage(final JSONObject message) throws JSONException {
    final String type = message.getString("type");

    Log.i(LOGTAG, "Test API: " + type);

    if ("clickBrowserAction".equals(type)) {
      return (GeckoResult) mImpl.clickBrowserAction(message.getString("extensionId"));
    } else if ("clickPageAction".equals(type)) {
      return (GeckoResult) mImpl.clickPageAction(message.getString("extensionId"));
    } else if ("closeBrowserAction".equals(type)) {
      return (GeckoResult) mImpl.closePopup();
    } else if ("closePageAction".equals(type)) {
      return (GeckoResult) mImpl.closePopup();
    } else if ("awaitExtensionPanel".equals(type)) {
      return (GeckoResult) mImpl.awaitExtensionPopup(message.getString("extensionId"));
    }

    return GeckoResult.fromException(new RuntimeException("Unrecognized command " + type));
  }

  @Nullable
  @Override
  public GeckoResult<Object> onMessage(
      @NonNull final String nativeApp,
      @NonNull final Object message,
      @NonNull final WebExtension.MessageSender sender) {
    try {
      return handleMessage((JSONObject) message);
    } catch (final JSONException ex) {
      throw new RuntimeException(ex);
    }
  }
}
