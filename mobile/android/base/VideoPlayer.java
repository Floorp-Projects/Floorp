/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.MediaController;
import android.widget.VideoView;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URL;
import java.net.URLConnection;

public final class VideoPlayer extends Activity {
    public static final String VIDEO_ACTION = "org.mozilla.gecko.PLAY_VIDEO";

    private VideoView mVideoView;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.videoplayer);
        mVideoView = (VideoView) findViewById(R.id.VideoView);
        MediaController mediaController = new MediaController(this);
        mediaController.setAnchorView(mVideoView);
        Intent intent = getIntent();
        final Uri data = intent.getData();
        if (data == null) {
            return;
        }

        String spec = null;
        if ("vnd.youtube".equals(data.getScheme())) {
            String ssp = data.getSchemeSpecificPart();
            int paramIndex = ssp.indexOf('?');
            String id;
            if (paramIndex == -1) {
                id = ssp;
            } else {
                id = ssp.substring(0, paramIndex);
            }
            spec = getSpecFromYouTubeVideoID(id);
        }

        if (spec == null) {
            return;
        }

        final Uri video = Uri.parse(spec);
        mVideoView.setMediaController(mediaController);
        mVideoView.setVideoURI(video);
        mVideoView.start();
    }

    private String getSpecFromYouTubeVideoID(String id) {
        String spec = null;
        try {
            String infoUri = "http://www.youtube.com/get_video_info?&video_id=" + id;
            URL infoUrl = new URL(infoUri);
            URLConnection urlConnection = infoUrl.openConnection();
            BufferedReader br = new BufferedReader(new InputStreamReader(urlConnection.getInputStream()));
            try {
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null)
                    sb.append(line);
                android.net.Uri fakeUri = android.net.Uri.parse("fake:/fake?" + sb);
                String streamMap = fakeUri.getQueryParameter("url_encoded_fmt_stream_map");
                if (streamMap == null)
                    return null;
                String[] streams = streamMap.split(",");
                for (int i = 0; i < streams.length; i++) {
                    fakeUri = android.net.Uri.parse("fake:/fake?" + streams[i]);
                    String url = fakeUri.getQueryParameter("url");
                    String type = fakeUri.getQueryParameter("type");
                    if (type != null && url != null &&
                        (type.startsWith("video/mp4") || type.startsWith("video/webm"))) {
                        spec = url;
                    }
                }
            } finally {
                br.close();
            }
        } catch (Exception e) {
            Log.e("VideoPlayer", "exception", e);
        }
        return spec;
    }
}
