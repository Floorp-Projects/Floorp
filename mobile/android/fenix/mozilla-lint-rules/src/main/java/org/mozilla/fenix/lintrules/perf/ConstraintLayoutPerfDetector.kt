/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lintrules.perf

import com.android.resources.ResourceFolderType
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Context
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.ResourceXmlDetector
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.XmlContext
import org.w3c.dom.Element

private val FRAMEWORK_ELEMENTS = setOf(
    "androidx.constraintlayout.widget.ConstraintLayout",

    // Android framework views that extend ConstraintLayout
    "androidx.constraintlayout.motion.widget.MotionLayout",
)
private val FRAMEWORK_ELEMENTS_BULLETED_LIST = FRAMEWORK_ELEMENTS.map { "- `$it`" }.joinToString("\n")

private const val FENIX_PREFIX = "org.mozilla.fenix"
private const val AC_PREFIX = "mozilla.components"

/**
 * Custom views that we wrote that extend ConstraintLayout.
 *
 * This data was manually added from the Type Hierarchy IDE action. Last update: 8/5/2020.
 * In the future, we could generate this list statically to prevent transcription errors and
 * errors from refactors.
 */
private val CUSTOM_VIEW_ELEMENTS = setOf(
    "$FENIX_PREFIX.library.LibrarySiteItemView",
    "$FENIX_PREFIX.settings.deletebrowsingdata.DeleteBrowsingDataItem",
    "$FENIX_PREFIX.trackingprotection.SwitchWithDescription",
    "$FENIX_PREFIX.trackingprotection.TrackingProtectionCategoryItem",

    "$AC_PREFIX.feature.readerview.view.ReaderViewControlsBar",
    "$AC_PREFIX.feature.findinpage.view.FindInPageBar",
    "$AC_PREFIX.feature.prompts.login.LoginSelectBar",
    "$AC_PREFIX.ui.widgets.WidgetSiteItemView",
    "$AC_PREFIX.browser.toolbar.display.DisplayToolbarView",
)
private val CUSTOM_VIEW_ELEMENTS_BULLETED_LIST = CUSTOM_VIEW_ELEMENTS.map { "- `$it`" }.joinToString("\n")

private val ALL_APPLICABLE_ELEMENTS = FRAMEWORK_ELEMENTS + CUSTOM_VIEW_ELEMENTS

private const val FRAMEWORK_ISSUE_DESCRIPTION = "2+ ConstraintLayouts in one hierarchy can be inefficient"
private const val FRAMEWORK_ISSUE_MESSAGE = "Flatten the view hierarchy by using one `ConstraintLayout`, " +
    "if possible. If the alternative is several nested `ViewGroup`, it may not help performance and " +
    "this may be worth suppressing."
private val FRAMEWORK_ISSUE_EXPLANATION = """`ConstraintLayout`'s main performance benefit is
    that it enables devs to define layouts with very little view nesting. However, they are slow to
    inflate so we only want to use as many as are strictly necessary. In most cases, this is one: a
    layout can have a ConstraintLayout defined at the root and the entire hierarchy can be a direct
    descendant of it.

    This check will report if there is more than one ConstraintLayout, or a view that extends it,
    defined in a single layout file to remind the implementor of these performance concerns.

    **WARNING: replacing a ConstraintLayout with several nested ViewGroups is unlikely to improve
    performance. Remember, a goal is to reduce nesting. If you're unable to remove the extra
    ConstraintLayout(s) without doing this, it's probably better to suppress this violation.**

    Here are **known cases where ConstraintLayouts are unnecessarily nested and the alternatives:**
    - To set a background color. Instead, use a `<view>` of the appropriate size directly in the ConstraintLayout
    - To change visibility on a group of sub-views. Instead, use `androidx.constraintlayout.widget.Group`

    There are a few cases the perf team is not 100% sure of the best solution - let us know if you
    have better solutions:
    - ScrollView can only have one child so perhaps making it a ConstraintLayout is fine, despite nesting
    - a11y may rely on nesting views. Maybe `Group` solves this problem?

    The following views that are, or that extend, ConstraintLayout are inspected in this check:
""".trimIndent()
    .lines()
    .joinToStringRemovingManualWrapping() +
    "\n$FRAMEWORK_ELEMENTS_BULLETED_LIST\n\n"

private val CUSTOM_VIEW_ISSUE_DESCRIPTION = "Caution regarding custom views extending `ConstraintLayout`"
private val CUSTOM_VIEW_ISSUE_MESSAGE = "Custom views extending `ConstraintLayout` are less " +
    "efficient because they cannot share other `ConstraintLayout` defined in file."
private val CUSTOM_VIEW_ISSUE_EXPLANATION = """`ConstraintLayout` is slow to inflate so we
    should aim to have as few as possible. However, the performance team doesn't have a good generic
    solution for when custom views extend ConstraintLayout because the encapsulation may be worth
    it and the replacement may not be more performant. Therefore, we warn the implementor that this
    may be undesireable but don't fail the build on it.

    More generic information about the performance concerns of ConstraintLayout is found below,
    copied from our more strict ConstraintLayout check.

    The following additional views are inspected for this less strict check:
""".trimIndent()
    .lines()
    .joinToStringRemovingManualWrapping() +
    "\n$CUSTOM_VIEW_ELEMENTS_BULLETED_LIST\n\n" +
    "---\n\n" + // divider
    FRAMEWORK_ISSUE_EXPLANATION

