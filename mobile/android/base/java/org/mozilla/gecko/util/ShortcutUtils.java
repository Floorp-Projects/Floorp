/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.annotation.TargetApi;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Icon;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;

import java.lang.reflect.Constructor;

public class ShortcutUtils {
    private static final String LOG_TAG = "ShortcutUtils";

    public static void createHomescreenIcon(final Intent shortcutIntent, final String aTitle,
                                             final String aURI, final Bitmap aIcon) {
        final Context context = GeckoAppShell.getApplicationContext();

        if (Versions.feature26Plus) {
            createHomescreenIcon26(context, shortcutIntent, aTitle, aURI, aIcon);
        } else {
            final Intent intent = new Intent();
            intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
            intent.putExtra(Intent.EXTRA_SHORTCUT_ICON,
                            getLauncherIcon(aIcon, GeckoAppShell.getPreferredIconSize()));

            if (aTitle != null) {
                intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
            } else {
                intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);
            }

            // Do not allow duplicate items.
            intent.putExtra("duplicate", false);

            intent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
            context.sendBroadcast(intent);

            // After shortcut is created, show the mobile desktop.
            ActivityUtils.goToHomeScreen(context);
        }

        // Remember interaction
        final UrlAnnotations urlAnnotations = BrowserDB.from(context).getUrlAnnotations();
        urlAnnotations.insertHomeScreenShortcut(context.getContentResolver(), aURI, true);
    }

    @TargetApi(26)
    private static void createHomescreenIcon26(final Context context, final Intent shortcutIntent,
                                               final String aTitle, final String aURI, final Bitmap aIcon) {
        try {
            final Class<?> mgrCls = Class.forName("android.content.pm.ShortcutManager");
            final Object mgr = context.getSystemService(mgrCls);

            final Class<?> builderCls = Class.forName("android.content.pm.ShortcutInfo$Builder");
            final Constructor<?> builderCtor = builderCls.getDeclaredConstructor(Context.class, String.class);

            final Object builder = builderCtor.newInstance(context, aURI);


            builderCls.getDeclaredMethod("setIcon", Icon.class)
                .invoke(builder, Icon.createWithBitmap(getLauncherIcon(aIcon, GeckoAppShell.getPreferredIconSize())));

            builderCls.getDeclaredMethod("setIntent", Intent.class)
                .invoke(builder, shortcutIntent);

            builderCls.getDeclaredMethod("setShortLabel", CharSequence.class)
                .invoke(builder, aTitle != null ? aTitle : aURI);

            Object info = builderCls.getDeclaredMethod("build")
                .invoke(builder);

            final Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_HOME);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
            final IntentSender intentSender = pendingIntent.getIntentSender();
            mgrCls.getDeclaredMethod("requestPinShortcut",
                                     info.getClass(),
                                     IntentSender.class)
                .invoke(mgr, info, intentSender);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Failed to pin shortcut: ", e);
        }

        // Use this when we can build against SDK 26
        //
        // final ShortcutManager mgr = context.getSystemService(ShortcutManager.class);

        // final ShortcutInfo info = new ShortcutInfo.Builder(context, aURI)
        //     .setIcon(Icon.createWithBitmap(getLauncherIcon(aIcon, GeckoAppShell.getPreferredIconSize())))
        //     .setIntent(shortcutIntent)
        //     .setShortLabel(aTitle != null ? aTitle : aURI)
        //     .build();

        // mgr.requestPinShortcut(info, null);
    }

    public static boolean isPinShortcutSupported() {
        if (Versions.feature26Plus) {
            return isPinShortcutSupported26();
        }
        return true;
    }

    @TargetApi(26)
    private static boolean isPinShortcutSupported26() {
        final Context context = GeckoAppShell.getApplicationContext();
        try {
            final Class<?> mgrCls = Class.forName("android.content.pm.ShortcutManager");
            final Object mgr = context.getSystemService(mgrCls);

            final boolean supported = (boolean)
                mgrCls.getDeclaredMethod("isRequestPinShortcutSupported")
                .invoke(mgr);
            return supported;
        } catch (final Exception e) {
            return false;
        }
    }

    private static Bitmap getLauncherIcon(Bitmap aSource, int size) {
        final float[] DEFAULT_LAUNCHER_ICON_HSV = { 32.0f, 1.0f, 1.0f };
        final int kOffset = 6;
        final int kRadius = 5;

        final int insetSize = aSource != null ? size * 2 / 3 : size;

        final Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);

        // draw a base color
        final Paint paint = new Paint();
        if (aSource == null) {
            // If we aren't drawing a favicon, just use an orange color.
            paint.setColor(Color.HSVToColor(DEFAULT_LAUNCHER_ICON_HSV));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                           kRadius, kRadius, paint);
        } else if (aSource.getWidth() >= insetSize || aSource.getHeight() >= insetSize) {
            // Otherwise, if the icon is large enough, just draw it.
            final Rect iconBounds = new Rect(0, 0, size, size);
            canvas.drawBitmap(aSource, null, iconBounds, null);
            return bitmap;
        } else {
            // Otherwise, use the dominant color from the icon +
            // a layer of transparent white to lighten it somewhat.
            final int color = BitmapUtils.getDominantColorCustomImplementation(aSource);
            paint.setColor(color);
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                           kRadius, kRadius, paint);
            paint.setColor(Color.argb(100, 255, 255, 255));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                 kRadius, kRadius, paint);
        }

        // draw the overlay
        final Context context = GeckoAppShell.getApplicationContext();
        final Bitmap overlay = BitmapUtils.decodeResource(context, R.drawable.home_bg);
        canvas.drawBitmap(overlay, null, new Rect(0, 0, size, size), null);

        // draw the favicon
        if (aSource == null)
            aSource = BitmapUtils.decodeResource(context, R.drawable.home_star);

        // by default, we scale the icon to this size
        final int sWidth = insetSize / 2;
        final int sHeight = sWidth;

        final int halfSize = size / 2;
        canvas.drawBitmap(aSource,
                null,
                new Rect(halfSize - sWidth,
                        halfSize - sHeight,
                        halfSize + sWidth,
                        halfSize + sHeight),
                null);

        return bitmap;
    }
}
