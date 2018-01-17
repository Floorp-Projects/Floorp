/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.annotation.TargetApi;
import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.support.v7.media.MediaRouter;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.view.WindowManager;

import org.mozilla.gecko.AppConstants.Versions;

import org.mozilla.gecko.annotation.BuildFlag;
import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * A MediaPlayerManager with API 17+ Presentation support.
 */
@BuildFlag("MOZ_NATIVE_DEVICES")
@TargetApi(17)
public class PresentationMediaPlayerManager extends MediaPlayerManager {

    private static final String LOGTAG = "Gecko" + PresentationMediaPlayerManager.class.getSimpleName();

    private GeckoPresentation presentation;

    public PresentationMediaPlayerManager() {
        if (!Versions.feature17Plus) {
            throw new IllegalStateException(PresentationMediaPlayerManager.class.getSimpleName() +
                    " does not support < API 17");
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (presentation != null) {
            presentation.dismiss();
            presentation = null;
        }
    }

    @Override
    protected void updatePresentation() {
        if (mediaRouter == null) {
            return;
        }

        if (isPresentationMode) {
            return;
        }

        MediaRouter.RouteInfo route = mediaRouter.getSelectedRoute();
        Display display = route != null ? route.getPresentationDisplay() : null;

        if (display != null) {
            if ((presentation != null) && (presentation.getDisplay() != display)) {
                presentation.dismiss();
                presentation = null;
            }

            if (presentation == null) {
                final GeckoView geckoView = (GeckoView) getActivity().findViewById(R.id.layer_view);
                presentation = new GeckoPresentation(getActivity(), display, geckoView);

                try {
                    presentation.show();
                } catch (WindowManager.InvalidDisplayException ex) {
                    Log.w(LOGTAG, "Couldn't show presentation!  Display was removed in "
                            + "the meantime.", ex);
                    presentation = null;
                }
            }
        } else if (presentation != null) {
            presentation.dismiss();
            presentation = null;
        }
    }

    @WrapForJNI(calledFrom = "ui")
    /* protected */ static native void invalidateAndScheduleComposite(GeckoSession session);

    @WrapForJNI(calledFrom = "ui")
    /* protected */ static native void addPresentationSurface(GeckoSession session, Surface surface);

    @WrapForJNI(calledFrom = "ui")
    /* protected */ static native void removePresentationSurface();

    private static final class GeckoPresentation extends Presentation {
        private SurfaceView mView;
        private GeckoView mGeckoView;

        public GeckoPresentation(Context context, Display display, GeckoView geckoView) {
            super(context, display);

            mGeckoView = geckoView;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            mView = new SurfaceView(getContext());
            setContentView(mView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT));
            mView.getHolder().addCallback(new SurfaceListener(mGeckoView));
        }
    }

    private static final class SurfaceListener implements SurfaceHolder.Callback {
        private GeckoView mGeckoView;

        public SurfaceListener(GeckoView geckoView) {
            mGeckoView = geckoView;
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width,
                                   int height) {
            // Surface changed so force a composite
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                invalidateAndScheduleComposite(mGeckoView.getSession());
            }
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                addPresentationSurface(mGeckoView.getSession(), holder.getSurface());
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                removePresentationSurface();
            }
        }
    }
}
