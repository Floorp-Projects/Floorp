"use strict";

// This test checks whether the sidebar color properties work.

/**
 * Test whether the selected browser has the sidebar theme applied
 * @param {Object} theme that is applied
 * @param {boolean} isBrightText whether the brighttext attribute should be set
 */
async function test_sidebar_theme(theme, isBrightText) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme,
    },
  });

  const sidebarBox = document.getElementById("sidebar-box");
  const content = SidebarUI.browser.contentWindow;
  const root = content.document.documentElement;

  ok(
    !sidebarBox.hasAttribute("lwt-sidebar"),
    "Sidebar box should not have lwt-sidebar attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar"),
    "Sidebar should not have lwt-sidebar attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar-brighttext"),
    "Sidebar should not have lwt-sidebar-brighttext attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar-highlight"),
    "Sidebar should not have lwt-sidebar-highlight attribute"
  );

  const rootCS = content.getComputedStyle(root);
  const originalBackground = rootCS.backgroundColor;
  const originalColor = rootCS.color;

  // ::-moz-tree-row(selected, focus) computed style can't be accessed, so we create a fake one.
  const highlightCS = {
    get backgroundColor() {
      // Standardize to rgb like other computed style.
      let color = rootCS.getPropertyValue(
        "--lwt-sidebar-highlight-background-color"
      );
      let [r, g, b] = color
        .replace("rgba(", "")
        .split(",")
        .map(channel => parseInt(channel, 10));
      return `rgb(${r}, ${g}, ${b})`;
    },

    get color() {
      let color = rootCS.getPropertyValue("--lwt-sidebar-highlight-text-color");
      let [r, g, b] = color
        .replace("rgba(", "")
        .split(",")
        .map(channel => parseInt(channel, 10));
      return `rgb(${r}, ${g}, ${b})`;
    },
  };
  const originalHighlightBackground = highlightCS.backgroundColor;
  const originalHighlightColor = highlightCS.color;

  await extension.startup();

  Services.ppmm.sharedData.flush();

  const actualBackground = hexToCSS(theme.colors.sidebar) || originalBackground;
  const actualColor = hexToCSS(theme.colors.sidebar_text) || originalColor;
  const actualHighlightBackground =
    hexToCSS(theme.colors.sidebar_highlight) || originalHighlightBackground;
  const actualHighlightColor =
    hexToCSS(theme.colors.sidebar_highlight_text) || originalHighlightColor;
  const isCustomHighlight = !!theme.colors.sidebar_highlight_text;
  const isCustomSidebar = !!theme.colors.sidebar_text;

  is(
    sidebarBox.hasAttribute("lwt-sidebar"),
    isCustomSidebar,
    `Sidebar box should${
      !isCustomSidebar ? " not" : ""
    } have lwt-sidebar attribute`
  );
  is(
    root.hasAttribute("lwt-sidebar"),
    isCustomSidebar,
    `Sidebar should${!isCustomSidebar ? " not" : ""} have lwt-sidebar attribute`
  );
  is(
    root.hasAttribute("lwt-sidebar-brighttext"),
    isBrightText,
    `Sidebar should${
      !isBrightText ? " not" : ""
    } have lwt-sidebar-brighttext attribute`
  );
  is(
    root.hasAttribute("lwt-sidebar-highlight"),
    isCustomHighlight,
    `Sidebar should${
      !isCustomHighlight ? " not" : ""
    } have lwt-sidebar-highlight attribute`
  );

  if (isCustomSidebar) {
    const sidebarBoxCS = window.getComputedStyle(sidebarBox);
    is(
      sidebarBoxCS.backgroundColor,
      actualBackground,
      "Sidebar box background should be set."
    );
    is(
      sidebarBoxCS.color,
      actualColor,
      "Sidebar box text color should be set."
    );
  }

  is(
    rootCS.backgroundColor,
    actualBackground,
    "Sidebar background should be set."
  );
  is(rootCS.color, actualColor, "Sidebar text color should be set.");

  is(
    highlightCS.backgroundColor,
    actualHighlightBackground,
    "Sidebar highlight background color should be set."
  );
  is(
    highlightCS.color,
    actualHighlightColor,
    "Sidebar highlight text color should be set."
  );

  await extension.unload();

  Services.ppmm.sharedData.flush();

  ok(
    !sidebarBox.hasAttribute("lwt-sidebar"),
    "Sidebar box should not have lwt-sidebar attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar"),
    "Sidebar should not have lwt-sidebar attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar-brighttext"),
    "Sidebar should not have lwt-sidebar-brighttext attribute"
  );
  ok(
    !root.hasAttribute("lwt-sidebar-highlight"),
    "Sidebar should not have lwt-sidebar-highlight attribute"
  );

  is(
    rootCS.backgroundColor,
    originalBackground,
    "Sidebar background should be reset."
  );
  is(rootCS.color, originalColor, "Sidebar text color should be reset.");
  is(
    highlightCS.backgroundColor,
    originalHighlightBackground,
    "Sidebar highlight background color should be reset."
  );
  is(
    highlightCS.color,
    originalHighlightColor,
    "Sidebar highlight text color should be reset."
  );
}

add_task(async function test_support_sidebar_colors() {
  for (let command of ["viewBookmarksSidebar", "viewHistorySidebar"]) {
    info("Executing command: " + command);

    await SidebarUI.show(command);

    await test_sidebar_theme(
      {
        colors: {
          sidebar: "#fafad2", // lightgoldenrodyellow
          sidebar_text: "#2f4f4f", // darkslategrey
        },
      },
      false
    );

    await test_sidebar_theme(
      {
        colors: {
          sidebar: "#8b4513", // saddlebrown
          sidebar_text: "#ffa07a", // lightsalmon
        },
      },
      true
    );

    await test_sidebar_theme(
      {
        colors: {
          sidebar: "#fffafa", // snow
          sidebar_text: "#663399", // rebeccapurple
          sidebar_highlight: "#7cfc00", // lawngreen
          sidebar_highlight_text: "#ffefd5", // papayawhip
        },
      },
      false
    );

    await test_sidebar_theme(
      {
        colors: {
          sidebar_highlight: "#a0522d", // sienna
          sidebar_highlight_text: "#fff5ee", // seashell
        },
      },
      false
    );
  }
});

add_task(async function test_support_sidebar_border_color() {
  const LIGHT_SALMON = "#ffa07a";
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          sidebar_border: LIGHT_SALMON,
        },
      },
    },
  });

  await extension.startup();

  const sidebarHeader = document.getElementById("sidebar-header");
  const sidebarHeaderCS = window.getComputedStyle(sidebarHeader);

  is(
    sidebarHeaderCS.borderBottomColor,
    hexToCSS(LIGHT_SALMON),
    "Sidebar header border should be colored properly"
  );

  if (AppConstants.platform !== "linux") {
    const sidebarSplitter = document.getElementById("sidebar-splitter");
    const sidebarSplitterCS = window.getComputedStyle(sidebarSplitter);

    is(
      sidebarSplitterCS.borderInlineEndColor,
      hexToCSS(LIGHT_SALMON),
      "Sidebar splitter should be colored properly"
    );

    SidebarUI.reversePosition();

    is(
      sidebarSplitterCS.borderInlineStartColor,
      hexToCSS(LIGHT_SALMON),
      "Sidebar splitter should be colored properly after switching sides"
    );

    SidebarUI.reversePosition();
  }

  await extension.unload();
});
