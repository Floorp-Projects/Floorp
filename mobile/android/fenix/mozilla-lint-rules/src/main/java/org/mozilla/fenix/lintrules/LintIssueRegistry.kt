/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules

import com.android.tools.lint.client.api.IssueRegistry
import com.android.tools.lint.client.api.Vendor
import com.android.tools.lint.detector.api.Issue
import org.mozilla.fenix.lintrules.perf.ConstraintLayoutPerfDetector

/**
 * Registry which provides a list of our custom lint checks to be performed on an Android project.
 */
@Suppress("unused")
class LintIssueRegistry : IssueRegistry() {
    override val api: Int = com.android.tools.lint.detector.api.CURRENT_API
    override val issues: List<Issue> = listOf(
        ButtonStyleXmlDetector.ISSUE_XML_STYLE,
        AndroidSrcXmlDetector.ISSUE_XML_SRC_USAGE,
        TextViewAndroidSrcXmlDetector.ISSUE_XML_SRC_USAGE,
        ImageViewAndroidTintXmlDetector.ISSUE_XML_SRC_USAGE,
        LicenseDetector.ISSUE_MISSING_LICENSE,
        LicenseDetector.ISSUE_INVALID_LICENSE_FORMAT
    ) + ConstraintLayoutPerfDetector.ISSUES + ContextCompatDetector.ISSUES
    override val vendor: Vendor = Vendor(
        vendorName = "Mozilla",
        identifier = "mozilla-fenix"
    )
}
