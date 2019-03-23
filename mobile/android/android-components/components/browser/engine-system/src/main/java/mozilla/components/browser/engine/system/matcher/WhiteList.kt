/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import android.net.Uri
import android.text.TextUtils
import android.util.JsonReader
import java.util.ArrayList

/**
 * Stores white-listed URIs for individual hosts.
 */
internal class WhiteList {
    private val rootNode: WhiteListTrie = WhiteListTrie.createRootNode()

    /**
     * Adds the provided whitelist for the provided host.
     *
     * @param host the reversed host URI ("foo.com".reverse())
     * @param whitelist a [Trie] representing the white-listed URIs
     */
    fun put(host: ReversibleString, whitelist: Trie) {
        rootNode.putWhiteList(host, whitelist)
    }

    /**
     * Checks if the given resource is white-listed for the given host.
     *
     * @param host the host URI as string ("foo.com")
     * @param host the resources URI as string ("bar.com")
     */
    fun contains(host: String, resource: String): Boolean {
        return contains(Uri.parse(host), Uri.parse(resource))
    }

    /**
     * Checks if the given resource is white-listed for the given host.
     *
     * @param hostUri the host URI
     * @param resource the resources URI
     */
    fun contains(hostUri: Uri, resource: Uri): Boolean {
        return if (TextUtils.isEmpty(hostUri.host) || TextUtils.isEmpty(resource.host) || hostUri.scheme == "data") {
            false
        } else if (resource.scheme?.isPermittedResourceProtocol() == true &&
                hostUri.scheme?.isSupportedProtocol() == true) {
            contains(hostUri.host!!.reverse(), resource.host!!.reverse(), rootNode)
        } else {
            false
        }
    }

    private fun contains(site: ReversibleString, resource: ReversibleString, revHostTrie: Trie): Boolean {
        val next = revHostTrie.children.get(site.charAt(0).toInt()) as? WhiteListTrie ?: return false

        if (next.whitelist?.findNode(resource) != null) {
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
         * Parses json for white-listed URIs.
         *
         * @param reader a JsonReader
         * @return the white list.
         */
        @Suppress("NestedBlockDepth")
        fun fromJson(reader: JsonReader): WhiteList {
            val whiteList = WhiteList()
            reader.beginObject()

            while (reader.hasNext()) {
                reader.skipValue()
                reader.beginObject()

                val whitelistTrie = Trie.createRootNode()
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
                            whitelistTrie.put(reader.nextString().reverse())
                        }
                        reader.endArray()
                    }
                }
                propertyList.forEach { whiteList.put(it.reverse(), whitelistTrie) }
                reader.endObject()
            }
            reader.endObject()
            return whiteList
        }
    }
}

/**
 * A [Trie] implementation which stores a white list (another [Trie]).
 */
internal class WhiteListTrie private constructor(character: Char, parent: WhiteListTrie?) : Trie(character, parent) {
    var whitelist: Trie? = null

    override fun createNode(character: Char, parent: Trie): Trie {
        return WhiteListTrie(character, parent as WhiteListTrie)
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string and stores
     * the provide whitelist Trie.
     *
     * @param string the string for which a node should be added.
     * @param whitelist the whitelist to store.
     * @return the newly created node or the existing one.
     */
    fun putWhiteList(string: String, whitelist: Trie) {
        this.putWhiteList(string.reversible(), whitelist)
    }

    /**
     * Adds new nodes (recursively) for all chars in the provided string and stores
     * the provide whitelist Trie.
     *
     * @param string the string for which a node should be added.
     * @param whitelist the whitelist to store.
     * @return the newly created node or the existing one.
     */
    fun putWhiteList(string: ReversibleString, whitelist: Trie) {
        val node = super.put(string) as WhiteListTrie

        if (node.whitelist != null) {
            throw IllegalStateException("Whitelist already set for node $string")
        }

        node.whitelist = whitelist
    }

    companion object {
        /**
         * Creates a new root node.
         */
        fun createRootNode(): WhiteListTrie {
            return WhiteListTrie(Character.MIN_VALUE, null)
        }
    }
}
