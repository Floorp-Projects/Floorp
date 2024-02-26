/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_DOCS
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar

class PDFViewerTest : TestSetup() {
    private val downloadTestPage =
        "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
    private val pdfFileName = "washington.pdf"
    private val pdfFileURL = "storage.googleapis.com/mobile_test_assets/public/washington.pdf"
    private val pdfFileContent = "Washington Crossing the Delaware"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2048140
    @SmokeTest
    @Test
    fun verifyPDFFileIsOpenedInTheSameTabTest() {
        val genericURL =
            getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemContainingText("PDF form file"))
            verifyPageContent("Washington Crossing the Delaware")
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2159718
    @Test
    fun verifyPDFViewerOpenInAppButtonTest() {
        val genericURL = getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF form file"))
            verifyPDFReaderToolbarItems()
            clickPageObject(itemWithResIdAndText("openInApp", "Open in app"))
            assertExternalAppOpens(GOOGLE_DOCS)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2145448
    // Download PDF file using the download toolbar button
    @Test
    fun verifyPDFViewerDownloadButtonTest() {
        val genericURL = getGenericAsset(mockWebServer, 3)
        val downloadFile = "pdfForm.pdf"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF form file"))
        }.clickDownloadPDFButton {
            verifyDownloadedFileName(downloadFile)
        }.clickOpen("application/pdf") {
            assertExternalAppOpens(GOOGLE_DOCS)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2283305
    @Test
    fun pdfFindInPageTest() {
        val genericURL = getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(MatcherHelper.itemWithText("PDF form file"))
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyFindInPageButton()
        }.openFindInPage {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("l")
            verifyFindInPageResult("1/2")
            clickFindInPageNextButton()
            verifyFindInPageResult("2/2")
            clickFindInPagePrevButton()
            verifyFindInPageResult("1/2")
        }.closeFindInPageWithCloseButton {
            verifyFindInPageBar(false)
        }.openThreeDotMenu {
        }.openFindInPage {
            enterFindInPageQuery("p")
            verifyFindInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2284297
    @Test
    fun addPDFToHomeScreenTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            clickPageObject(MatcherHelper.itemContainingText(pdfFileName))
            verifyUrl(pdfFileURL)
            verifyPageContent(pdfFileContent)
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            verifyShortcutTextFieldTitle(pdfFileName)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(pdfFileName) {
            verifyUrl(pdfFileURL)
        }
    }
}
