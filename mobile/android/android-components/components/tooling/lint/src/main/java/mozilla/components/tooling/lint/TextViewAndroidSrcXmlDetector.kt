/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.SdkConstants.ATTR_DRAWABLE_BOTTOM
import com.android.SdkConstants.ATTR_DRAWABLE_END
import com.android.SdkConstants.ATTR_DRAWABLE_LEFT
import com.android.SdkConstants.ATTR_DRAWABLE_RIGHT
import com.android.SdkConstants.ATTR_DRAWABLE_START
import com.android.SdkConstants.ATTR_DRAWABLE_TOP
import com.android.SdkConstants.FQCN_TEXT_VIEW
import com.android.SdkConstants.TEXT_VIEW
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
 * A custom lint check that prohibits not using the app:srcCompat for ImageViews
 */
class TextViewAndroidSrcXmlDetector : ResourceXmlDetector() {
    companion object {
        const val SCHEMA = "http://schemas.android.com/apk/res/android"

        const val ERROR_MESSAGE =
            "Using android:drawableX to define resource instead of app:drawableXCompat"

        val ISSUE_XML_SRC_USAGE = Issue.create(
            id = "TextViewAndroidSrcXmlDetector",
            briefDescription = "Prohibits using android namespace to define drawables in TextViews",
            explanation = "TextView drawables should be declared using app:drawableXCompat",
            category = Category.CORRECTNESS,
            severity = Severity.ERROR,
            implementation = Implementation(
                TextViewAndroidSrcXmlDetector::class.java,
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
            FQCN_TEXT_VIEW,
            TEXT_VIEW,
        )
    }

    override fun visitElement(context: XmlContext, element: Element) {
        val node = when {
            element.hasAttributeNS(SCHEMA, ATTR_DRAWABLE_BOTTOM) -> element.getAttributeNodeNS(
                SCHEMA,
                ATTR_DRAWABLE_BOTTOM,
            )
            element.hasAttributeNS(SCHEMA, ATTR_DRAWABLE_END) -> element.getAttributeNodeNS(
                SCHEMA,
                ATTR_DRAWABLE_END,
            )
            element.hasAttributeNS(SCHEMA, ATTR_DRAWABLE_LEFT) -> element.getAttributeNodeNS(
                SCHEMA,
                ATTR_DRAWABLE_LEFT,
            )
            element.hasAttributeNS(
                SCHEMA,
                ATTR_DRAWABLE_RIGHT,
            ) -> element.getAttributeNodeNS(SCHEMA, ATTR_DRAWABLE_RIGHT)
            element.hasAttributeNS(
                SCHEMA,
                ATTR_DRAWABLE_START,
            ) -> element.getAttributeNodeNS(SCHEMA, ATTR_DRAWABLE_START)
            element.hasAttributeNS(SCHEMA, ATTR_DRAWABLE_TOP) -> element.getAttributeNodeNS(
                SCHEMA,
                ATTR_DRAWABLE_TOP,
            )
            else -> null
        } ?: return

        context.report(
            issue = ISSUE_XML_SRC_USAGE,
            scope = node,
            location = context.getLocation(node),
            message = ERROR_MESSAGE,
        )
    }
}
