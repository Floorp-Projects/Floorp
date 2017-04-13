/**
 * This file tests the size ref on the icons protocols.
 */

const PAGE_URL = "http://icon.mozilla.org/";
const ICON16_URL = "http://places.test/favicon-normal16.png";
const ICON32_URL = "http://places.test/favicon-normal32.png";

add_task(function* () {
  yield PlacesTestUtils.addVisits(PAGE_URL);
  // Add 2 differently sized favicons for this page.

  let data = readFileData(do_get_file("favicon-normal16.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ICON16_URL),
                                          data, data.length, "image/png");
  yield setFaviconForPage(PAGE_URL, ICON16_URL);
  data = readFileData(do_get_file("favicon-normal32.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ICON32_URL),
                                          data, data.length, "image/png");
  yield setFaviconForPage(PAGE_URL, ICON32_URL);

  const PAGE_ICON_URL = "page-icon:" + PAGE_URL;

  yield compareFavicons(PAGE_ICON_URL,
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Not specifying a ref should return the bigger icon");
  // Fake window object.
  let win = { devicePixelRatio: 1.0 };
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 16),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON16_URL)),
                        "Size=16 should return the 16px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 32),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=32 should return the 32px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 33),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=33 should return the 32px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 17),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=17 should return the 32px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 1),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON16_URL)),
                        "Size=1 should return the 16px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 0),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=0 should return the bigger icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, -1),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Invalid size should return the bigger icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL + "#other§=12", 32),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Pre-existing refs should be ignored");
  win = { devicePixelRatio: 1.1 };
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 16),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=16 with HIDPI should return the 32px icon");
  yield compareFavicons(PlacesUtils.urlWithSizeRef(win, PAGE_ICON_URL, 32),
                        PlacesUtils.favicons.getFaviconLinkForIcon(NetUtil.newURI(ICON32_URL)),
                        "Size=32 with HIDPI should return the 32px icon");
});
