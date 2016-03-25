package org.mozilla.gecko.feeds.knownsites;

import android.support.annotation.NonNull;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Wordpress.com
 */
public class KnownSiteWordpress implements KnownSite {
    @Override
    public String getURLSearchString() {
        return ".wordpress.com";
    }

    @Override
    public String getFeedFromURL(String url) {
        Pattern pattern = Pattern.compile("https?://(.*?).wordpress.com(/.*)?");
        Matcher matcher = pattern.matcher(url);
        if (matcher.matches()) {
            return "https://" + matcher.group(1) + ".wordpress.com/feed/";
        }
        return null;
    }
}
