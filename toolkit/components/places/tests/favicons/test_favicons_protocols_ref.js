/**
 * This file tests the size ref on the icons protocols.
 */

const PAGE_URL = "http://icon.mozilla.org/";
const ICON16_URL = "http://places.test/favicon-normal16.png";
const ICON32_URL = "http://places.test/favicon-normal32.png";

add_task(async function () {
  await PlacesTestUtils.addVisits(PAGE_URL);
  // Add 2 differently sized favicons for this page.

  let data = readFileData(do_get_file("favicon-normal16.png"));
  PlacesUtils.favicons.replaceFaviconData(
    Services.io.newURI(ICON16_URL),
    data,
    "image/png"
  );
  await setFaviconForPage(PAGE_URL, ICON16_URL);
  data = readFileData(do_get_file("favicon-normal32.png"));
  PlacesUtils.favicons.replaceFaviconData(
    Services.io.newURI(ICON32_URL),
    data,
    "image/png"
  );
  await setFaviconForPage(PAGE_URL, ICON32_URL);

  const PAGE_ICON_URL = "page-icon:" + PAGE_URL;

  await compareFavicons(
    PAGE_ICON_URL,
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Not specifying a ref should return the bigger icon"
  );
  // Fake window object.
  let win = { devicePixelRatio: 1.0 };
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 16),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON16_URL)),
    "Size=16 should return the 16px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 32),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=32 should return the 32px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 33),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=33 should return the 32px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 17),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=17 should return the 32px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 1),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON16_URL)),
    "Size=1 should return the 16px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 0),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=0 should return the bigger icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, -1),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Invalid size should return the bigger icon"
  );

  // Add the icon also for the page with ref.
  await PlacesTestUtils.addVisits(PAGE_URL + "#other§=12");
  await setFaviconForPage(PAGE_URL + "#other§=12", ICON16_URL, false);
  await setFaviconForPage(PAGE_URL + "#other§=12", ICON32_URL, false);
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL + "#other§=12", 16),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON16_URL)),
    "Pre-existing refs should be retained"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL + "#other§=12", 32),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Pre-existing refs should be retained"
  );

  // If the ref-ed url is unknown, should still try to fetch icon for the unref-ed url.
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL + "#randomstuff", 32),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Non-existing refs should be ignored"
  );

  win = { devicePixelRatio: 1.1 };
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 16),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=16 with HIDPI should return the 32px icon"
  );
  await compareFavicons(
    PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 32),
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON32_URL)),
    "Size=32 with HIDPI should return the 32px icon"
  );

  // Check setting a different default preferred size works.
  PlacesUtils.favicons.setDefaultIconURIPreferredSize(16);
  await compareFavicons(
    PAGE_ICON_URL,
    PlacesUtils.favicons.getFaviconLinkForIcon(Services.io.newURI(ICON16_URL)),
    "Not specifying a ref should return the set default size icon"
  );
});
