/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.net.Uri;
import android.util.DisplayMetrics;
import android.util.Log;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Downloader.Response;
import com.squareup.picasso.UrlConnectionDownloader;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.EnumSet;
import java.util.Set;

import org.mozilla.gecko.distribution.Distribution;

public class ImageLoader {
    private static final String LOGTAG = "GeckoImageLoader";

    private static final String DISTRIBUTION_SCHEME = "gecko.distribution";
    private static final String SUGGESTED_SITES_AUTHORITY = "suggestedsites";

    // The order of density factors to try when looking for an image resource
    // in the distribution directory. It looks for an exact match first (1.0) then
    // tries to find images with higher density (2.0 and 1.5). If no image is found,
    // try a lower density (0.5). See loadDistributionImage().
    private static final float[] densityFactors = new float[] { 1.0f, 2.0f, 1.5f, 0.5f };

    private static enum Density {
        MDPI,
        HDPI,
        XHDPI,
        XXHDPI;

        @Override
        public String toString() {
            return super.toString().toLowerCase();
        }
    }

    private static Picasso instance;

    public static synchronized Picasso with(Context context) {
        if (instance == null) {
            Picasso.Builder builder = new Picasso.Builder(context);

            final Distribution distribution = Distribution.getInstance(context);
            builder.downloader(new ImageDownloader(context, distribution));
            instance = builder.build();
        }

        return instance;
    }

    /**
     * Custom Downloader built on top of Picasso's UrlConnectionDownloader
     * that supports loading images from custom URIs.
     */
    public static class ImageDownloader extends UrlConnectionDownloader {
        private final Context context;
        private final Distribution distribution;

        public ImageDownloader(Context context, Distribution distribution) {
            super(context);
            this.context = context;
            this.distribution = distribution;
        }

        private Density getDensity(float factor) {
            final DisplayMetrics dm = context.getResources().getDisplayMetrics();
            final float densityDpi = dm.densityDpi * factor;

            if (densityDpi >= DisplayMetrics.DENSITY_XXHIGH) {
                return Density.XXHDPI;
            } else if (densityDpi >= DisplayMetrics.DENSITY_XHIGH) {
                return Density.XHDPI;
            } else if (densityDpi >= DisplayMetrics.DENSITY_HIGH) {
                return Density.HDPI;
            }

            // Fallback to mdpi, no need to handle ldpi.
            return Density.MDPI;
        }

        @Override
        public Response load(Uri uri, boolean localCacheOnly) throws IOException {
            final String scheme = uri.getScheme();
            if (DISTRIBUTION_SCHEME.equals(scheme)) {
                return loadDistributionImage(uri);
            }

            return super.load(uri, localCacheOnly);
        }

        private static String getPathForDensity(String basePath, Density density,
                                                String filename) {
            final File dir = new File(basePath, density.toString());
            return String.format("%s/%s.png", dir.toString(), filename);
        }

        /**
         * Handle distribution URIs in Picasso. The expected format is:
         *
         *   gecko.distribution://<basepath>/<imagename>
         *
         * Which will look for the following file in the distribution:
         *
         *   <distribution-root-dir>/<basepath>/<device-density>/<imagename>.png
         */
        private Response loadDistributionImage(Uri uri) throws IOException {
            // Eliminate the leading '//'
            final String ssp = uri.getSchemeSpecificPart().substring(2);

            final String filename;
            final String basePath;

            final int slashIndex = ssp.lastIndexOf('/');
            if (slashIndex == -1) {
                filename = ssp;
                basePath = "";
            } else {
                filename = ssp.substring(slashIndex + 1);
                basePath = ssp.substring(0, slashIndex);
            }

            Set<Density> triedDensities = EnumSet.noneOf(Density.class);

            for (int i = 0; i < densityFactors.length; i++) {
                final Density density = getDensity(densityFactors[i]);
                if (!triedDensities.add(density)) {
                    continue;
                }

                final String path = getPathForDensity(basePath, density, filename);
                Log.d(LOGTAG, "Trying to load image from distribution " + path);

                final File f = distribution.getDistributionFile(path);
                if (f != null) {
                    return new Response(new FileInputStream(f), true);
                }
            }

            throw new ResponseException("Couldn't find suggested site image in distribution");
        }
    }
}
