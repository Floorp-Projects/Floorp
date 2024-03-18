/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.tools.lint.client.api.UElementHandler
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.LintFix
import org.jetbrains.kotlin.psi.psiUtil.siblings
import org.jetbrains.kotlin.psi.psiUtil.startsWithComment
import org.jetbrains.uast.UComment
import org.jetbrains.uast.UFile

class LicenseCommentChecker(private val context: JavaContext) : UElementHandler() {

    companion object {
        val ValidLicenseForKotlinFiles = """
            |/* This Source Code Form is subject to the terms of the Mozilla Public
            | * License, v. 2.0. If a copy of the MPL was not distributed with this
            | * file, You can obtain one at http://mozilla.org/MPL/2.0/. */""".trimMargin()
    }

    override fun visitFile(node: UFile) {
        if (!node.sourcePsi.startsWithComment()) {
            reportMissingLicense(node)
        } else {
            val firstComment = node.allCommentsInFile.first()
            if (firstComment.text != ValidLicenseForKotlinFiles) {
                reportInvalidLicenseFormat(firstComment)
            } else {
                val nextSibling =
                    firstComment.sourcePsi.siblings(withItself = false).firstOrNull()
                if (nextSibling?.text != "\n\n") {
                    reportMissingLeadingNewLineCharacter(firstComment)
                }
            }
        }
    }

    private fun reportMissingLicense(node: UFile) = context.report(
        LicenseDetector.ISSUE_MISSING_LICENSE,
        context.getLocation(node.sourcePsi.firstChild),
        "The file must start with a comment containing the license",
        addLicenseQuickFix(),
    )

    private fun reportInvalidLicenseFormat(comment: UComment) = context.report(
        LicenseDetector.ISSUE_INVALID_LICENSE_FORMAT,
        context.getLocation(comment),
        "The license comment doesn't have the appropriate format",
        replaceCommentWithValidLicenseFix(comment),
    )

    private fun reportMissingLeadingNewLineCharacter(licenseComment: UComment) = context.report(
        LicenseDetector.ISSUE_INVALID_LICENSE_FORMAT,
        context.getRangeLocation(licenseComment, licenseComment.text.lastIndex, 1),
        "The license comment must be followed by a newline character",
        addLeadingNewLineQuickFix(licenseComment),
    )

    private fun addLicenseQuickFix() = LintFix.create()
        .name("Insert license at the top of the file")
        .replace()
        .beginning()
        .with(ValidLicenseForKotlinFiles + "\n\n")
        .autoFix()
        .build()

    private fun replaceCommentWithValidLicenseFix(comment: UComment) = LintFix.create()
        .name("Replace with correctly formatted license")
        .replace()
        .range(context.getLocation(comment))
        .with(ValidLicenseForKotlinFiles)
        .build()

    private fun addLeadingNewLineQuickFix(licenseComment: UComment) = LintFix.create()
        .name("Insert newline character after the license")
        .replace()
        .range(context.getLocation(licenseComment))
        .with(ValidLicenseForKotlinFiles + "\n")
        .autoFix()
        .build()
}
