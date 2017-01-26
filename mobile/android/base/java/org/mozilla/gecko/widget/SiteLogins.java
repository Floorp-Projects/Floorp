package org.mozilla.gecko.widget;

import org.mozilla.gecko.util.GeckoBundle;

public class SiteLogins {
    private final GeckoBundle[] logins;

    public SiteLogins(GeckoBundle[] logins) {
        this.logins = logins;
    }

    public GeckoBundle[] getLogins() {
        return logins;
    }
}
