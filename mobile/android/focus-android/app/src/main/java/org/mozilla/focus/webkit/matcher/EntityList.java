/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;


import org.mozilla.focus.webkit.matcher.Trie.WhiteListTrie;

import java.net.MalformedURLException;
import java.net.URL;

/* package-private */ class EntityList {

    private WhiteListTrie rootNode;

    public EntityList() {
        rootNode = WhiteListTrie.createRootNode();
    }

    public void putWhiteList(final String revhost, final Trie whitelist) {
        rootNode.putWhiteList(revhost, whitelist);
    }

    public boolean isWhiteListed(final String site, final String resource) {
        if (site.length() == 0 ||
                resource.length() == 0) {
            return true;
        }

        try {
            final String revSitehost = new StringBuilder(new URL(site).getHost()).reverse().toString();
            final String revResourcehost = new StringBuilder(new URL(resource).getHost()).reverse().toString();

            return isWhiteListed(revSitehost, revResourcehost, rootNode);
        } catch (MalformedURLException e) {
            throw new IllegalArgumentException("Malformed URI supplied");
        }
    }

    private boolean isWhiteListed(final String site, final String resource, final Trie revHostTrie) {
        final WhiteListTrie next = (WhiteListTrie) revHostTrie.children.get(site.charAt(0));

        if (next == null) {
            // No matches
            return false;
        }

        if (next.whitelist != null &&
                next.whitelist.findNode(resource) != null) {
            return true;
        }

        if (site.length() == 1) {
            return false;
        }

        return isWhiteListed(site.substring(1), resource, next);
    }
}
