/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.service;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.util.Log;
import android.view.View;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.overlays.OverlayConstants;
import org.mozilla.gecko.overlays.service.sharemethods.AddBookmark;
import org.mozilla.gecko.overlays.service.sharemethods.AddToReadingList;
import org.mozilla.gecko.overlays.service.sharemethods.SendTab;
import org.mozilla.gecko.overlays.service.sharemethods.ShareMethod;
import org.mozilla.gecko.overlays.ui.OverlayToastHelper;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.EnumMap;
import java.util.Map;

import static org.mozilla.gecko.overlays.OverlayConstants.ACTION_PREPARE_SHARE;
import static org.mozilla.gecko.overlays.OverlayConstants.ACTION_SHARE;
import static org.mozilla.gecko.overlays.OverlayConstants.EXTRA_SHARE_METHOD;

/**
 * A service to receive requests from overlays to perform actions.
 * See OverlayConstants for details of the intent API supported by this service.
 *
 * Currently supported operations are:
 *
 * Add bookmark*
 * Add to reading list*
 * Send tab (delegates to Sync's existing handler)
 * Future: Load page in background.
 *
 * * Neither of these incur a page fetch on the service... yet. That will require headless Gecko,
 *   something we're yet to have. Refactoring Gecko as a service itself and restructing the rest of
 *   the app to talk to it seems like the way to go there.
 */
public class OverlayActionService extends Service {
    private static final String LOGTAG = "GeckoOverlayService";

    // Map used for selecting the appropriate helper object when handling a share.
    private final Map<ShareMethod.Type, ShareMethod> shareTypes = new EnumMap<>(ShareMethod.Type.class);

    // Map relating Strings representing share types to the corresponding ShareMethods.
    // Share methods are initialised (and shown in the UI) in the order they are given here.
    // This map is used to look up the appropriate ShareMethod when handling a request, as well as
    // for identifying which ShareMethod needs re-initialising in response to such an intent (which
    // will be necessary in situations such as the deletion of Sync accounts).

    // Not a bindable service.
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            return START_NOT_STICKY;
        }

        // Dispatch intent to appropriate method according to its action.
        String action = intent.getAction();

        switch (action) {
            case ACTION_SHARE:
                handleShare(intent);
                break;
            case ACTION_PREPARE_SHARE:
                initShareMethods(getApplicationContext());
                break;
            default:
                throw new IllegalArgumentException("Unsupported intent action: " + action);
        }

        return START_NOT_STICKY;
    }

    /**
     * Reinitialise all ShareMethods, causing them to broadcast any UI update events necessary.
     */
    private void initShareMethods(Context context) {
        shareTypes.clear();

        shareTypes.put(ShareMethod.Type.ADD_BOOKMARK, new AddBookmark(context));
        shareTypes.put(ShareMethod.Type.ADD_TO_READING_LIST, new AddToReadingList(context));
        shareTypes.put(ShareMethod.Type.SEND_TAB, new SendTab(context));
    }

    public void handleShare(final Intent intent) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                ShareData shareData;
                try {
                    shareData = ShareData.fromIntent(intent);
                } catch (IllegalArgumentException e) {
                    Log.e(LOGTAG, "Error parsing share intent: ", e);
                    return;
                }

                ShareMethod shareMethod = shareTypes.get(shareData.shareMethodType);

                final ShareMethod.Result result = shareMethod.handle(shareData);
                // Dispatch the share to the targeted ShareMethod.
                switch (result) {
                    case SUCCESS:
                        // \o/
                        OverlayToastHelper.showSuccessToast(getApplicationContext(), shareMethod.getSuccessMesssage());
                        break;
                    case TRANSIENT_FAILURE:
                        // An OnClickListener to do this share again.
                        View.OnClickListener retryListener = new View.OnClickListener() {
                            @Override
                            public void onClick(View view) {
                                handleShare(intent);
                            }
                        };

                        // Show a failure toast with a retry button.
                        OverlayToastHelper.showFailureToast(getApplicationContext(), shareMethod.getFailureMessage(), retryListener);
                        break;
                    case PERMANENT_FAILURE:
                        // Show a failure toast without a retry button.
                        OverlayToastHelper.showFailureToast(getApplicationContext(), shareMethod.getFailureMessage());
                        break;
                    default:
                        Assert.isTrue(false, "Unknown share method result code: " + result);
                        break;
                }
            }
        });
    }
}
