/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.Log;

import org.mozilla.gecko.GeckoThread;
import org.yaml.snakeyaml.TypeDescription;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.Constructor;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class DebugConfig {
    private static final String LOGTAG = "GeckoDebugConfig";

    protected Map<String, Object> prefs;
    protected Map<String, String> env;
    protected List<String> args;

    public static @NonNull DebugConfig fromFile(final @NonNull File configFile) throws FileNotFoundException {
        final Constructor constructor = new Constructor(DebugConfig.class);
        final TypeDescription description = new TypeDescription(DebugConfig.class);
        description.putMapPropertyType("prefs", String.class, Object.class);
        description.putMapPropertyType("env", String.class, String.class);
        description.putListPropertyType("args", String.class);

        final Yaml yaml = new Yaml(constructor);
        yaml.addTypeDescription(description);

        final FileInputStream fileInputStream = new FileInputStream(configFile);
        try {
            return yaml.load(fileInputStream);
        } finally {
            IOUtils.safeStreamClose(fileInputStream);
        }
    }

    public void mergeIntoInitInfo(final @NonNull GeckoThread.InitInfo info) {
        if (env != null) {
            Log.d(LOGTAG, "Adding environment variables from debug config: " + env);

            if (info.extras == null) {
                info.extras = new Bundle();
            }

            int c = 0;
            while (info.extras.getString("env" + c) != null) {
                c += 1;
            }

            for (final Map.Entry<String, String> entry : env.entrySet()) {
                info.extras.putString("env" + c, entry.getKey() + "=" + entry.getValue());
                c += 1;
            }
        }

        if (args != null) {
            Log.d(LOGTAG, "Adding arguments from debug config: " + args);

            final ArrayList<String> combinedArgs = new ArrayList<>();
            combinedArgs.addAll(Arrays.asList(info.args));
            combinedArgs.addAll(args);

            info.args = combinedArgs.toArray(new String[combinedArgs.size()]);
        }

        if (prefs != null) {
            Log.d(LOGTAG, "Adding prefs from debug config: " + prefs);

            final Map<String, Object> combinedPrefs = new HashMap<>();
            combinedPrefs.putAll(info.prefs);
            combinedPrefs.putAll(prefs);
            info.prefs = Collections.unmodifiableMap(prefs);
        }
    }
}
