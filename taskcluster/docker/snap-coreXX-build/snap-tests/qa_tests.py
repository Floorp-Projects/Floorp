#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.


import os
import random
import tempfile
import time

from basic_tests import SnapTestsBase
from selenium.common.exceptions import TimeoutException
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.select import Select


class QATests(SnapTestsBase):
    def __init__(self):
        self._dir = "qa_tests"

        super(QATests, self).__init__(
            exp=os.path.join(self._dir, "qa_expectations.json")
        )

    def _test_audio_playback(
        self, url, iframe_selector=None, click_to_play=False, video_selector=None
    ):
        self._logger.info("open url {}".format(url))
        if url:
            self.open_tab(url)

        if iframe_selector:
            self._logger.info("find iframe")
            iframe = self._wait.until(
                EC.visibility_of_element_located((By.CSS_SELECTOR, iframe_selector))
            )
            self._driver.switch_to.frame(iframe)

        self._logger.info("find video")
        video = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, video_selector or "video")
            )
        )
        self._wait.until(lambda d: type(video.get_property("duration")) == float)
        assert video.get_property("duration") > 0.0, "<video> duration null"

        # For HE-AAC page, Google Drive does not like SPACE
        if not click_to_play and video.get_property("autoplay") is False:
            self._logger.info("force play")
            video.send_keys(Keys.SPACE)

        # Mostly for Google Drive video, click()/play() seems not to really
        # work to trigger, but 'k' is required
        if click_to_play:
            self._driver.execute_script("arguments[0].click();", video)
            video.send_keys("k")

        ref_volume = video.get_property("volume")

        self._logger.info("find video: wait readyState")
        self._wait.until(lambda d: video.get_property("readyState") >= 4)

        # Some videos sometimes self-pause?
        self._logger.info(
            "find video: check paused: {}".format(video.get_property("paused"))
        )
        self._logger.info(
            "find video: check autoplay: {}".format(video.get_property("autoplay"))
        )
        if not click_to_play and video.get_property("paused") is True:
            self._driver.execute_script("arguments[0].play()", video)

        self._logger.info("find video: sleep")
        # let it play at least 500ms
        time.sleep(0.5)

        self._logger.info("find video: wait currentTime")
        self._wait.until(lambda d: video.get_property("currentTime") >= 0.01)
        assert (
            video.get_property("currentTime") >= 0.01
        ), "<video> currentTime not moved"

        # this should pause
        self._logger.info("find video: pause")
        if click_to_play:
            video.send_keys("k")
        else:
            self._driver.execute_script("arguments[0].pause()", video)
        datum = video.get_property("currentTime")
        time.sleep(1)
        datum_after_sleep = video.get_property("currentTime")
        self._logger.info(
            "datum={} datum_after_sleep={}".format(datum, datum_after_sleep)
        )
        assert datum == datum_after_sleep, "<video> is sleeping"
        assert video.get_property("paused") is True, "<video> is paused"

        self._logger.info("find video: unpause")
        # unpause and verify playback
        if click_to_play:
            video.send_keys("k")
        else:
            self._driver.execute_script("arguments[0].play()", video)
        assert video.get_property("paused") is False, "<video> is not paused"
        time.sleep(2)
        datum_after_resume = video.get_property("currentTime")
        self._logger.info(
            "datum_after_resume={} datum_after_sleep={}".format(
                datum_after_resume, datum_after_sleep
            )
        )
        # we wait for 2s but it's not super accurate on CI (vbox VMs?),
        # observed values +/- 15% so check for more that should avoid
        # intermittent failures
        assert (
            datum_after_resume >= datum_after_sleep + 0.5
        ), "<video> progressed after pause"

        self._logger.info("find video: volume")
        self._driver.execute_script(
            "arguments[0].volume = arguments[1]", video, ref_volume * 0.25
        )
        assert (
            video.get_property("volume") == ref_volume * 0.25
        ), "<video> sound volume increased"

        self._logger.info("find video: done")

    def _test_audio_video_playback(self, url):
        self._logger.info("open url {}".format(url))
        self.open_tab(url)
        self._logger.info("find thumbnail")
        thumbnail = self._longwait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "img"))
        )
        self._logger.info("click")
        self._driver.execute_script("arguments[0].click()", thumbnail)
        self._logger.info("audio test")
        self._test_audio_playback(
            url=None,
            iframe_selector="#drive-viewer-video-player-object-0",
            click_to_play=True,
        )

        self._logger.info("find video again")
        # we are still in the iframe
        video = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "video"))
        )
        self._logger.info("try fullscreen")
        promise = self._driver.execute_script(
            "return arguments[0].requestFullscreen().then(() => { return true; }).catch(() => { return false; })",
            video,
        )
        assert promise is True, "<video> full screen promised fullfilled"

        self._driver.execute_script("arguments[0].pause();", video)
        self._driver.execute_script("document.exitFullscreen()")

    def test_h264_mov(self, exp):
        """
        C95233
        """

        self._test_audio_video_playback(
            "https://drive.google.com/file/d/0BwxFVkl63-lEY3l3ODJReDg3RzQ/view?resourcekey=0-5kDw2QbFk9eLrWE1N9M1rQ"
        )

        return True

    def test_he_aac(self, exp):
        """
        C95239
        """
        self._test_audio_playback(
            url="https://www2.iis.fraunhofer.de/AAC/multichannel.html",
            video_selector="p.inlineVideo > video",
        )

        return True

    def test_flac(self, exp):
        """
        C95240
        """

        self._test_audio_playback(
            "http://www.hyperion-records.co.uk/audiotest/18%20MacCunn%20The%20Lay%20of%20the%20Last%20Minstrel%20-%20Part%202%20Final%20chorus%20O%20Caledonia!%20stern%20and%20wild.FLAC"
        )

        return True

    def test_mp3(self, exp):
        """
        C95241
        """
        self._test_audio_playback(
            "https://freetestdata.com/wp-content/uploads/2021/09/Free_Test_Data_5MB_MP3.mp3"
        )

        return True

    def test_ogg(self, exp):
        """
        C95244
        """
        self._test_audio_playback(
            "http://www.metadecks.org/software/sweep/audio/demos/beats1.ogg"
        )

        return True

    def test_custom_fonts(self, exp):
        """
        C128146
        """

        self.open_tab(
            "http://codinginparadise.org/projects/svgweb/samples/demo.html?name=droid%20font1"
        )

        renderer = self._wait.until(
            EC.visibility_of_element_located((By.ID, "selectRenderer"))
        )
        self._wait.until(lambda d: len(renderer.text) > 0)

        renderer_drop = Select(renderer)
        renderer_drop.select_by_visible_text("browser native svg")

        font = self._wait.until(EC.visibility_of_element_located((By.ID, "selectSVG")))
        self._wait.until(lambda d: len(font.text) > 0)

        font_drop = Select(font)
        font_drop.select_by_value("droid font1")

        svg_div = self._wait.until(
            EC.visibility_of_element_located((By.ID, "__svg__random___1__object"))
        )
        self._wait.until(lambda d: svg_div.is_displayed() is True)

        self.assert_rendering(exp, svg_div)

        return True

    def pdf_select_zoom(self, value):
        pdf_zoom = self._wait.until(
            EC.visibility_of_element_located((By.ID, "scaleSelect"))
        )
        self._wait.until(lambda d: len(pdf_zoom.text) > 0)

        pdf_zoom_drop = Select(pdf_zoom)
        pdf_zoom_drop.select_by_value(value)

    def pdf_wait_div(self):
        pdf_div = self._wait.until(EC.visibility_of_element_located((By.ID, "viewer")))
        self._wait.until(lambda d: pdf_div.is_displayed() is True)
        return pdf_div

    def pdf_get_page(self, page, long=False):
        waiter = self._longwait if long is True else self._wait
        page = waiter.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, "div.page[data-page-number='{}'] canvas".format(page))
            )
        )

        if not self.is_esr():
            self._wait.until(
                lambda d: d.execute_script(
                    'return window.getComputedStyle(document.querySelector(".loadingInput.start"), "::after").getPropertyValue("visibility");'
                )
                != "visible"
            )
            # PDF.js can take time to settle and we don't have a nice way to wait
            # for an event on it
            time.sleep(1)
        else:
            self._logger.info("Running against ESR, just wait too much.")
            # Big but let's be safe, this is only for ESR because its PDF.js
            # does not have "<span class='loadingInput start'>"
            time.sleep(10)

        # Rendering can be slower on debug build so give more time to settle
        if self.is_debug_build():
            time.sleep(3)

        return page

    def pdf_go_to_page(self, page):
        pagenum = self._wait.until(
            EC.visibility_of_element_located((By.ID, "pageNumber"))
        )
        pagenum.send_keys(Keys.BACKSPACE)
        pagenum.send_keys("{}".format(page))

    def test_pdf_navigation(self, exp):
        """
        C3927
        """

        self.open_tab("http://www.pdf995.com/samples/pdf.pdf")

        # Test basic rendering
        self.pdf_wait_div()
        self.pdf_select_zoom("1")
        self.pdf_get_page(1)
        self.assert_rendering(exp["base"], self._driver)

        # Navigating to page X, we know the PDF has 5 pages.
        rand_page = random.randint(1, 5)
        self.pdf_go_to_page(rand_page)
        # the click step ensures we change page
        self.pdf_wait_div().click()
        # getting page X will wait on is_displayed() so if page X is not visible
        # this will timeout
        self.pdf_get_page(rand_page)

        # press down/up/right/left/PageDown/PageUp/End/Home
        key_presses = [
            (Keys.DOWN, "down"),
            (Keys.UP, "up"),
            (Keys.RIGHT, "right"),
            (Keys.LEFT, "left"),
            (Keys.PAGE_DOWN, "pagedown"),
            (Keys.PAGE_UP, "pageup"),
            (Keys.END, "end"),
            (Keys.HOME, "home"),
        ]

        for key, ref in key_presses:
            # reset to page 2
            self.pdf_go_to_page(2)
            pdfjs = self._wait.until(
                EC.visibility_of_element_located((By.CSS_SELECTOR, "html"))
            )
            pdfjs.send_keys(key)
            self.pdf_get_page(2)
            # give some time for rendering to update
            time.sleep(0.2)
            self._logger.info("assert {}".format(ref))
            self.assert_rendering(exp[ref], self._driver)

        # click Next/Previous page
        self.pdf_go_to_page(1)
        button_next = self._wait.until(
            EC.visibility_of_element_located((By.ID, "next"))
        )
        button_next.click()
        button_next.click()
        self._logger.info("assert next twice 1 => 3")
        self.assert_rendering(exp["next"], self._driver)

        button_previous = self._wait.until(
            EC.visibility_of_element_located((By.ID, "previous"))
        )
        button_previous.click()
        self._logger.info("assert previous 3 => 2")
        self.assert_rendering(exp["previous"], self._driver)

        secondary_menu = self._wait.until(
            EC.visibility_of_element_located((By.ID, "secondaryToolbarToggle"))
        )

        # Use tools button
        #  - first/lage page
        #  - rotate left/right
        #  - doc properties
        menu_buttons = [
            "firstPage",
            "lastPage",
            "pageRotateCw",
            "pageRotateCcw",
            "documentProperties",
        ]

        for menu_id in menu_buttons:
            self._logger.info("reset to page for {}".format(menu_id))
            if menu_id != "firstPage":
                self.pdf_go_to_page(1)
            else:
                self.pdf_go_to_page(2)
            time.sleep(0.2)

            self._logger.info("click menu for {}".format(menu_id))
            # open menu
            secondary_menu.click()

            self._logger.info("find button for {}".format(menu_id))
            button_to_test = self._wait.until(
                EC.visibility_of_element_located((By.ID, menu_id))
            )

            self._logger.info("click button for {}".format(menu_id))
            button_to_test.click()

            # rotation does not close the menu?:
            if menu_id == "pageRotateCw" or menu_id == "pageRotateCcw":
                secondary_menu.click()

            time.sleep(0.75)

            self._logger.info("assert {}".format(menu_id))
            if self.is_esr() and menu_id == "documentProperties":
                # on ESR pdf.js misreports in mm instead of inches
                title = self._wait.until(
                    EC.visibility_of_element_located((By.ID, "titleField"))
                )
                author = self._wait.until(
                    EC.visibility_of_element_located((By.ID, "authorField"))
                )
                subject = self._wait.until(
                    EC.visibility_of_element_located((By.ID, "subjectField"))
                )
                version = self._wait.until(
                    EC.visibility_of_element_located((By.ID, "versionField"))
                )
                assert title.text == "PDF", "Incorrect PDF title reported: {}".format(
                    title
                )
                assert (
                    author.text == "Software 995"
                ), "Incorrect PDF author reported: {}".format(author)
                assert (
                    subject.text == "Create PDF with Pdf 995"
                ), "Incorrect PDF subject reported: {}".format(subject)
                assert (
                    version.text == "1.3"
                ), "Incorrect PDF version reported: {}".format(version)
            else:
                self.assert_rendering(exp[menu_id], self._driver)

            if menu_id == "documentProperties":
                close = self._wait.until(
                    EC.visibility_of_element_located((By.ID, "documentPropertiesClose"))
                )
                close.click()

        self.pdf_go_to_page(1)

        #  - select text
        secondary_menu.click()
        text_selection = self._wait.until(
            EC.visibility_of_element_located((By.ID, "cursorSelectTool"))
        )
        text_selection.click()

        action = ActionChains(self._driver)
        paragraph = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, "span[role=presentation]")
            )
        )
        action.drag_and_drop_by_offset(paragraph, 50, 10).perform()
        time.sleep(0.75)
        try:
            ref_screen_source = "select_text_with_highlight"
            self._wait.until(
                EC.visibility_of_element_located(
                    (By.CSS_SELECTOR, "button.highlightButton")
                )
            )
        except TimeoutException:
            ref_screen_source = "select_text_without_highlight"
            self._logger.info(
                "Wait for pdf highlight button: timed out, maybe it is not there"
            )
        finally:
            time.sleep(0.75)
            self.assert_rendering(exp[ref_screen_source], self._driver)

        # release select selection
        action.move_by_offset(0, 150).perform()
        action.click()
        # make sure we go back to page 1
        self.pdf_go_to_page(1)

        #  - hand tool
        secondary_menu.click()
        hand_tool = self._wait.until(
            EC.visibility_of_element_located((By.ID, "cursorHandTool"))
        )
        hand_tool.click()
        action.drag_and_drop_by_offset(paragraph, 0, -200).perform()
        self.assert_rendering(exp["hand_tool"], self._driver)

        return True

    def test_pdf_zoom(self, exp):
        """
        C3929
        """

        self.open_tab("http://www.pdf995.com/samples/pdf.pdf")

        self.pdf_wait_div()

        zoom_levels = [
            ("1", 1, "p1_100p"),
            ("0.5", 1, "p1_50p"),
            ("0.75", 1, "p1_75p"),
            ("1.5", 1, "p1_150p"),
            ("4", 1, "p1_400p"),
            ("page-actual", 1, "p1_actual"),
            ("page-fit", 1, "p1_fit"),
            ("page-width", 1, "p1_width"),
        ]

        for zoom, page, ref in zoom_levels:
            self.pdf_select_zoom(zoom)
            self.pdf_get_page(page, long=True)
            self._logger.info("assert {}".format(ref))
            self.assert_rendering(exp[ref], self._driver)

        return True

    def test_pdf_download(self, exp):
        """
        C936503
        """

        self.open_tab(
            "https://file-examples.com/index.php/sample-documents-download/sample-pdf-download/"
        )

        try:
            consent = self._wait.until(
                EC.visibility_of_element_located((By.CSS_SELECTOR, ".fc-cta-consent"))
            )
            consent.click()
        except TimeoutException:
            self._logger.info("Wait for consent form: timed out, maybe it is not here")

        for iframe in self._driver.find_elements(By.CSS_SELECTOR, "iframe"):
            self._driver.execute_script("arguments[0].remove();", iframe)

        download_button = self._wait.until(
            EC.presence_of_element_located((By.CSS_SELECTOR, ".download-button"))
        )
        self._driver.execute_script("arguments[0].scrollIntoView();", download_button)
        self._driver.execute_script("this.window.scrollBy(0, -100);")
        self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, ".download-button"))
        )
        # clicking seems to break on CI because we nuke ads
        self._driver.get(download_button.get_property("href"))

        self.pdf_get_page(1)

        self.assert_rendering(exp, self._driver)

        return True

    def context_menu_copy(self, element, mime_type):
        action = ActionChains(self._driver)

        # Open context menu and copy
        action.context_click(element).perform()
        self._driver.set_context("chrome")
        context_menu = self._wait.until(
            EC.visibility_of_element_located((By.ID, "contentAreaContextMenu"))
        )
        copy = self._wait.until(
            EC.visibility_of_element_located(
                (
                    By.ID,
                    "context-copyimage-contents"
                    if mime_type.startswith("image/")
                    else "context-copy",
                )
            )
        )
        copy.click()
        self.wait_for_element_in_clipboard(mime_type, False)
        context_menu.send_keys(Keys.ESCAPE)

        # go back to content context
        self._driver.set_context("content")

    def verify_clipboard(self, mime_type, should_be_present):
        self._driver.set_context("chrome")
        in_clipboard = self._driver.execute_script(
            "return Services.clipboard.hasDataMatchingFlavors([arguments[0]], Ci.nsIClipboard.kGlobalClipboard);",
            mime_type,
        )
        self._driver.set_context("content")
        assert (
            in_clipboard == should_be_present
        ), "type {} should/should ({}) not be in clipboard".format(
            mime_type, should_be_present
        )

    def wait_for_element_in_clipboard(self, mime_type, context_change=False):
        if context_change:
            self._driver.set_context("chrome")
        self._wait.until(
            lambda d: self._driver.execute_script(
                "return Services.clipboard.hasDataMatchingFlavors([arguments[0]], Ci.nsIClipboard.kGlobalClipboard);",
                mime_type,
            )
            is True
        )
        if context_change:
            self._driver.set_context("content")

    def test_copy_paste_image_text(self, exp):
        """
        C464474
        """

        mystor = self.open_tab("https://mystor.github.io/dragndrop/#")
        images = self.open_tab("https://1stwebdesigner.com/image-file-types/")

        image = self._wait.until(
            EC.presence_of_element_located((By.CSS_SELECTOR, ".wp-image-42224"))
        )
        self._driver.execute_script("arguments[0].scrollIntoView();", image)
        self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, ".wp-image-42224"))
        )
        self.verify_clipboard("image/png", False)
        self.context_menu_copy(image, "image/png")
        self.verify_clipboard("image/png", True)

        self._driver.switch_to.window(mystor)
        link = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, "#testlist > li:nth-child(11) > a:nth-child(1)")
            )
        )
        link.click()
        drop_area = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "html"))
        )
        drop_area.click()
        drop_area.send_keys(Keys.CONTROL + "v")
        self.verify_clipboard("image/png", True)

        matching_text = self._wait.until(
            EC.visibility_of_element_located((By.ID, "matching"))
        )
        assert matching_text.text == "MATCHING", "copy/paste image should match"

        self._driver.switch_to.window(images)
        text = self._wait.until(
            EC.presence_of_element_located(
                (By.CSS_SELECTOR, ".entry-content > p:nth-child(1)")
            )
        )
        self._driver.execute_script("arguments[0].scrollIntoView();", text)

        action = ActionChains(self._driver)
        action.drag_and_drop_by_offset(text, 50, 10).perform()

        self.context_menu_copy(text, "text/plain")

        self._driver.switch_to.window(mystor)
        link = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, "#testlist > li:nth-child(12) > a:nth-child(1)")
            )
        )
        link.click()
        drop_area = self._wait.until(
            EC.visibility_of_element_located((By.CSS_SELECTOR, "html"))
        )
        drop_area.click()
        drop_area.send_keys(Keys.CONTROL + "v")

        matching_text = self._wait.until(
            EC.visibility_of_element_located((By.ID, "matching"))
        )
        assert matching_text.text == "MATCHING", "copy/paste html should match"

        return True

    def accept_download(self):
        # check the Firefox UI
        self._driver.set_context("chrome")
        download_button = self._wait.until(
            EC.visibility_of_element_located((By.ID, "downloads-button"))
        )
        download_button.click()
        time.sleep(1)

        blocked_item = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadTarget")
            )
        )
        blocked_item.click()
        download_name = blocked_item.get_property("value")

        download_allow = self._wait.until(
            EC.presence_of_element_located(
                (By.ID, "downloadsPanel-blockedSubview-unblockButton")
            )
        )
        download_allow.click()

        # back to page
        self._driver.set_context("content")

        return download_name

    def wait_for_download(self):
        # check the Firefox UI
        self._driver.set_context("chrome")
        download_button = self._wait.until(
            EC.visibility_of_element_located((By.ID, "downloads-button"))
        )
        download_button.click()
        time.sleep(1)

        download_item = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadTarget")
            )
        )
        download_name = download_item.get_property("value")

        download_progress = self._wait.until(
            EC.presence_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadProgress")
            )
        )
        self._wait.until(lambda d: download_progress.get_property("value") == 100)

        # back to page
        self._driver.set_context("content")
        return download_name

    def change_download_folder(self, previous=None, new=None):
        self._logger.info("Download change folder: {} => {}".format(previous, new))
        self._driver.set_context("chrome")
        self._driver.execute_script(
            "Services.prefs.setIntPref('browser.download.folderList', 2);"
        )
        self._driver.execute_script(
            "Services.prefs.setCharPref('browser.download.dir', arguments[0]);", new
        )
        download_dir_pref = self._driver.execute_script(
            "return Services.prefs.getCharPref('browser.download.dir', null);"
        )
        self._driver.set_context("content")
        self._logger.info("Download folder pref: {}".format(download_dir_pref))
        assert (
            download_dir_pref == new
        ), "download directory from pref should match new directory"

    def open_lafibre(self):
        download_site = self.open_tab("https://ip.lafibre.info/test-debit.php")
        return download_site

    def test_download_folder_change(self, exp):
        """
        C1756713
        """

        download_site = self.open_lafibre()
        extra_small = self._wait.until(
            EC.presence_of_element_located(
                (
                    By.CSS_SELECTOR,
                    ".tableau > tbody:nth-child(1) > tr:nth-child(6) > td:nth-child(2) > a:nth-child(1)",
                )
            )
        )
        self._driver.execute_script("arguments[0].click();", extra_small)

        download_name = self.accept_download()
        self.wait_for_download()

        self.open_tab("about:preferences")
        download_folder = self._wait.until(
            EC.presence_of_element_located((By.ID, "downloadFolder"))
        )
        previous_folder = (
            download_folder.get_property("value")
            .replace("\u2066", "")
            .replace("\u2069", "")
        )
        self._logger.info(
            "Download folder from about:preferences: {}".format(previous_folder)
        )
        if not os.path.isabs(previous_folder):
            previous_folder = os.path.join(os.environ.get("HOME", ""), previous_folder)
        with tempfile.TemporaryDirectory() as tmpdir:
            assert os.path.isdir(tmpdir), "tmpdir download should exists"

            download_1 = os.path.abspath(os.path.join(previous_folder, download_name))
            self._logger.info("Download 1 assert: {}".format(download_1))
            assert os.path.isfile(download_1), "downloaded file #1 should exists"

            self.change_download_folder(previous_folder, tmpdir)

            self._driver.switch_to.window(download_site)
            self._driver.execute_script("arguments[0].click();", extra_small)
            self.accept_download()
            download_name2 = self.wait_for_download()
            download_2 = os.path.join(tmpdir, download_name2)

            self._logger.info("Download 2 assert: {}".format(download_2))
            assert os.path.isfile(download_2), "downloaded file #2 should exists"

        return True

    def test_download_folder_removal(self, exp):
        """
        C1756715
        """

        download_site = self.open_lafibre()
        extra_small = self._wait.until(
            EC.presence_of_element_located(
                (
                    By.CSS_SELECTOR,
                    ".tableau > tbody:nth-child(1) > tr:nth-child(6) > td:nth-child(2) > a:nth-child(1)",
                )
            )
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            self.change_download_folder(None, tmpdir)

            self._driver.switch_to.window(download_site)
            self._driver.execute_script("arguments[0].click();", extra_small)

            self.accept_download()
            download_name = self.wait_for_download()
            download_file = os.path.join(tmpdir, download_name)
            self._logger.info("Download assert: {}".format(download_file))
            assert os.path.isdir(tmpdir), "tmpdir download should exists"
            assert os.path.isfile(download_file), "downloaded file should exists"

            self._driver.set_context("chrome")
            download_button = self._wait.until(
                EC.visibility_of_element_located((By.ID, "downloads-button"))
            )
            download_button.click()
            time.sleep(1)

            download_details = self._wait.until(
                EC.visibility_of_element_located(
                    (By.CSS_SELECTOR, ".download-state .downloadDetailsNormal")
                )
            )
            assert download_details.get_property("value").startswith(
                "Completed"
            ), "download should be marked as completed"

        # TemporaryDirectory out of focus so folder removed

        # Close panel we will re-open it
        self._driver.execute_script("this.window.DownloadsButton.hide();")
        self._wait.until(
            EC.invisibility_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadDetailsNormal")
            )
        )

        assert os.path.isdir(tmpdir) is False, "tmpdir should have been removed"
        assert (
            os.path.isfile(download_file) is False
        ), "downloaded file should have been removed"

        download_button.click()
        time.sleep(1)

        download_item = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadTarget")
            )
        )
        download_name_2 = download_item.get_property("value")
        assert download_name == download_name_2, "downloaded names should match"

        download_details = self._wait.until(
            EC.visibility_of_element_located(
                (By.CSS_SELECTOR, ".download-state .downloadDetailsNormal")
            )
        )
        assert download_details.get_property("value").startswith(
            "File moved or missing"
        ), "download panel should report file moved/missing"

        self._driver.execute_script("this.window.DownloadsButton.hide();")

        self._driver.set_context("content")

        return True


if __name__ == "__main__":
    QATests()
