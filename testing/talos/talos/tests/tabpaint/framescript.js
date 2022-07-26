/* eslint-env mozilla/frame-script */

(function() {
  addEventListener(
    "load",
    loadevt => {
      if (!content.location.pathname.endsWith("target.html")) {
        return;
      }

      /**
       * When a page is loaded, we expect a search string to be
       * appended with the "starting time" (in ms) of when the tab
       * was opened.
       *
       * Example: target.html?1457063506846
       */
      let opened = parseInt(content.location.search.substring(1), 10);

      addEventListener("MozAfterPaint", function onPaint(e) {
        // Bug 1371332 - sometimes, MozAfterPaint events fire
        // for "empty" paints, where nothing has actually been
        // painted. We can detect that by looking at the rect
        // for the region that has painted.
        let rect = e.boundingClientRect;
        if (!rect.width && !rect.height) {
          return;
        }

        removeEventListener("MozAfterPaint", onPaint);

        // The MozAfterPaint event comes with a paintTimeStamp
        // which tells us when in this content's lifetime the
        // paint actually occurred. Note that this is not a
        // measurement of when this paint occurred from
        // the UNIX epoch. This makes it a little tricky to
        // calculate when the paint actually occurred relative
        // to the starting time that's been appended to the
        // page's URL.
        //
        // Thankfully, the PerformanceTiming API gives us a
        // sense of when this page's lifetime started, relative
        // to the UNIX epoch - the "fetchStart". Taking that
        // time and adding the paintTimeStamp should give us
        // a pretty decent approximation of when since the
        // UNIX epoch the paint actually occurred for this
        // content.
        //
        // We can then subtract the starting time to get the
        // delta, which should now represent the time it took
        // from requesting that the tab be opened, to the
        // paint occurring within the tab.
        let fetchStart = content.performance.timing.fetchStart;
        let presented = fetchStart + e.paintTimeStamp;
        let delta = presented - opened;

        sendAsyncMessage("TabPaint:Painted", { delta });
      });
    },
    true
  );

  addEventListener(
    "TabPaint:Ping",
    e => {
      let evt = new content.CustomEvent("TabPaint:Pong", { bubbles: true });
      content.dispatchEvent(evt);
    },
    false,
    true
  );

  addEventListener(
    "TabPaint:Go",
    e => {
      sendAsyncMessage("TabPaint:Go", { target: e.detail.target });
    },
    false,
    true
  );

  addMessageListener("TabPaint:OpenFromContent", msg => {
    let evt = new content.CustomEvent("TabPaint:OpenFromContent", {
      bubbles: true,
    });
    content.dispatchEvent(evt);
  });

  addMessageListener("TabPaint:FinalResults", msg => {
    let evt = Cu.cloneInto(
      {
        bubbles: true,
        detail: msg.data,
      },
      content
    );

    content.dispatchEvent(
      new content.CustomEvent("TabPaint:FinalResults", evt)
    );
  });
})();