/**
 * An issue where multiple ConstraintLayouts that are defined by the framework are detected in the
 * same file. This is distinct from [CUSTOM_VIEW_ISSUE] because that one handles custom views.
 * We don't want to combine them because solving a custom view ConstraintLayout problem rarely has
 * a clear cut solution and we don't want to fail the build on checks that aren't clear cut.
 */
private val FRAMEWORK_ISSUE = Issue.create(
    id = "MozMultipleConstraintLayouts",
    briefDescription = FRAMEWORK_ISSUE_DESCRIPTION,
    explanation = FRAMEWORK_ISSUE_EXPLANATION,
    category = Category.PERFORMANCE,
    priority = 8, // UseCompoundDrawables is a 6 and I think this is much more impactful.
    severity = Severity.ERROR,
    implementation = Implementation(
        ConstraintLayoutPerfDetector::class.java,
        Scope.RESOURCE_FILE_SCOPE,
    ),
)

/**
 * An issue where multiple ConstraintLayouts, that may also be defined by us as custom views, are
 * detected in the same file. See [FRAMEWORK_ISSUE] for why these are distinct.
 */
private val CUSTOM_VIEW_ISSUE = Issue.create(
    id = "MozMultipleConstraintLayoutsAndCustomViews",
    briefDescription = CUSTOM_VIEW_ISSUE_DESCRIPTION,
    explanation = CUSTOM_VIEW_ISSUE_EXPLANATION,
    category = Category.PERFORMANCE,
    priority = 6, // UseCompoundDrawables is a 6 and this seems more impactful but less actionable.
    severity = Severity.WARNING,
    implementation = Implementation(
        ConstraintLayoutPerfDetector::class.java,
        Scope.RESOURCE_FILE_SCOPE,
    ),
)

/**
 * A custom lint detector that helps us ensure we're using ConstraintLayout performantly.
 *
 * This detector has a few shortcomings:
 * - it will not notice if a ConstraintLayout is in the same view hierarchy at runtime but added
 * from a different file (e.g. via <include>)
 * - it will not notice if a ConstraintLayout is added at runtime
 * - if we add additional views that extend ConstraintLayout, this check won't know about them
 * (though we could generate this information and include it in this check via static analysis)
 */
class ConstraintLayoutPerfDetector : ResourceXmlDetector() {

    companion object {
        val ISSUES = listOf(
            FRAMEWORK_ISSUE,
            CUSTOM_VIEW_ISSUE,
        )
    }

    private var frameworkConstraintLayoutCountInFile = 0
    private var customViewConstraintLayoutCountInFile = 0

    override fun appliesTo(folderType: ResourceFolderType): Boolean = folderType == ResourceFolderType.LAYOUT
    override fun getApplicableElements(): Collection<String>? = ALL_APPLICABLE_ELEMENTS

    override fun beforeCheckFile(context: Context) {
        frameworkConstraintLayoutCountInFile = 0
        customViewConstraintLayoutCountInFile = 0
    }

    override fun visitElement(context: XmlContext, element: Element) {
        // This scope is unideal: if the root element is a ConstraintLayout and is suppressed, all
        // ConstraintLayout children will also be suppressed. If more ConstraintLayout children are
        // added, the root ConstraintLayout will suppress them too. Ideally, we'd want a suppression
        // to only affect a node and not its children.
        val scope = element
        val location = context.getElementLocation(element)

        // For these inner visit methods, we only warn on 2+ violations because:
        // - we avoid having 2 warnings for the first intuitive violation (i.e. 2 ConstraintLayouts
        // in a file is one violation on the second ConstraintLayout)
        // - of the suppression & scope issues mentioned above. The first found element is most
        // likely to be a parent of other ConstraintLayouts because of the traversal order so if
        // there's one node we don't report on, that's the best one.
        //
        // The fact that we use two different issues can create a weird situation where if a custom
        // view is a sibling above a ConstraintLayout, it won't report but if it's below the
        // ConstraintLayout, it will report. I don't think it's worth the complexity to address this
        // and in-IDE inspection seems to break when we use afterCheckFile, which would be useful for this.
        fun visitFrameworkElement() {
            frameworkConstraintLayoutCountInFile += 1
            if (frameworkConstraintLayoutCountInFile > 1) {
                context.report(FRAMEWORK_ISSUE, scope, location, FRAMEWORK_ISSUE_MESSAGE)
            }
        }

        fun visitCustomViewElement() {
            customViewConstraintLayoutCountInFile += 1

            // We want to report if there are too many ConstraintLayouts in the file. This issue
            // intentionally includes both framework CL and our custom view CL so we add both counts.
            if (frameworkConstraintLayoutCountInFile + customViewConstraintLayoutCountInFile > 1) {
                context.report(CUSTOM_VIEW_ISSUE, scope, location, CUSTOM_VIEW_ISSUE_MESSAGE)
            }
        }

        when (element.tagName) {
            in FRAMEWORK_ELEMENTS -> visitFrameworkElement()
            in CUSTOM_VIEW_ELEMENTS -> visitCustomViewElement()
            else -> throw IllegalStateException("Unexpected tag seen: ${element.tagName}")
        }
    }
}

/**
 * Takes paragraphs wrapped by hand and combines them into a single line, preserving white space
 * between paragraphs.
 */
private fun List<String>.joinToStringRemovingManualWrapping(): String = fold("") { acc, str ->
    val toAppend = when {
        str.isBlank() -> "\n\n" // new paragraph
        str.trim().startsWith("-") -> "\n$str" // bulleted list
        else -> str
    }
    acc + toAppend
}
