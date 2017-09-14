/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher;


import android.net.Uri;
import android.text.TextUtils;

import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.webview.matcher.Trie.WhiteListTrie;
import org.mozilla.focus.webview.matcher.util.FocusString;

/* package-private */ class EntityList {

    private WhiteListTrie rootNode;

    public EntityList() {
        rootNode = WhiteListTrie.createRootNode();
    }

    public void putWhiteList(final FocusString revhost, final Trie whitelist) {
        rootNode.putWhiteList(revhost, whitelist);
    }

    public boolean isWhiteListed(final Uri site, final Uri resource) {
        if (TextUtils.isEmpty(site.getHost()) ||
                TextUtils.isEmpty(resource.getHost()) ||
                site.getScheme().equals("data")) {
            return false;
        }

        if (UrlUtils.isPermittedResourceProtocol(resource.getScheme()) &&
                UrlUtils.isSupportedProtocol(site.getScheme())) {
            final FocusString revSitehost = FocusString.create(site.getHost()).reverse();
            final FocusString revResourcehost = FocusString.create(resource.getHost()).reverse();

            return isWhiteListed(revSitehost, revResourcehost, rootNode);
        } else {
            // This might be some imaginary/custom protocol: theguardian.com loads
            // things like "nielsenwebid://nuid/999" and/or sets an iFrame URL to that:
            return false;
        }
    }

    private boolean isWhiteListed(final FocusString site, final FocusString resource, final Trie revHostTrie) {
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
