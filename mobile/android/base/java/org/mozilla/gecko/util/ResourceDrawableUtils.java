/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.util.Log;

import java.io.InputStream;
import java.net.URL;

import static org.mozilla.gecko.util.BitmapUtils.getBitmapFromDataURI;
import static org.mozilla.gecko.util.BitmapUtils.getResource;

public class ResourceDrawableUtils {
    private static final String LOGTAG = "ResourceDrawableUtils";

    public static Drawable getDrawable(@NonNull final Context context,
                                       @DrawableRes final int drawableID) {
        return AppCompatResources.getDrawable(context, drawableID);
    }

    public interface BitmapLoader {
        public void onBitmapFound(Drawable d);
    }

    public static void runOnBitmapFoundOnUiThread(final BitmapLoader loader, final Drawable d) {
        if (ThreadUtils.isOnUiThread()) {
            loader.onBitmapFound(d);
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                loader.onBitmapFound(d);
            }
        });
    }

    /**
     * Attempts to find a drawable associated with a given string, using its URI scheme to determine
     * how to load the drawable. The BitmapLoader's `onBitmapFound` method is always called, and
     * will be called with `null` if no drawable is found.
     *
     * The BitmapLoader `onBitmapFound` method always runs on the UI thread.
     */
    public static void getDrawable(final Context context, final String data, final BitmapLoader loader) {
        if (TextUtils.isEmpty(data)) {
            runOnBitmapFoundOnUiThread(loader, null);
            return;
        }

        if (data.startsWith("data")) {
            final BitmapDrawable d = new BitmapDrawable(context.getResources(), getBitmapFromDataURI(data));
            runOnBitmapFoundOnUiThread(loader, d);
            return;
        }

        if (data.startsWith("jar:") || data.startsWith("file://")) {
            (new UIAsyncTask.WithoutParams<Drawable>(ThreadUtils.getBackgroundHandler()) {
                @Override
                public Drawable doInBackground() {
                    try {
                        if (data.startsWith("jar:jar")) {
                            return GeckoJarReader.getBitmapDrawable(
                                    context, context.getResources(), data);
                        }

                        // Don't attempt to validate the JAR signature when loading an add-on icon
                        if (data.startsWith("jar:file")) {
                            return GeckoJarReader.getBitmapDrawable(
                                    context, context.getResources(), Uri.decode(data));
                        }

                        final URL url = new URL(data);
                        final InputStream is = (InputStream) url.getContent();
                        try {
                            return Drawable.createFromStream(is, "src");
                        } finally {
                            is.close();
                        }
                    } catch (Exception e) {
                        Log.w(LOGTAG, "Unable to set icon", e);
                    }
                    return null;
                }

                @Override
                public void onPostExecute(Drawable drawable) {
                    loader.onBitmapFound(drawable);
                }
            }).execute();
            return;
        }

        if (data.startsWith("-moz-icon://")) {
            final Uri imageUri = Uri.parse(data);
            final String ssp = imageUri.getSchemeSpecificPart();
            final String resource = ssp.substring(ssp.lastIndexOf('/') + 1);

            try {
                final Drawable d = context.getPackageManager().getApplicationIcon(resource);
                runOnBitmapFoundOnUiThread(loader, d);
            } catch (Exception ex) { }

            return;
        }

        if (data.startsWith("drawable://")) {
            final Uri imageUri = Uri.parse(data);
            final int id = getResource(context, imageUri);
            final Drawable d = getDrawable(context, id);

            runOnBitmapFoundOnUiThread(loader, d);
            return;
        }

        runOnBitmapFoundOnUiThread(loader, null);
    }

}
