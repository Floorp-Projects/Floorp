/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.helpers;

import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.util.Scanner;

public class FileUtil {
    private FileUtil() { }

    public static JSONObject readJSONObjectFromFile(final File file) throws Exception {
        final StringBuilder builder = new StringBuilder();
        final Scanner scanner = new Scanner(file);
        try {
            while (scanner.hasNext()) {
                builder.append(scanner.next());
            }
        } finally {
            scanner.close();
        }
        return new JSONObject(builder.toString());
    }

    public static void writeJSONObjectToFile(final File file, final JSONObject obj) throws Exception {
        writeStringToFile(file, obj.toString());
    }

    public static void writeStringToFile(final File file, final String str) throws Exception {
        final FileWriter writer = new FileWriter(file, false);
        try {
            writer.write(str);
        } finally {
            writer.close();
        }
    }
}
