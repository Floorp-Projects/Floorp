/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.SdkConstants
import com.android.SdkConstants.ATTR_STYLE
import com.android.resources.ResourceFolderType
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.ResourceXmlDetector
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.XmlContext
import org.w3c.dom.Element

/**
 * A custom lint check that prohibits not using the style attribute on buttons
 */
class ButtonStyleXmlDetector : ResourceXmlDetector() {
    companion object {
        const val ERROR_MESSAGE =
            "All buttons must have a style, try using NeutralButton or similar."

        val ISSUE_XML_STYLE = Issue.create(
            id = "ButtonStyleXmlDetector",
            briefDescription = "Prohibits using a button without a style",
            explanation = "Butttons should have a style applied",
            category = Category.CORRECTNESS,
            severity = Severity.ERROR,
            implementation = Implementation(
                ButtonStyleXmlDetector::class.java,
                Scope.RESOURCE_FILE_SCOPE,
            ),
        )
    }

    override fun appliesTo(folderType: ResourceFolderType): Boolean {
        // Return true if we want to analyze resource files in the specified resource
        // folder type. In this case we only need to analyze layout resource files.
        return folderType == ResourceFolderType.LAYOUT
    }

    override fun getApplicableElements(): Collection<String>? {
        return setOf(
            SdkConstants.FQCN_BUTTON,
            SdkConstants.MATERIAL_BUTTON,
            SdkConstants.BUTTON,
        )
    }

    override fun visitElement(context: XmlContext, element: Element) {
        if (element.hasAttribute(ATTR_STYLE)) { return }

        context.report(
            issue = ISSUE_XML_STYLE,
            scope = element,
            location = context.getElementLocation(element),
            message = ERROR_MESSAGE,
        )
    }
}
