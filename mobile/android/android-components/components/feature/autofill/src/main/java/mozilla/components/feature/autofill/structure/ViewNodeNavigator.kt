/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.structure

import android.app.assist.AssistStructure
import android.app.assist.AssistStructure.ViewNode
import android.os.Build
import android.text.InputType
import android.view.View
import android.view.autofill.AutofillId
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.structure.AutofillNodeNavigator.Companion.editTextMask
import java.util.Locale

/**
 * Helper for navigating autofill nodes.
 *
 * Original implementation imported from Lockwise:
 * https://github.com/mozilla-lockwise/lockwise-android/blob/f303f8aee7cc96dcdf4e7863fef6c19ae874032e/app/src/main/java/mozilla/lockbox/autofill/ViewNodeNavigator.kt#L13
 */
internal interface AutofillNodeNavigator<Node, Id> {
    companion object {
        val editTextMask = InputType.TYPE_CLASS_TEXT
        val passwordMask =
            InputType.TYPE_TEXT_VARIATION_PASSWORD or
                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
    }

    val rootNodes: List<Node>
    val activityPackageName: String
    fun childNodes(node: Node): List<Node>
    fun clues(node: Node): Iterable<CharSequence>
    fun autofillId(node: Node): Id?
    fun isEditText(node: Node): Boolean
    fun isHtmlInputField(node: Node): Boolean
    fun isHtmlForm(node: Node): Boolean
    fun packageName(node: Node): String?
    fun webDomain(node: Node): String?
    fun currentText(node: Node): String?
    fun inputType(node: Node): Int
    fun isPasswordField(node: Node): Boolean = (inputType(node) and passwordMask) > 0
    fun isButton(node: Node): Boolean
    fun isFocused(node: Node): Boolean
    fun isVisible(node: Node): Boolean
    fun build(
        usernameId: Id?,
        passwordId: Id?,
        webDomain: String?,
        packageName: String,
    ): ParsedStructure

    private fun <T> findFirstRoots(transform: (Node) -> T?): T? {
        rootNodes
            .forEach { node ->
                findFirst(node, transform)?.let { result ->
                    return result
                }
            }
        return null
    }

    @Suppress("ReturnCount")
    fun <T> findFirst(rootNode: Node? = null, transform: (Node) -> T?): T? {
        val node = rootNode ?: return findFirstRoots(transform)

        transform(node)?.let {
            return it
        }

        childNodes(node)
            .forEach { child ->
                findFirst(child, transform)?.let { result ->
                    return result
                }
            }
        return null
    }
}

/**
 * Helper for navigating autofill nodes.
 *
 * Original implementation imported from Lockwise:
 * https://github.com/mozilla-lockwise/lockwise-android/blob/f303f8aee7cc96dcdf4e7863fef6c19ae874032e/app/src/main/java/mozilla/lockbox/autofill/ViewNodeNavigator.kt#L72
 */
@RequiresApi(Build.VERSION_CODES.O)
internal class ViewNodeNavigator(
    private val structure: AssistStructure,
    override val activityPackageName: String,
) : AutofillNodeNavigator<ViewNode, AutofillId> {
    override val rootNodes: List<ViewNode>
        get() = structure.run { (0 until windowNodeCount).map { getWindowNodeAt(it).rootViewNode } }

    override fun childNodes(node: ViewNode): List<ViewNode> =
        node.run { (0 until childCount) }.map { node.getChildAt(it) }

    override fun clues(node: ViewNode): Iterable<CharSequence> {
        val hints = mutableListOf<CharSequence?>(
            node.text,
            node.idEntry,
            node.hint, // This is localized.
        )

        node.autofillOptions?.let {
            hints.addAll(it)
        }

        node.autofillHints?.let {
            hints.addAll(it)
        }

        node.htmlInfo?.attributes?.let { attrs ->
            hints.addAll(attrs.map { it.second })
        }

        return hints.filterNotNull()
    }

    override fun autofillId(node: ViewNode): AutofillId? = node.autofillId

    override fun isEditText(node: ViewNode) =
        inputType(node) and editTextMask > 0

    override fun inputType(node: ViewNode) = node.inputType

    override fun isHtmlInputField(node: ViewNode) =
        htmlTagName(node) == "input"

    private fun htmlAttr(node: ViewNode, name: String) =
        node.htmlInfo?.attributes?.find { name == it.first }?.second

    @Suppress("ReturnCount")
    override fun isButton(node: ViewNode): Boolean {
        val className = node.className ?: ""
        when {
            className.contains("Button") -> return true
            htmlTagName(node) == "button" -> return true
            htmlTagName(node) != "input" -> return false
        }

        return when (htmlAttr(node, "type")) {
            "submit" -> true
            "button" -> true
            else -> false
        }
    }

    private fun htmlTagName(node: ViewNode) =
        // Use English locale, as the HTML tags are all in English.
        node.htmlInfo?.tag?.lowercase(Locale.ENGLISH)

    override fun isHtmlForm(node: ViewNode) =
        htmlTagName(node) == "form"

    override fun isVisible(node: ViewNode) = node.visibility == View.VISIBLE

    override fun packageName(node: ViewNode): String? = node.idPackage

    override fun webDomain(node: ViewNode): String? = node.webDomain

    override fun currentText(node: ViewNode): String? {
        return if (node.autofillValue?.isText == true) {
            node.autofillValue?.textValue.toString()
        } else {
            null
        }
    }

    override fun isFocused(node: ViewNode) = node.isFocused

    override fun build(
        usernameId: AutofillId?,
        passwordId: AutofillId?,
        webDomain: String?,
        packageName: String,
    ): ParsedStructure {
        return ParsedStructure(
            usernameId,
            passwordId,
            webDomain,
            packageName,
        )
    }
}
