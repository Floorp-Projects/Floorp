/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.tools.lint.client.api.UElementHandler
import com.android.tools.lint.detector.api.*
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UFile

class LicenseDetector : Detector(), SourceCodeScanner {

    companion object {

        private val Implementation = Implementation(
            LicenseDetector::class.java,
            Scope.JAVA_FILE_SCOPE,
        )

        val ISSUE_MISSING_LICENSE = Issue.create(
            id = "MissingLicense",
            briefDescription = "File doesn't start with the license comment",
            explanation = "Every file must start with the license comment:\n" +
                LicenseCommentChecker.ValidLicenseForKotlinFiles,
            category = Category.CORRECTNESS,
            severity = Severity.WARNING,
            implementation = Implementation,
        )

        val ISSUE_INVALID_LICENSE_FORMAT = Issue.create(
            id = "InvalidLicenseFormat",
            briefDescription = "License isn't formatted correctly",
            explanation = "The license must be:\n${LicenseCommentChecker.ValidLicenseForKotlinFiles}",
            category = Category.CORRECTNESS,
            severity = Severity.WARNING,
            implementation = Implementation,
        )
    }

    override fun getApplicableUastTypes(): List<Class<out UElement>>? = listOf(UFile::class.java)

    override fun createUastHandler(context: JavaContext): UElementHandler? =
        LicenseCommentChecker(context)
}
