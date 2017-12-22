/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.ConnectException;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.nio.charset.StandardCharsets;

import tools.fastlane.screengrab.ScreenshotCallback;
import tools.fastlane.screengrab.ScreenshotStrategy;

/**
 * This ScreenshotStrategy implementation pings the host system (the computer the emulator is
 * running on) to take a screenshot using "screencap" via adb. After that the screenshot is read
 * from the internal app storage and passed back to fastlane/screengrab using the provided callback.
 */
public class HostScreencapScreenshotStrategy implements ScreenshotStrategy {
    private static final int CONNECT_TIMEOUT = 1000;
    private static final int READ_TIMEOUT = 5000;
    private static final String HOST_LOOPBACK = "10.0.2.2";
    private static final int PORT = 9771;

    private UiDevice device;

    public HostScreencapScreenshotStrategy(UiDevice device) {
        this.device = device;
    }

    @Override
    public void takeScreenshot(String screenshotName, ScreenshotCallback screenshotCallback) {
        device.waitForIdle();

        takeScreenshotViaHost(screenshotName);

        Bitmap bitmap = readScreenshotFromStorage();

        if (bitmap == null) {
            bitmap = createDummyScreenShot();
        }

        screenshotCallback.screenshotCaptured(screenshotName, bitmap);
    }

    private void takeScreenshotViaHost(String name) {
        try {
            final HttpURLConnection connection = (HttpURLConnection) new URL("http://" + HOST_LOOPBACK + ":" + PORT + "/" + name).openConnection();
            connection.setConnectTimeout(CONNECT_TIMEOUT);
            connection.setReadTimeout(READ_TIMEOUT);

            try (final InputStream stream = connection.getInputStream()) {
                final String response = readInput(stream);
                stream.close();
                connection.disconnect();

                if (!"screenshot, exit=0".equals(response)) {
                    throw new RuntimeException("Taking screenshot failed, response: " + response);
                }
            }
        } catch (ConnectException | SocketTimeoutException e) {
            // We ignore those two exceptions because they occur if the http server on the host
            // system is not running and we want to be able to execute this test even if we are
            // not taking screenshots with fastlane. By running this test whenever we run the
            // other UI tests we make sure that this test doesn't break without us noticing.
        } catch (IOException e) {
            throw new RuntimeException("Taking screenshot failed", e);
        }
    }

    private Bitmap readScreenshotFromStorage() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        final String screenshotPath = new File(context.getFilesDir(), "temp_screen.png").getAbsolutePath();

        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;

        return BitmapFactory.decodeFile(screenshotPath);
    }

    private Bitmap createDummyScreenShot() {
        final Bitmap bitmap = Bitmap.createBitmap(480, 800, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.MAGENTA);
        return bitmap;
    }

    private static String readInput(InputStream stream) throws IOException {
        try (final BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            final StringBuilder builder = new StringBuilder();
            String line;

            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }

            return builder.toString();
        }
    }
}
