/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * See the documentation for the DEFAULT_TOGGLE_STYLES object in head.js
 * for a description of what these toggle style objects are representing.
 */
const TOGGLE_STYLES_LEFT_EXPLAINER = {
  rootID: "pictureInPictureToggleExperiment",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 0.8,
        ".pip-expanded": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-icon-label > .pip-icon"],
    },

    hoverToggle: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 1.0,
        ".pip-expanded": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-icon-label > .pip-icon"],
    },
  },
};

const TOGGLE_STYLES_RIGHT_EXPLAINER = {
  rootID: "pictureInPictureToggleExperiment",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 0.8,
        ".pip-expanded": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-wrapper > .pip-icon"],
    },

    hoverToggle: {
      opacities: {
        ".pip-small": 0.0,
        ".pip-wrapper": 1.0,
        ".pip-expanded": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-wrapper > .pip-icon"],
    },
  },
};

const TOGGLE_STYLES_LEFT_SMALL = {
  rootID: "pictureInPictureToggleExperiment",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-wrapper": 0.8,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-expanded"],
    },

    hoverToggle: {
      opacities: {
        ".pip-wrapper": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-expanded"],
    },
  },
};

const TOGGLE_STYLES_RIGHT_SMALL = {
  rootID: "pictureInPictureToggleExperiment",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-wrapper": 0.8,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-expanded"],
    },

    hoverToggle: {
      opacities: {
        ".pip-wrapper": 1.0,
      },
      hidden: ["#pictureInPictureToggleButton", ".pip-expanded"],
    },
  },
};

/**
 * Tests the Mode 2 variation of the Picture-in-Picture toggle in both the
 * left and right positions, when the user is in the state where they've never
 * clicked on the Picture-in-Picture toggle before (since we show a more detailed
 * toggle in that state).
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.mode", 2],
      ["media.videocontrols.picture-in-picture.video-toggle.position", "left"],
      [HAS_USED_PREF, false],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_LEFT_EXPLAINER,
    },
  });

  Assert.ok(
    Services.prefs.getBoolPref(HAS_USED_PREF, false),
    "Entered has-used mode."
  );
  Services.prefs.clearUserPref(HAS_USED_PREF);

  await testToggle(TEST_PAGE, {
    "no-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_LEFT_EXPLAINER,
    },
  });

  Assert.ok(
    Services.prefs.getBoolPref(HAS_USED_PREF, false),
    "Entered has-used mode."
  );
  Services.prefs.clearUserPref(HAS_USED_PREF);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.position", "right"],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_RIGHT_EXPLAINER,
    },
  });

  Assert.ok(
    Services.prefs.getBoolPref(HAS_USED_PREF, false),
    "Entered has-used mode."
  );
  Services.prefs.clearUserPref(HAS_USED_PREF);

  await testToggle(TEST_PAGE, {
    "no-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_RIGHT_EXPLAINER,
    },
  });

  Assert.ok(
    Services.prefs.getBoolPref(HAS_USED_PREF, false),
    "Entered has-used mode."
  );
  Services.prefs.clearUserPref(HAS_USED_PREF);
});

/**
 * Tests the Mode 2 variation of the Picture-in-Picture toggle in both the
 * left and right positions, when the user is in the state where they've
 * used the Picture-in-Picture feature before.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.mode", 2],
      ["media.videocontrols.picture-in-picture.video-toggle.position", "left"],
      [HAS_USED_PREF, true],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_LEFT_SMALL,
    },
    "no-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES_LEFT_SMALL },
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.position", "right"],
    ],
  });

  await testToggle(TEST_PAGE, {
    "with-controls": {
      canToggle: true,
      toggleStyles: TOGGLE_STYLES_LEFT_SMALL,
    },
    "no-controls": { canToggle: true, toggleStyles: TOGGLE_STYLES_LEFT_SMALL },
  });
});
