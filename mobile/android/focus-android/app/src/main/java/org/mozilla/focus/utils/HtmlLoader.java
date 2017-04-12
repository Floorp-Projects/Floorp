package org.mozilla.focus.utils;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RawRes;
import android.util.Base64;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Map;

public class HtmlLoader {

    /**
     * Load a given (html or css) resource file into a String. The input can contain tokens that will
     * be replaced with localised strings.
     *
     * @param substitutionTable A table of substitions, e.g. %shortMessage% -> "Error loading page..."
     *                          Can be null, in which case no substitutions will be made.
     * @return The file content, with all substitutions having being made.
     */
    public static String loadResourceFile(@NonNull final Context context,
                                           @NonNull final @RawRes int resourceID,
                                           @Nullable final Map<String, String> substitutionTable) {

        BufferedReader fileReader = null;

        try {
            final InputStream fileStream = context.getResources().openRawResource(resourceID);
            fileReader = new BufferedReader(new InputStreamReader(fileStream));

            final StringBuilder outputBuffer = new StringBuilder();

            String line;
            while ((line = fileReader.readLine()) != null) {
                if (substitutionTable != null) {
                    for (final Map.Entry<String, String> entry : substitutionTable.entrySet()) {
                        line = line.replace(entry.getKey(), entry.getValue());
                    }
                }

                outputBuffer.append(line);
            }

            return outputBuffer.toString();
        } catch (final IOException e) {
            throw new IllegalStateException("Unable to load error page data");
        } finally {
            try {
                if (fileReader != null) {
                    fileReader.close();
                }
            } catch (IOException e) {
                // There's pretty much nothing we can do here. It doesn't seem right to crash
                // just because we couldn't close a file?
            }
        }
    }

    private final static byte[] pngHeader = new byte[] { -119, 80, 78, 71, 13, 10, 26, 10 };

    public static String loadPngAsDataURI(@NonNull final Context context,
                                          @NonNull final @DrawableRes int resourceID) {

        final StringBuilder builder = new StringBuilder();
        builder.append("data:image/png;base64,");

        // We are copying the approach BitmapFactory.decodeResource(Resources, int, Options)
        // uses - you are explicitly allowed to open Drawables, but the method has a @RawRes
        // annotation (despite officially supporting Drawables).
        //noinspection ResourceType
        final InputStream pngInputStream = context.getResources().openRawResource(resourceID);
        final BufferedReader reader = new BufferedReader(new InputStreamReader(pngInputStream));

        try {
            // Base64 encodes 3 bytes at a time, make sure we have a multiple of 3 here
            // I don't know what a sensible chunk size is, let's just go with 300b.
            final byte[] data = new byte[3*100];
            int bytesRead;
            boolean headerVerified = false;

            while ((bytesRead = pngInputStream.read(data)) > 0) {
                // Sanity check: lets make sure this is still a png (i.e. make sure the build system
                // or Android haven't broken / change the image format).
                if (!headerVerified) {
                    if (bytesRead < 8) {
                        throw new IllegalStateException("Loaded drawable is improbably small");
                    }

                    for (int i = 0; i < pngHeader.length; i++) {
                        if (data[i] != pngHeader[i]) {
                            throw new IllegalStateException("Invalid png detected");
                        }
                    }
                    headerVerified = true;
                }

                builder.append(Base64.encodeToString(data, 0, bytesRead, 0));
            }
        } catch (IOException e) {
            throw new IllegalStateException("Unable to load png data");
        } finally {
            try {
                reader.close();
            } catch (IOException e) {
                // Nothing to do here...
            }
        }

        return  builder.toString();
    }
}
