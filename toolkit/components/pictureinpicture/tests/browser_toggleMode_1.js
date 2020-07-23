/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * See the documentation for the DEFAULT_TOGGLE_STYLES object in head.js
 * for a description of what this toggle style object is representing.
 */
const TOGGLE_STYLES = {
  rootID: "pictureInPictureToggleExperiment",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 0.8,
        ".pip-expanded": 0.0,
      },
      hidden: [
        "#pictureInPictureToggleButton",
        ".pip-explainer",
        ".pip-icon-label > .pip-icon",
      ],
    },
    hoverToggle: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 1.0,
        ".pip-expanded": 1.0,
      },
      hidden: [
        "#pictureInPictureToggleButton",
        ".pip-explainer",
        ".pip-icon-label > .pip-icon",
      ],
    },
  },
};

/**
 * Tests the Mode 1 variation of the Picture-in-Picture toggle in both the
 * left and right positions.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.mode", 1],
      ["media.videocontrols.picture-in-picture.video-toggle.position", "left"],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES },
    "no-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES },
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.position", "right"],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES },
    "no-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES },
  });
});
