package org.mozilla.geckoview.test;

import android.content.Context;
import android.content.Intent;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.io.File;
import java.util.List;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSession.PermissionDelegate;
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission;
import org.mozilla.geckoview.GeckoSessionSettings;

public class TrackingPermissionService extends TestRuntimeService {
  public static final int MESSAGE_SET_TRACKING_PERMISSION = FIRST_SAFE_MESSAGE + 1;
  public static final int MESSAGE_SET_PRIVATE_BROWSING_TRACKING_PERMISSION = FIRST_SAFE_MESSAGE + 2;
  public static final int MESSAGE_GET_TRACKING_PERMISSION = FIRST_SAFE_MESSAGE + 3;

  private ContentPermission mContentPermission;

  @Override
  protected GeckoSession createSession(final Intent intent) {
    return new GeckoSession(
        new GeckoSessionSettings.Builder()
            .usePrivateMode(mTestData.getBoolean("privateMode"))
            .build());
  }

  @Override
  protected void onSessionReady(final GeckoSession session) {
    session.setNavigationDelegate(
        new GeckoSession.NavigationDelegate() {
          @Override
          public void onLocationChange(
              final @NonNull GeckoSession session,
              final @Nullable String url,
              final @NonNull List<ContentPermission> perms,
              final @NonNull Boolean hasUserGesture) {
            for (ContentPermission perm : perms) {
              if (perm.permission == PermissionDelegate.PERMISSION_TRACKING) {
                mContentPermission = perm;
              }
            }
          }
        });
  }

  @Override
  protected GeckoResult<GeckoBundle> handleMessage(final int messageId, final GeckoBundle data) {
    if (mContentPermission == null) {
      throw new IllegalStateException("Content permission not received yet!");
    }

    switch (messageId) {
      case MESSAGE_SET_TRACKING_PERMISSION:
        {
          final int permission = data.getInt("trackingPermission");
          mRuntime.getStorageController().setPermission(mContentPermission, permission);
          break;
        }
      case MESSAGE_SET_PRIVATE_BROWSING_TRACKING_PERMISSION:
        {
          final int permission = data.getInt("trackingPermission");
          mRuntime
              .getStorageController()
              .setPrivateBrowsingPermanentPermission(mContentPermission, permission);
          break;
        }
      case MESSAGE_GET_TRACKING_PERMISSION:
        {
          final GeckoBundle result = new GeckoBundle(1);
          result.putInt("trackingPermission", mContentPermission.value);
          return GeckoResult.fromValue(result);
        }
    }

    return null;
  }

  public static class TrackingPermissionInstance
      extends RuntimeInstance<TrackingPermissionService> {
    public static GeckoBundle testData(boolean privateMode) {
      GeckoBundle testData = new GeckoBundle(1);
      testData.putBoolean("privateMode", privateMode);
      return testData;
    }

    private TrackingPermissionInstance(
        final Context context, final File profileFolder, final boolean privateMode) {
      super(context, TrackingPermissionService.class, profileFolder, testData(privateMode));
    }

    public static TrackingPermissionInstance start(
        final Context context, final File profileFolder, final boolean privateMode) {
      TrackingPermissionInstance instance =
          new TrackingPermissionInstance(context, profileFolder, privateMode);
      instance.sendIntent();
      return instance;
    }

    public GeckoResult<Integer> getTrackingPermission() {
      return query(MESSAGE_GET_TRACKING_PERMISSION)
          .map(bundle -> bundle.getInt("trackingPermission"));
    }

    public void setTrackingPermission(final int permission) {
      final GeckoBundle bundle = new GeckoBundle(1);
      bundle.putInt("trackingPermission", permission);
      sendMessage(MESSAGE_SET_TRACKING_PERMISSION, bundle);
    }

    public void setPrivateBrowsingPermanentTrackingPermission(final int permission) {
      final GeckoBundle bundle = new GeckoBundle(1);
      bundle.putInt("trackingPermission", permission);
      sendMessage(MESSAGE_SET_PRIVATE_BROWSING_TRACKING_PERMISSION, bundle);
    }
  }
}
