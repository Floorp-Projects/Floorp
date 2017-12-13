const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";

async function check_video_decoding_state(args) {
  let video = content.document.getElementById("autoplay");
  if (!video) {
    ok(false, "Can't get the video element!");
  }

  let isSuspended = args.suspend;
  let reload = args.reload;

  if (reload) {
    // It is too late to register event handlers when playback is half
    // way done. Let's start playback from the beginning so we won't
    // miss any events.
    video.load();
    video.play();
  }

  let state = isSuspended ? "suspended" : "resumed";
  let event = isSuspended ? "mozentervideosuspend" : "mozexitvideosuspend";
  return new Promise(resolve => {
    video.addEventListener(event, function() {
      ok(true, `Video decoding is ${state}.`);
      resolve();
    }, {once: true});
  });
}

function check_should_send_unselected_tab_hover_msg(browser) {
  if (browser.shouldHandleUnselectedTabHover) {
    ok(true, "Should send unselected tab hover msg, someone is listening for it.");
    return true;
  }

  info("did not update the value now, wait until it changes.");
  return new Promise(resolve => {
    browser.messageManager.addMessageListener("UnselectedTabHoverMsg:Enabled", function() {
      ok(true, "Should send unselected tab hover msg, someone is listening for it.");
      resolve();
    });
  });
}

function check_should_not_send_unselected_tab_hover_msg(browser) {
  if (!browser.shouldHandleUnselectedTabHover) {
    ok(true, "Should not send unselected tab hover msg, no one is listening for it.");
    return true;
  }

  info("did not update the value now, wait until it changes.");
  return new Promise(resolve => {
    browser.messageManager.addMessageListener("UnselectedTabHoverMsg:Disabled", function() {
      ok(true, "Should not send unselected tab hover msg, no one is listening for it.");
      resolve();
    });
  });
}

function get_video_decoding_suspend_promise(browser, reload) {
  let suspend = true;
  return ContentTask.spawn(browser, { suspend, reload },
                           check_video_decoding_state);
}

function get_video_decoding_resume_promise(browser) {
  let suspend = false;
  let reload = false;
  return ContentTask.spawn(browser, { suspend, reload },
                           check_video_decoding_state);
}

/**
 * Because of bug1029451, we can't receive "mouseover" event correctly when
 * we disable non-test mouse event. Therefore, we can't synthesize mouse event
 * to simulate cursor hovering, so we temporarily use a hacky way to resume and
 * suspend video decoding.
 */
function cursor_hover_over_tab_and_resume_video_decoding(browser) {
  // TODO : simulate cursor hovering over the tab after fixing bug1029451.
  browser.unselectedTabHover(true /* hover */);
}

function cursor_leave_tab_and_suspend_video_decoding(browser) {
  // TODO : simulate cursor leaveing the tab after fixing bug1029451.
  browser.unselectedTabHover(false /* leave */);
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["media.block-autoplay-until-in-foreground", false],
    ["media.suspend-bkgnd-video.enabled", true],
    ["media.suspend-bkgnd-video.delay-ms", 0],
    ["media.resume-bkgnd-video-on-tabhover", true]
  ]});
});

/**
 * TODO : add the following user-level tests after fixing bug1029451.
 * test1 - only affect the unselected tab
 * test2 - only affect the tab with suspended video
 */
add_task(async function resume_and_suspend_background_video_decoding() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;

  info("- before loading media, we shoudn't send the tab hover msg for tab -");
  await check_should_not_send_unselected_tab_hover_msg(browser);
  browser.loadURI(PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  info("- should suspend background video decoding -");
  await get_video_decoding_suspend_promise(browser, true);
  await check_should_send_unselected_tab_hover_msg(browser);

  info("- when cursor is hovering over the tab, resuming the video decoding -");
  let promise = get_video_decoding_resume_promise(browser);
  await cursor_hover_over_tab_and_resume_video_decoding(browser);
  await promise;
  await check_should_send_unselected_tab_hover_msg(browser);

  info("- when cursor leaves the tab, suspending the video decoding -");
  promise = get_video_decoding_suspend_promise(browser);
  await cursor_leave_tab_and_suspend_video_decoding(browser);
  await promise;
  await check_should_send_unselected_tab_hover_msg(browser);

  info("- select video's owner tab as foreground tab, should resume video -");
  promise = get_video_decoding_resume_promise(browser);
  await BrowserTestUtils.switchTab(window.gBrowser, tab);
  await promise;
  await check_should_send_unselected_tab_hover_msg(browser);

  info("- video's owner tab goes to background again, should suspend video -");
  promise = get_video_decoding_suspend_promise(browser);
  let blankTab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                             "about:blank");
  await promise;
  await check_should_send_unselected_tab_hover_msg(browser);

  info("- remove tabs -");
  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(blankTab);
});
