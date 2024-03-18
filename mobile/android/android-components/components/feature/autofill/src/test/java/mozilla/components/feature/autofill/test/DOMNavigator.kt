/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.test

import android.view.autofill.AutofillId
import mozilla.components.feature.autofill.structure.AutofillNodeNavigator
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.support.test.mock
import org.w3c.dom.Document
import org.w3c.dom.Element
import java.io.File
import java.util.WeakHashMap
import javax.xml.parsers.DocumentBuilderFactory

/**
 * Alternative [AutofillNodeNavigator] implementation for reading a captured view structure from an
 * XML file for testing purposes.
 */
internal class DOMNavigator(
    file: File,
    override val activityPackageName: String,
) : AutofillNodeNavigator<Element, AutofillId> {

    override fun currentText(node: Element): String? {
        return node.getAttribute("autofillValue")
    }

    private val document: Document

    init {
        val db = DocumentBuilderFactory.newInstance().newDocumentBuilder()
        file.inputStream().use {
            document = db.parse(it)
        }
    }

    override val rootNodes: List<Element>
        get() = listOf(document.documentElement)

    override fun childNodes(node: Element): List<Element> {
        val children = node.childNodes
        return (0 until children.length)
            .map { children.item(it) }
            .filter { it is Element }
            .map { it as Element }
    }

    override fun clues(node: Element): Iterable<CharSequence> {
        val attributes = node.attributes
        return (0 until attributes.length)
            .map { attributes.item(it) }
            .mapNotNull { if (it.nodeName != "hint") it.nodeValue else null }
    }

    override fun autofillId(node: Element): AutofillId? {
        val rawId = if (isEditText(node) || isHtmlInputField(node)) {
            attr(node, "autofillId") ?: clues(node).joinToString("|")
        } else {
            null
        }

        return rawId?.let { getOrCreateAutofillIdMock(it) }
    }

    override fun isEditText(node: Element): Boolean =
        tagName(node) == "EditText" || (inputType(node) and AutofillNodeNavigator.editTextMask) > 0

    override fun isHtmlInputField(node: Element) = tagName(node) == "input"

    private fun tagName(node: Element) = node.tagName

    override fun isHtmlForm(node: Element): Boolean = node.tagName == "form"

    fun attr(node: Element, name: String) = node.attributes.getNamedItem(name)?.nodeValue

    override fun isFocused(node: Element) = attr(node, "focus") == "true"

    override fun isVisible(node: Element) = attr(node, "visibility")?.let { it == "0" } ?: true

    override fun packageName(node: Element) = attr(node, "idPackage")

    override fun webDomain(node: Element) = attr(node, "webDomain")

    override fun isButton(node: Element): Boolean {
        when (node.tagName) {
            "Button" -> return true
            "button" -> return true
        }

        return when (attr(node, "type")) {
            "submit" -> true
            "button" -> true
            else -> false
        }
    }

    override fun inputType(node: Element): Int =
        attr(node, "inputType")?.let {
            Integer.parseInt(it, 16)
        } ?: 0

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

private val autofillIdMapping = WeakHashMap<AutofillId, String>()

private fun getOrCreateAutofillIdMock(id: String): AutofillId {
    val existingEntry = autofillIdMapping.entries.firstOrNull { entry -> entry.value == id }
    if (existingEntry != null) {
        return existingEntry.key
    }

    val autofillId: AutofillId = mock()
    autofillIdMapping[autofillId] = id
    return autofillId
}

internal val AutofillId.originalId: String
    get() = autofillIdMapping[this] ?: throw AssertionError("Unknown AutofillId instance")
