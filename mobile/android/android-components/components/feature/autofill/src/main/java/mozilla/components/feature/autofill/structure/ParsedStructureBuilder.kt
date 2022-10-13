/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.structure

import android.os.Build
import android.view.View
import androidx.annotation.RequiresApi

@RequiresApi(Build.VERSION_CODES.O)
internal class ParsedStructureBuilder<ViewNode, AutofillId>(
    private val navigator: AutofillNodeNavigator<ViewNode, AutofillId>,
) {
    fun build(): ParsedStructure {
        val formNode = findFocusedForm()
        val (usernameId, passwordId) = findAutofillIds(formNode)
        val hostnameClue = usernameId ?: passwordId

        return navigator.build(
            usernameId,
            passwordId,
            getWebDomain(hostnameClue),
            getPackageName(hostnameClue) ?: navigator.activityPackageName,
        )
    }

    private fun findFocusedForm(): ViewNode? {
        val focusPath = findMatchedNodeAncestors {
            navigator.isFocused(it)
        }

        return focusPath?.lastOrNull {
            navigator.isHtmlForm(it)
        }
    }

    private fun findAutofillIds(rootNode: ViewNode?): Pair<AutofillId?, AutofillId?> =
        checkForAdjacentFields(rootNode) ?: getUsernameId(rootNode) to getPasswordId(rootNode)

    private fun getUsernameId(rootNode: ViewNode?): AutofillId? {
        // how do we localize the "email" and "username"?
        return getAutofillIdForKeywords(
            rootNode,
            listOf(
                View.AUTOFILL_HINT_USERNAME,
                View.AUTOFILL_HINT_EMAIL_ADDRESS,
                "email",
                "username",
                "user name",
                "identifier",
                "account_name",
            ),
        )
    }

    private fun getPasswordId(rootNode: ViewNode?): AutofillId? {
        // similar l10n question for password
        return getAutofillIdForKeywords(rootNode, listOf(View.AUTOFILL_HINT_PASSWORD, "password"))
    }

    private fun getAutofillIdForKeywords(rootNode: ViewNode?, keywords: Collection<String>): AutofillId? {
        return checkForNamedTextField(rootNode, keywords)
            ?: checkForConsecutiveLabelAndField(rootNode, keywords)
            ?: checkForNestedLayoutAndField(rootNode, keywords)
    }

    private fun checkForNamedTextField(rootNode: ViewNode?, keywords: Collection<String>): AutofillId? {
        return navigator.findFirst(rootNode) { node: ViewNode ->
            if (isAutoFillableEditText(node, keywords) || isAutoFillableInputField(node, keywords)) {
                navigator.autofillId(node)
            } else {
                null
            }
        }
    }

    private fun checkForConsecutiveLabelAndField(rootNode: ViewNode?, keywords: Collection<String>): AutofillId? {
        return navigator.findFirst(rootNode) { node: ViewNode ->
            val childNodes = navigator.childNodes(node)
            // check for consecutive views with keywords followed by possible fill locations
            for (i in 1.until(childNodes.size)) {
                val prevNode = childNodes[i - 1]
                val currentNode = childNodes[i]
                val id = navigator.autofillId(currentNode) ?: continue
                if (
                    (navigator.isEditText(currentNode) || navigator.isHtmlInputField(currentNode)) &&
                    containsKeywords(prevNode, keywords)
                ) {
                    return@findFirst id
                }
            }
            null
        }
    }

    private fun checkForNestedLayoutAndField(rootNode: ViewNode?, keywords: Collection<String>): AutofillId? {
        return navigator.findFirst(rootNode) { node: ViewNode ->
            val childNodes = navigator.childNodes(node)

            if (childNodes.size != 1) {
                return@findFirst null
            }

            val child = childNodes[0]
            val id = navigator.autofillId(child) ?: return@findFirst null
            if (
                (navigator.isEditText(child) || navigator.isHtmlInputField(child)) &&
                containsKeywords(node, keywords)
            ) {
                return@findFirst id
            }
            null
        }
    }

    private fun checkForAdjacentFields(rootNode: ViewNode?): Pair<AutofillId?, AutofillId?>? {
        return navigator.findFirst(rootNode) { node: ViewNode ->

            val childNodes = navigator.childNodes(node)
            // XXX we only look at the list of edit texts before the first button.
            // This is because we can see the invisible fields, but not that they are
            // invisible. https://bugzilla.mozilla.org/show_bug.cgi?id=1592047
            val firstButtonIndex = childNodes.indexOfFirst { navigator.isButton(it) }

            val firstFewNodes = if (firstButtonIndex >= 0) {
                childNodes.subList(0, firstButtonIndex)
            } else {
                childNodes
            }

            val inputFields = firstFewNodes.filter {
                navigator.isEditText(it) && navigator.autofillId(it) != null && navigator.isVisible(it)
            }

            // we must have a minimum of two EditText boxes in order to have a pair.
            if (inputFields.size < 2) {
                return@findFirst null
            }

            for (i in 1.until(inputFields.size)) {
                val prevNode = inputFields[i - 1]
                val currentNode = inputFields[i]
                if (navigator.isPasswordField(currentNode) && navigator.isPasswordField(prevNode).not()) {
                    return@findFirst navigator.autofillId(prevNode) to navigator.autofillId(currentNode)
                }
            }

            null
        }
    }

    private fun getWebDomain(nearby: AutofillId?): String? {
        return nearestFocusedNode(nearby) {
            navigator.webDomain(it)
        }
    }

    private fun getPackageName(nearby: AutofillId?): String? {
        return nearestFocusedNode(nearby) {
            navigator.packageName(it)
        }
    }

    private fun <T> nearestFocusedNode(nearby: AutofillId?, transform: (ViewNode) -> T?): T? {
        val id = nearby ?: return null
        val ancestors = findMatchedNodeAncestors {
            navigator.autofillId(it) == id
        }
        return ancestors?.map(transform)?.firstOrNull { it != null }
    }

    private fun isAutoFillableEditText(node: ViewNode, keywords: Collection<String>): Boolean {
        return navigator.isEditText(node) &&
            containsKeywords(node, keywords) &&
            navigator.autofillId(node) != null
    }

    private fun isAutoFillableInputField(node: ViewNode, keywords: Collection<String>): Boolean {
        return navigator.isHtmlInputField(node) &&
            containsKeywords(node, keywords) &&
            navigator.autofillId(node) != null
    }

    private fun containsKeywords(node: ViewNode, keywords: Collection<String>): Boolean {
        val hints = navigator.clues(node)
        keywords.forEach { keyword ->
            hints.forEach { hint ->
                if (hint.contains(keyword, true)) {
                    return true
                }
            }
        }
        return false
    }

    private fun findMatchedNodeAncestors(matcher: (ViewNode) -> Boolean): Iterable<ViewNode>? {
        navigator.rootNodes
            .forEach { node ->
                findMatchedNodeAncestors(node, matcher)?.let { result ->
                    return result
                }
            }
        return null
    }

    /**
     * Depth first search a ViewNode tree. Once a match is found, a list of ancestors all the way to
     * the top is returned. The first node in the list is the matching node, the last is the root node.
     * If no match is found, then <code>null</code> is returned.
     *
     * @param node the parent node.
     * @param matcher a closure which returns <code>true</code> if and only if the node is matched.
     * @return an ordered list of the matched node and all its ancestors starting at the matched node.
     */
    private fun findMatchedNodeAncestors(node: ViewNode, matcher: (ViewNode) -> Boolean): Iterable<ViewNode>? {
        if (matcher(node)) {
            return listOf(node)
        }

        navigator.childNodes(node)
            .forEach { child ->
                findMatchedNodeAncestors(child, matcher)?.let { list ->
                    return list + node
                }
            }
        return null
    }
}
