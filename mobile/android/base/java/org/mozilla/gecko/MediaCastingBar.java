/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.widget.TextView;

public class MediaCastingBar extends RelativeLayout implements View.OnClickListener, BundleEventListener  {
    private static final String LOGTAG = "GeckoMediaCastingBar";

    private TextView mCastingTo;
    private ImageButton mMediaPlay;
    private ImageButton mMediaPause;
    private ImageButton mMediaStop;

    private boolean mInflated;

    public MediaCastingBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Casting:Started",
            "Casting:Paused",
            "Casting:Playing",
            "Casting:Stopped");
    }

    public void inflateContent() {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View content = inflater.inflate(R.layout.media_casting, this);

        mMediaPlay = (ImageButton) content.findViewById(R.id.media_play);
        mMediaPlay.setOnClickListener(this);
        mMediaPause = (ImageButton) content.findViewById(R.id.media_pause);
        mMediaPause.setOnClickListener(this);
        mMediaStop = (ImageButton) content.findViewById(R.id.media_stop);
        mMediaStop.setOnClickListener(this);

        mCastingTo = (TextView) content.findViewById(R.id.media_sending_to);

        // Capture clicks on the rest of the view to prevent them from
        // leaking into other views positioned below.
        content.setOnClickListener(this);

        mInflated = true;
    }

    public void show() {
        if (!mInflated)
            inflateContent();

        setVisibility(VISIBLE);
    }

    public void hide() {
        setVisibility(GONE);
    }

    public void onDestroy() {
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
            "Casting:Started",
            "Casting:Paused",
            "Casting:Playing",
            "Casting:Stopped");
    }

    // View.OnClickListener implementation
    @Override
    public void onClick(View v) {
        final int viewId = v.getId();

        if (viewId == R.id.media_play) {
            EventDispatcher.getInstance().dispatch("Casting:Play", null);
            mMediaPlay.setVisibility(GONE);
            mMediaPause.setVisibility(VISIBLE);
        } else if (viewId == R.id.media_pause) {
            EventDispatcher.getInstance().dispatch("Casting:Pause", null);
            mMediaPause.setVisibility(GONE);
            mMediaPlay.setVisibility(VISIBLE);
        } else if (viewId == R.id.media_stop) {
            EventDispatcher.getInstance().dispatch("Casting:Stop", null);
        }
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Casting:Started".equals(event)) {
            show();
            mCastingTo.setText(message.getString("device", ""));
            mMediaPlay.setVisibility(GONE);
            mMediaPause.setVisibility(VISIBLE);

        } else if ("Casting:Paused".equals(event)) {
            mMediaPause.setVisibility(GONE);
            mMediaPlay.setVisibility(VISIBLE);

        } else if ("Casting:Playing".equals(event)) {
            mMediaPlay.setVisibility(GONE);
            mMediaPause.setVisibility(VISIBLE);

        } else if ("Casting:Stopped".equals(event)) {
            hide();
        }
    }
}
