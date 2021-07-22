/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import android.net.Uri
import android.text.TextUtils
import android.util.JsonReader
import java.util.ArrayList

/**
 * Stores safe-listed URIs for individual hosts.
 */
internal class Safelist {
    private val rootNode: SafelistTrie = SafelistTrie.createRootNode()

    /**
     * Adds the provided safelist for the provided host.
     *
     * @param host the reversed host URI ("foo.com".reverse())
     * @param safelist a [Trie] representing the safe-listed URIs
     */
    fun put(host: ReversibleString, safelist: Trie) {
        rootNode.putSafelist(host, safelist)
    }

    /**
     * Checks if the given resource is safe-listed for the given host.
     *
     * @param host the host URI as string ("foo.com")
     * @param host the resources URI as string ("bar.com")
     */
    fun contains(host: String, resource: String): Boolean {
        return contains(Uri.parse(host), Uri.parse(resource))
    }

    /**
     * Checks if the given resource is safe-listed for the given host.
     *
     * @param hostUri the host URI
     * @param resource the resources URI
     */
    fun contains(hostUri: Uri, resource: Uri): Boolean {
        return if (TextUtils.isEmpty(hostUri.host) || TextUtils.isEmpty(resource.host) || hostUri.scheme == "data") {
            false
        } else if (resource.scheme?.isPermittedResourceProtocol() == true &&
            hostUri.scheme?.isSupportedProtocol() == true
        ) {
            contains(hostUri.host!!.reverse(), resource.host!!.reverse(), rootNode)
        } else {
            false
        }
    }

    private fun contains(site: ReversibleString, resource: ReversibleString, revHostTrie: Trie): Boolean {
        val next = revHostTrie.children.get(site.charAt(0).code) as? SafelistTrie ?: return false

        if (next.safelist?.findNode(resource) != null) {
            return true
        }

        return if (site.length() == 1) false else contains(site.substring(1), resource, next)
    }

    /**
     * Check if this String is a valid resource protocol.
     */
    private fun String.isPermittedResourceProtocol(): Boolean {
        return this.startsWith("http") ||
            this.startsWith("https") ||
            this.startsWith("file") ||
            this.startsWith("data") ||
            this.startsWith("javascript") ||
            this.startsWith("about")
    }

    /**
     * Check if this String is a supported protocol.
     */
    private fun String.isSupportedProtocol(): Boolean {
        return this.isPermittedResourceProtocol() || this.startsWith("error")
    }

    companion object {
        /**
         * Parses json for safe-listed URIs.
         *
         * @param reader a JsonReader
         * @return the safe list.
         */
        @Suppress("NestedBlockDepth")
        fun fromJson(reader: JsonReader): Safelist {
            val safelist = Safelist()
            reader.beginObject()

            while (reader.hasNext()) {
                reader.skipValue()
                reader.beginObject()

                val safelistTrie = Trie.createRootNode()
                val propertyList = ArrayList<String>()
                while (reader.hasNext()) {
                    val itemName = reader.nextName()
                    if (itemName == "properties") {
                        reader.beginArray()
                        while (reader.hasNext()) {
                            propertyList.add(reader.nextString())
                        }
                        reader.endArray()
                    } else if (itemName == "resources") {
                        reader.beginArray()
                        while (reader.hasNext()) {
                            safelistTrie.put(reader.nextString().reverse())
                        }
                        reader.endArray()
                    }
                }
                propertyList.forEach { safelist.put(it.reverse(), safelistTrie) }
                reader.endObject()
            }
            reader.endObject()
            return safelist
        }
    }
}

/**
 * A [Trie] implementation which stores a safe list (another [Trie]).
 */
internal class SafelistTrie private constructor(character: Char, parent: SafelistTrie?) : Trie(character, parent) {
    var safelist: Trie? = null

    override fun createNode(character: Char, parent: Trie): Trie {
        return SafelistTrie(character, parent as SafelistTrie)
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string and stores
     * the provide safelist Trie.
     *
     * @param string the string for which a node should be added.
     * @param safelist the safelist to store.
     * @return the newly created node or the existing one.
     */
    fun putSafelist(string: String, safelist: Trie) {
        this.putSafelist(string.reversible(), safelist)
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string and stores
     * the provide safelist Trie.
     *
     * @param string the string for which a node should be added.
     * @param safelist the safelist to store.
     * @return the newly created node or the existing one.
     */
    fun putSafelist(string: ReversibleString, safelist: Trie) {
        val node = super.put(string) as SafelistTrie

        if (node.safelist != null) {
            throw IllegalStateException("Safelist already set for node $string")
        }

        node.safelist = safelist
    }

    companion object {
        /**
         * Creates a new root node.
         */
        fun createRootNode(): SafelistTrie {
            return SafelistTrie(Character.MIN_VALUE, null)
        }
    }
}
