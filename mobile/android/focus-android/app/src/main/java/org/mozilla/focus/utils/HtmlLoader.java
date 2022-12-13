package org.mozilla.focus.utils;

import android.content.Context;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RawRes;
import android.util.Base64;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
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

        try (final BufferedReader fileReader =
                     new BufferedReader(new InputStreamReader(context.getResources().openRawResource(resourceID), StandardCharsets.UTF_8))) {

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
            throw new IllegalStateException("Unable to load error page data", e);
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
        try (final InputStream pngInputStream = context.getResources().openRawResource(resourceID)) {
            // Base64 encodes 3 bytes at a time, make sure we have a multiple of 3 here
            // I don't know what a sensible chunk size is, let's just go with 300b.
            final byte[] data = new byte[3 * 100];
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
        }

        return  builder.toString();
    }
}
