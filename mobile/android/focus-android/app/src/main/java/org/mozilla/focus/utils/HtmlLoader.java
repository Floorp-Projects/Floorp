package org.mozilla.focus.utils;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RawRes;

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
}
