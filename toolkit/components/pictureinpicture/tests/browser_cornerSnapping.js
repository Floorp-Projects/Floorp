/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const FLOAT_OFFSET = 50;
const CHANGE_OFFSET = 30;
const DECREASE_OFFSET = FLOAT_OFFSET - CHANGE_OFFSET;
const INCREASE_OFFSET = FLOAT_OFFSET + CHANGE_OFFSET;
/**
 * This function tests the PiP corner snapping feature.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let pipWin = await triggerPictureInPicture(browser, "no-controls");
      let controls = pipWin.document.getElementById("controls");

      /**
       * pipWin floating in top left corner(quadrant 2), dragged left
       * should snap into top left corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft + DECREASE_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft,
        "Window should be on the left"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop,
        "Window should be on the top"
      );

      /**
       * pipWin floating in top left corner(quadrant 2), dragged up
       * should snap into top left corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + DECREASE_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft,
        "Window should be on the left"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop,
        "Window should be on the top"
      );

      /**
       * pipWin floating in top left corner(quadrant 2), dragged right
       * should snap into top right corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft + INCREASE_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft + pipWin.screen.availWidth - pipWin.innerWidth,
        "Window should be on the right"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop,
        "Window should be on the top"
      );

      /**
       * pipWin floating in top left corner(quadrant 2), dragged down
       * should snap into bottom left corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop + INCREASE_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft,
        "Window should be on the left"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop + pipWin.screen.availHeight - pipWin.innerHeight,
        "Window should be on the bottom"
      );

      /**
       * pipWin floating in top right corner(quadrant 1), dragged down
       * should snap into bottom right corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft +
          pipWin.screen.availWidth -
          pipWin.innerWidth -
          FLOAT_OFFSET,
        pipWin.screen.availTop + FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft +
          pipWin.screen.availWidth -
          pipWin.innerWidth -
          FLOAT_OFFSET,
        pipWin.screen.availTop + INCREASE_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft + pipWin.screen.availWidth - pipWin.innerWidth,
        "Window should be on the right"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop + pipWin.screen.availHeight - pipWin.innerHeight,
        "Window should be on the bottom"
      );

      /**
       * pipWin floating in top left corner(quadrant 4), dragged left
       * should snap into bottom left corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft +
          pipWin.screen.availWidth -
          pipWin.innerWidth -
          FLOAT_OFFSET,
        pipWin.screen.availTop +
          pipWin.screen.availHeight -
          pipWin.innerHeight -
          FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft +
          pipWin.screen.availWidth -
          pipWin.innerWidth -
          INCREASE_OFFSET,
        pipWin.screen.availTop +
          pipWin.screen.availHeight -
          pipWin.innerHeight -
          FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft,
        "Window should be on the left"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop + pipWin.screen.availHeight - pipWin.innerHeight,
        "Window should be on the bottom"
      );

      /**
       * pipWin floating in top left corner(quadrant 3), dragged up
       * should snap into top left corner
       */
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop +
          pipWin.screen.availHeight -
          pipWin.innerHeight -
          FLOAT_OFFSET
      );
      EventUtils.sendMouseEvent({ type: "mouseup" }, controls, pipWin);
      pipWin.moveTo(
        pipWin.screen.availLeft + FLOAT_OFFSET,
        pipWin.screen.availTop +
          pipWin.screen.availHeight -
          pipWin.innerHeight -
          INCREASE_OFFSET
      );
      EventUtils.sendMouseEvent(
        {
          type: "mouseup",
          metaKey: true,
        },
        controls,
        pipWin
      );
      Assert.equal(
        pipWin.screenX,
        pipWin.screen.availLeft,
        "Window should be on the left"
      );
      Assert.equal(
        pipWin.screenY,
        pipWin.screen.availTop,
        "Window should be on the top"
      );
    }
  );
});
