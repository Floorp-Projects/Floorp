/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util.publicsuffix;

import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.util.IOUtils;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashSet;
import java.util.Set;

class PublicSuffixPatterns {
    /** If a hostname is contained as a key in this map, it is a public suffix. */
    private static Set<String> EXACT = null;

    static synchronized Set<String> getExactSet(Context context) {
        if (EXACT != null) {
            return EXACT;
        }

        EXACT = new HashSet<>();

        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new InputStreamReader(
                    new BufferedInputStream(context.getAssets().open("publicsuffixlist"))));

            String line;
            while ((line = reader.readLine()) != null) {
                EXACT.add(line);
            }

        } catch (IOException e) {
            Log.e("Patterns", "IOException during loading public suffix list");
        } finally {
            IOUtils.safeStreamClose(reader);
        }

        return EXACT;
    }


    /**
     * If a hostname is not a key in the EXCLUDE map, and if removing its
     * leftmost component results in a name which is a key in this map, it is a
     * public suffix.
     */
    static final Set<String> UNDER = new HashSet<>();
    static {
        UNDER.add("bd");
        UNDER.add("magentosite.cloud");
        UNDER.add("ke");
        UNDER.add("triton.zone");
        UNDER.add("compute.estate");
        UNDER.add("ye");
        UNDER.add("pg");
        UNDER.add("kh");
        UNDER.add("platform.sh");
        UNDER.add("fj");
        UNDER.add("ck");
        UNDER.add("fk");
        UNDER.add("alces.network");
        UNDER.add("sch.uk");
        UNDER.add("jm");
        UNDER.add("mm");
        UNDER.add("api.githubcloud.com");
        UNDER.add("ext.githubcloud.com");
        UNDER.add("0emm.com");
        UNDER.add("githubcloudusercontent.com");
        UNDER.add("cns.joyent.com");
        UNDER.add("bn");
        UNDER.add("yokohama.jp");
        UNDER.add("nagoya.jp");
        UNDER.add("kobe.jp");
        UNDER.add("sendai.jp");
        UNDER.add("kawasaki.jp");
        UNDER.add("sapporo.jp");
        UNDER.add("kitakyushu.jp");
        UNDER.add("np");
        UNDER.add("nom.br");
        UNDER.add("er");
        UNDER.add("cryptonomic.net");
        UNDER.add("gu");
        UNDER.add("kw");
        UNDER.add("zw");
        UNDER.add("mz");
    }

    /**
     * The elements in this map would pass the UNDER test, but are known not to
     * be public suffixes and are thus excluded from consideration. Since it
     * refers to elements in UNDER of the same type, the type is actually not
     * important here. The map is simply used for consistency reasons.
     */
    static final Set<String> EXCLUDED = new HashSet<>();
    static {
        EXCLUDED.add("www.ck");
        EXCLUDED.add("city.yokohama.jp");
        EXCLUDED.add("city.nagoya.jp");
        EXCLUDED.add("city.kobe.jp");
        EXCLUDED.add("city.sendai.jp");
        EXCLUDED.add("city.kawasaki.jp");
        EXCLUDED.add("city.sapporo.jp");
        EXCLUDED.add("city.kitakyushu.jp");
        EXCLUDED.add("teledata.mz");
    }
}
