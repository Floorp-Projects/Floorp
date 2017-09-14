/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher;

import android.util.SparseArray;

import org.mozilla.focus.webview.matcher.util.FocusString;

/* package-private */ class Trie {

    /**
     * Trie that adds storage for a whitelist (itself another trie) on each node.
     */
    public static class WhiteListTrie extends Trie {
        Trie whitelist = null;

        private WhiteListTrie(final char character, final WhiteListTrie parent) {
            super(character, parent);
        }

        @Override
        protected Trie createNode(final char character, final Trie parent) {
            return new WhiteListTrie(character, (WhiteListTrie) parent);
        }

        public static WhiteListTrie createRootNode() {
            return new WhiteListTrie(Character.MIN_VALUE, null);
        }

        /* Convenience method so that clients aren't forced to do their own casting. */
        public void putWhiteList(final FocusString string, final Trie whitelist) {
            WhiteListTrie node = (WhiteListTrie) super.put(string);

            if (node.whitelist != null) {
                throw new IllegalStateException("Whitelist already set for node " + string);
            }

            node.whitelist = whitelist;
        }
    }

    public final SparseArray<Trie> children = new SparseArray<>();
    public boolean terminator = false;

    public Trie findNode(final FocusString string) {
        if (terminator) {
            // Match achieved - and we're at a domain boundary. This is important, because
            // we don't want to return on partial domain matches. (E.g. if the trie node is bar.com,
            // and the search string is foo-bar.com, we shouldn't match. But foo.bar.com should match.)
            if (string.length() == 0 || string.charAt(0) == '.') {
                return this;
            }
        } else if (string.length() == 0) {
            // Finished the string, no match
            return null;
        }

        final Trie next = children.get(string.charAt(0));

        if (next == null) {
            return null;
        }

        return next.findNode(string.substring(1));
    }

    public Trie put(final FocusString string) {
        if (string.length() == 0) {
            terminator = true;
            return this;
        }

        final char character = string.charAt(0);

        final Trie child = put(character);

        return child.put(string.substring(1));
    }

    public Trie put(char character) {
        final Trie existingChild = children.get(character);

        if (existingChild != null) {
            return existingChild;
        }

        final Trie newChild = createNode(character, this);

        children.put(character, newChild);

        return newChild;
    }

    private Trie(char character, Trie parent) {
        if (parent != null) {
            parent.children.put(character, this);
        }
    }

    public static Trie createRootNode() {
        return new Trie(Character.MIN_VALUE, null);
    }

    // Subclasses must override to provide their node implementation
    protected Trie createNode(final char character, final Trie parent) {
        return new Trie(character, parent);
    }
}
