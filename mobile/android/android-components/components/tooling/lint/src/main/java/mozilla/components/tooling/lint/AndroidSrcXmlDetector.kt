/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.SdkConstants.ATTR_SRC
import com.android.SdkConstants.FQCN_IMAGE_BUTTON
import com.android.SdkConstants.FQCN_IMAGE_VIEW
import com.android.SdkConstants.IMAGE_BUTTON
import com.android.SdkConstants.IMAGE_VIEW
import com.android.resources.ResourceFolderType
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.ResourceXmlDetector
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.XmlContext
import com.google.common.annotations.VisibleForTesting
import org.w3c.dom.Element

/**
 * A custom lint check that prohibits not using the app:srcCompat for ImageViews
 */
class AndroidSrcXmlDetector : ResourceXmlDetector() {
    companion object {
        const val SCHEMA = "http://schemas.android.com/apk/res/android"
        const val FULLY_QUALIFIED_APP_COMPAT_IMAGE_BUTTON =
            "androidx.appcompat.widget.AppCompatImageButton"
        const val FULLY_QUALIFIED_APP_COMPAT_VIEW_CLASS =
            "androidx.appcompat.widget.AppCompatImageView"
        const val APP_COMPAT_IMAGE_BUTTON = "AppCompatImageButton"
        const val APP_COMPAT_IMAGE_VIEW = "AppCompatImageView"

        const val ERROR_MESSAGE = "Using android:src to define resource instead of app:srcCompat"

        @VisibleForTesting
        val ISSUE_XML_SRC_USAGE = Issue.create(
            id = "AndroidSrcXmlDetector",
            briefDescription = "Prohibits using android:src in ImageViews and ImageButtons",
            explanation = "ImageView (and descendants) images should be declared using app:srcCompat",
            category = Category.CORRECTNESS,
            severity = Severity.ERROR,
            implementation = Implementation(
                AndroidSrcXmlDetector::class.java,
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
            FQCN_IMAGE_VIEW,
            IMAGE_VIEW,
            FQCN_IMAGE_BUTTON,
            IMAGE_BUTTON,
            FULLY_QUALIFIED_APP_COMPAT_IMAGE_BUTTON,
            FULLY_QUALIFIED_APP_COMPAT_VIEW_CLASS,
            APP_COMPAT_IMAGE_BUTTON,
            APP_COMPAT_IMAGE_VIEW,
        )
    }

    override fun visitElement(context: XmlContext, element: Element) {
        if (!element.hasAttributeNS(SCHEMA, ATTR_SRC)) return
        val node = element.getAttributeNodeNS(SCHEMA, ATTR_SRC)

        context.report(
            issue = ISSUE_XML_SRC_USAGE,
            scope = node,
            location = context.getLocation(node),
            message = ERROR_MESSAGE,
        )
    }
}
