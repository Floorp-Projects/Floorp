/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import android.app.Instrumentation;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import com.robotium.solo.Solo;

import org.mozilla.gecko.tests.UITestContext;

import java.util.regex.Pattern;

/**
 * Provides helper functions for using Robotium.
 */
public final class RobotiumHelper {
    private static Solo sSolo;
    private static Instrumentation sInstrumentation;

    private RobotiumHelper() { /* To disallow instantiation. */ }

    protected static void init(final UITestContext context) {
        sSolo = context.getSolo();
        sInstrumentation = context.getInstrumentation();
    }

    /**
     * Same as Solo.waitForText(), but matching against full text, without regular expressions.
     */
    public static boolean waitForExactText(final String text,
                                           final int minimumNumberOfMatches,
                                           final long timeout) {
        String matchText = "^" + Pattern.quote(text) + "$";
        return sSolo.waitForText(matchText, minimumNumberOfMatches, timeout);
    }

    /**
     * Same as Solo.searchText(), but matching against full text, without regular expressions.
     */
    public static boolean searchExactText(final String text,
                                          final boolean onlyVisible) {
        String matchText = "^" + Pattern.quote(text) + "$";
        return sSolo.searchText(matchText, onlyVisible);
    }

    /**
     * Long press on the center of {@code fromView}, and then drag to the center of {@code toView}.
     * @param fromView The {@code View} where the drag starts.
     * @param toView The {@code View} where the drag ends.
     * @param overshoot Overshoot the center of {@code toView} by a little bit if true.
     */
    public static void longPressDragView(View fromView, View toView, boolean overshoot) {
        final int[] fromCoords = new int[2];
        fromView.getLocationOnScreen(fromCoords);
        fromCoords[0] += fromView.getWidth() / 2;
        fromCoords[1] += fromView.getHeight() / 2;

        final int[] toTabCoords = new int[2];
        toView.getLocationOnScreen(toTabCoords);
        toTabCoords[0] += toView.getWidth() / 2;
        toTabCoords[1] += toView.getHeight() / 2;

        final long timeAtDownPress = SystemClock.uptimeMillis();

        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(timeAtDownPress, eventTime, MotionEvent.ACTION_DOWN, fromCoords[0], fromCoords[1], 0);
        sInstrumentation.sendPointerSync(event);
        // Wait for the long press to kick in.
        sSolo.sleep(ViewConfiguration.getLongPressTimeout() + 250);

        final int iterations = 20;
        final float deltaX = ((float) (toTabCoords[0] - fromCoords[0])) / iterations;
        final float deltaY = ((float) (toTabCoords[1] - fromCoords[1])) / iterations;
        for (int i = 1; i <= iterations; i++) {
            final float x = fromCoords[0] + i * deltaX;
            final float y = fromCoords[1] + i * deltaY;
            eventTime = SystemClock.uptimeMillis();
            event = MotionEvent.obtain(timeAtDownPress, eventTime, MotionEvent.ACTION_MOVE, x, y, 0);
            sInstrumentation.sendPointerSync(event);
        }

        final int finalX;
        final int finalY;
        if (overshoot) {
            // Overshoot the destination by a little bit.
            final int extra = 10;
            final int extraX = extra * (int) Math.signum(deltaX);
            final int extraY = extra * (int) Math.signum(deltaY);
            finalX = toTabCoords[0] + extraX;
            finalY = toTabCoords[1] + extraY;

            eventTime = SystemClock.uptimeMillis();
            event = MotionEvent.obtain(timeAtDownPress, eventTime, MotionEvent.ACTION_MOVE, finalX, finalY, 0);
            sInstrumentation.sendPointerSync(event);
        } else {
            finalX = toTabCoords[0];
            finalY = toTabCoords[1];
        }

        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(timeAtDownPress, eventTime, MotionEvent.ACTION_UP, finalX, finalY, 0);
        sInstrumentation.sendPointerSync(event);
    }
}
