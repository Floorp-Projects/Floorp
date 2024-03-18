/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import android.util.SparseArray

/**
 * Simple implementation of a Trie, used for indexing URLs.
 */
open class Trie constructor(character: Char, parent: Trie?) {
    val children = SparseArray<Trie>()
    private var terminator = false

    init {
        parent?.children?.put(character.code, this)
    }

    /**
     * Finds the node corresponding to the provided string.
     *
     * @param string the string to search.
     * @return the corresponding node if found, otherwise null.
     */
    fun findNode(string: String): Trie? {
        return this.findNode(string.reversible())
    }

    /**
     * Finds the node corresponding to the provided string.
     *
     * @param string the string to search.
     * @return the corresponding node if found, otherwise null.
     */
    fun findNode(string: ReversibleString): Trie? {
        var match: Trie? = null
        if (terminator && (string.length() == 0 || string.charAt(0) == '.')) {
            // Match found and we're at a domain boundary. This is important, because
            // we don't want to return on partial domain matches. If the trie node is bar.com,
            // and the search string is foo-bar.com, we shouldn't match, but
            // foo.bar.com should match.)
            match = this
        } else if (string.length() != 0) {
            val next = children.get(string.charAt(0).code)
            match = next?.findNode(string.substring(1))
        }
        return match
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string.
     *
     * @param string the string for which a node should be added.
     * @return the newly created node or the existing one.
     */
    fun put(string: String): Trie {
        return this.put(string.reversible())
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string.
     *
     * @param string the string for which a node should be added.
     * @return the newly created node or the existing one.
     */
    fun put(string: ReversibleString): Trie {
        if (string.length() == 0) {
            terminator = true
            return this
        }

        val character = string.charAt(0)
        val child = put(character)
        return child.put(string.substring(1))
    }

    /**
     * Adds a new node for the provided character if none exists.
     *
     * @param character the character for which a node should be added.
     * @return the newly created node or the existing one.
     */
    fun put(character: Char): Trie {
        val existingChild = children.get(character.code)

        if (existingChild != null) {
            return existingChild
        }

        val newChild = createNode(character, this)
        children.put(character.code, newChild)
        return newChild
    }

    /**
     * Creates a new node for the provided character and parent node.
     *
     * @param character the character this node represents
     * @param parent the parent of this node
     */
    open fun createNode(character: Char, parent: Trie): Trie {
        return Trie(character, parent)
    }

    companion object {
        /**
         * Creates a new root node.
         */
        fun createRootNode(): Trie {
            return Trie(Character.MIN_VALUE, null)
        }
    }
}
