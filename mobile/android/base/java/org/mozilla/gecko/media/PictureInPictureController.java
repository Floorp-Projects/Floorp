package org.mozilla.gecko.media;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Icon;
import android.support.annotation.NonNull;
import android.util.Log;
import android.view.accessibility.AccessibilityManager;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BuildConfig;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.ArrayList;
import java.util.List;

@SuppressLint("NewApi")
public class PictureInPictureController implements BundleEventListener {
    public static final String LOGTAG = "PictureInPictureController";

    private final Activity pipActivity;
    private boolean isInPipMode;

    public PictureInPictureController(Activity activity) {
        pipActivity = activity;
    }

    /**
     * If the feature is supported and media is playing in fullscreen will try to activate
     * Picture In Picture mode for the current activity.
     * @throws IllegalStateException if entering Picture In Picture mode was not possible.
     */
    public void tryEnteringPictureInPictureMode() throws IllegalStateException {
        if (shouldTryPipMode()) {
            EventDispatcher.getInstance().registerUiThreadListener(this, "MediaControlService:MediaPlayingStatus");
            pipActivity.enterPictureInPictureMode(getPipParams(isMediaPlaying()));
            isInPipMode = true;
        }
    }

    public void cleanResources() {
        if (isInPipMode) {
            EventDispatcher.getInstance().unregisterUiThreadListener(this, "MediaControlService:MediaPlayingStatus");
            isInPipMode = false;
        }
    }

    public boolean isInPipMode() {
        return isInPipMode;
    }

    private boolean shouldTryPipMode() {
        if (!AppConstants.Versions.feature26Plus) {
            logDebugMessage("Picture In Picture is only available on Oreo+");
            return false;
        }

        if (pipActivity.isChangingConfigurations()) {
            logDebugMessage("Activity is being restarted");
            return false;
        }

        if (pipActivity.isFinishing()) {
            logDebugMessage("Activity is finishing");
            return false;
        }

        // PIP might be disabled on devices that have low RAM
        if (!pipActivity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            logDebugMessage("Picture In Picture mode not supported");
            return false;
        }

        if (isScreenReaderActiveAndTroublesome()) {
            logDebugMessage("Picture In Picture mode not supported when screen reader is active");
            return false;
        }

        if (!ActivityUtils.isFullScreen(pipActivity)) {
            logDebugMessage("Activity is not in fullscreen");
            return false;
        }

        if (!isMediaPlaying()) {
            logDebugMessage("Will not enter Picture in Picture mode if no media is playing");
            return false;
        }

        if (pipActivity.isInPictureInPictureMode()) {
            logDebugMessage("Activity is already in Picture In Picture " +
                    "or Picture In Picture mode is \"Not allowed\" by the user");
            return false;
        }

        return true;
    }

    private void updatePictureInPictureActions(@NonNull final  PictureInPictureParams params) {
        pipActivity.setPictureInPictureParams(params);
    }

    private PictureInPictureParams getPipParams(final boolean isMediaPlaying) {
        final List<RemoteAction> actions = new ArrayList<>(1);
        final PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();

        actions.add(isMediaPlaying ? getPauseRemoteAction() : getPlayRemoteAction());
        builder.setActions(actions);

        return builder.build();
    }

    private RemoteAction getPauseRemoteAction() {
        final String actionTitle = pipActivity.getString(R.string.pip_pause_button_title);
        final String actionDescription = pipActivity.getString(R.string.pip_pause_button_description);

        return new RemoteAction(getPauseIcon(), actionTitle, actionDescription, getIntentToPauseMedia());
    }

    private RemoteAction getPlayRemoteAction() {
        final String actionTitle = pipActivity.getString(R.string.pip_play_button_title);
        final String actionDescription = pipActivity.getString(R.string.pip_play_button_description);

        return new RemoteAction(getPlayIcon(), actionTitle, actionDescription, getIntentToResumeMedia());
    }

    private PendingIntent getIntentToPauseMedia() {
        return PendingIntent.getService(pipActivity, 0,
                getMediaControllerIntentForAction(GeckoMediaControlAgent.ACTION_PAUSE), 0);
    }

    private PendingIntent getIntentToResumeMedia() {
        return PendingIntent.getService(pipActivity, 0,
                getMediaControllerIntentForAction(GeckoMediaControlAgent.ACTION_RESUME), 0);
    }

    private Icon getPauseIcon() {
        return Icon.createWithResource(pipActivity, R.drawable.ic_media_pause);
    }

    private Icon getPlayIcon() {
        return Icon.createWithResource(pipActivity, R.drawable.ic_media_play);
    }

    private Intent getMediaControllerIntentForAction(@NonNull final String action) {
        return new Intent(action, null, pipActivity, MediaControlService.class);
    }

    private boolean isMediaPlaying() {
        return GeckoMediaControlAgent.isMediaPlaying();
    }

    /**
     * Trying to enter Picture In Picture mode on a Samsung device while an accessibility service
     * that provides spoken feedback would result in an IllegalStateException.
     */
    private boolean isScreenReaderActiveAndTroublesome() {
        final String affectedManufacturer = "samsung";

        final AccessibilityManager am =
                (AccessibilityManager) pipActivity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        final List<AccessibilityServiceInfo> enabledScreenReaderServices =
                am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        final String deviceManufacturer = android.os.Build.MANUFACTURER.toLowerCase();

        final boolean isScreenReaderServiceActive = !enabledScreenReaderServices.isEmpty();
        final boolean isDeviceManufacturerAffected = affectedManufacturer.equals(deviceManufacturer);

        return  isScreenReaderServiceActive && isDeviceManufacturerAffected;
    }

    private void logDebugMessage(@NonNull final String message) {
        if (BuildConfig.DEBUG) {
            Log.d(LOGTAG, message);
        }
    }

    @Override
    public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
        String newMediaStatus = message.getString("mediaControl");
        if (newMediaStatus == null) {
            Log.w(LOGTAG, "Can't extract new media status");
            return;
        }
        switch (newMediaStatus) {
            case "resumeMedia":
                updatePictureInPictureActions(getPipParams(true));
                break;
            case "mediaControlPaused":
                updatePictureInPictureActions(getPipParams(false));
                break;
            case "mediaControlStopped":
                updatePictureInPictureActions(getPipParams(false));
                break;
            default:
                Log.w(LOGTAG, String.format("Unknown new media status: %s", newMediaStatus));
        }
    }
}
