/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const AVAILABLE_INJECTIONS = [
  {
    id: "testbed-injection",
    platform: "all",
    domain: "webcompat-addon-testbed.herokuapp.com",
    bug: "0000000",
    hidden: true,
    contentScripts: {
      matches: ["*://webcompat-addon-testbed.herokuapp.com/*"],
      css: [
        {
          file: "injections/css/bug0000000-testbed-css-injection.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug0000000-testbed-js-injection.js",
        },
      ],
    },
  },
  {
    id: "bug1452707",
    platform: "all",
    domain: "ib.absa.co.za",
    bug: "1452707",
    contentScripts: {
      matches: ["https://ib.absa.co.za/*"],
      js: [
        {
          file:
            "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js",
        },
      ],
    },
  },
  {
    id: "bug1457335",
    platform: "desktop",
    domain: "histography.io",
    bug: "1457335",
    contentScripts: {
      matches: ["*://histography.io/*"],
      js: [
        {
          file: "injections/js/bug1457335-histography.io-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1472075",
    platform: "desktop",
    domain: "bankofamerica.com",
    bug: "1472075",
    contentScripts: {
      matches: [
        "*://*.bankofamerica.com/*",
        "*://*.ml.com/*", // #120104
      ],
      js: [
        {
          file: "injections/js/bug1472075-bankofamerica.com-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1579159",
    platform: "android",
    domain: "m.tailieu.vn",
    bug: "1579159",
    contentScripts: {
      matches: ["*://m.tailieu.vn/*", "*://m.elib.vn/*"],
      js: [
        {
          file: "injections/js/bug1579159-m.tailieu.vn-pdfjs-worker-disable.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1583366",
    platform: "desktop",
    domain: "Download prompt for files with no content-type",
    bug: "1583366",
    data: {
      urls: ["https://ads-us.rd.linksynergy.com/as.php*"],
      contentType: {
        name: "content-type",
        value: "text/html; charset=utf-8",
      },
    },
    customFunc: "noSniffFix",
  },
  {
    id: "bug1570328",
    platform: "android",
    domain: "developer.apple.com",
    bug: "1570328",
    contentScripts: {
      matches: ["*://developer.apple.com/*"],
      css: [
        {
          file:
            "injections/css/bug1570328-developer-apple.com-transform-scale.css",
        },
      ],
    },
  },
  {
    id: "bug1575000",
    platform: "all",
    domain: "apply.lloydsbank.co.uk",
    bug: "1575000",
    contentScripts: {
      matches: ["*://apply.lloydsbank.co.uk/*"],
      css: [
        {
          file:
            "injections/css/bug1575000-apply.lloydsbank.co.uk-radio-buttons-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1605611",
    platform: "android",
    domain: "maps.google.com",
    bug: "1605611",
    contentScripts: {
      matches: InterventionHelpers.matchPatternsForGoogle(
        "*://www.google.",
        "/maps*"
      ),
      css: [
        {
          file: "injections/css/bug1605611-maps.google.com-directions-time.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug1605611-maps.google.com-directions-time.js",
        },
      ],
    },
  },
  {
    id: "bug1610344",
    platform: "all",
    domain: "directv.com.co",
    bug: "1610344",
    contentScripts: {
      matches: [
        "https://*.directv.com.co/*",
        "https://*.directv.com.ec/*", // bug 1827706
      ],
      css: [
        {
          file:
            "injections/css/bug1610344-directv.com.co-hide-unsupported-message.css",
        },
      ],
    },
  },
  {
    id: "bug1644830",
    platform: "desktop",
    domain: "usps.com",
    bug: "1644830",
    contentScripts: {
      matches: ["https://*.usps.com/*"],
      css: [
        {
          file:
            "injections/css/bug1644830-missingmail.usps.com-checkboxes-not-visible.css",
        },
      ],
    },
  },
  {
    id: "bug1651917",
    platform: "android",
    domain: "teletrader.com",
    bug: "1651917",
    contentScripts: {
      matches: ["*://*.teletrader.com/*"],
      css: [
        {
          file:
            "injections/css/bug1651917-teletrader.com.body-transform-origin.css",
        },
      ],
    },
  },
  {
    id: "bug1653075",
    platform: "desktop",
    domain: "livescience.com",
    bug: "1653075",
    contentScripts: {
      matches: ["*://*.livescience.com/*"],
      css: [
        {
          file: "injections/css/bug1653075-livescience.com-scrollbar-width.css",
        },
      ],
    },
  },
  {
    id: "bug1654877",
    platform: "android",
    domain: "preev.com",
    bug: "1654877",
    contentScripts: {
      matches: ["*://preev.com/*"],
      css: [
        {
          file: "injections/css/bug1654877-preev.com-moz-appearance-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1654907",
    platform: "android",
    domain: "reactine.ca",
    bug: "1654907",
    contentScripts: {
      matches: ["*://*.reactine.ca/*"],
      css: [
        {
          file: "injections/css/bug1654907-reactine.ca-hide-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1631811",
    platform: "all",
    domain: "datastudio.google.com",
    bug: "1631811",
    contentScripts: {
      matches: ["https://datastudio.google.com/embed/reporting/*"],
      js: [
        {
          file: "injections/js/bug1631811-datastudio.google.com-indexedDB.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1694470",
    platform: "android",
    domain: "m.myvidster.com",
    bug: "1694470",
    contentScripts: {
      matches: ["https://m.myvidster.com/*"],
      css: [
        {
          file: "injections/css/bug1694470-myvidster.com-content-not-shown.css",
        },
      ],
    },
  },
  {
    id: "bug1731825",
    platform: "desktop",
    domain: "Office 365 email handling prompt",
    bug: "1731825",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.sharepoint.com/*",
        "*://*.office365.com/*",
      ],
      js: [
        {
          file:
            "injections/js/bug1731825-office365-email-handling-prompt-autohide.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1707795",
    platform: "desktop",
    domain: "Office Excel spreadsheets",
    bug: "1707795",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.sharepoint.com/*",
      ],
      css: [
        {
          file:
            "injections/css/bug1707795-office365-sheets-overscroll-disable.css",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1712833",
    platform: "all",
    domain: "buskocchi.desuca.co.jp",
    bug: "1712833",
    contentScripts: {
      matches: ["*://buskocchi.desuca.co.jp/*"],
      css: [
        {
          file:
            "injections/css/bug1712833-buskocchi.desuca.co.jp-fix-map-height.css",
        },
      ],
    },
  },
  {
    id: "bug1722955",
    platform: "android",
    domain: "frontgate.com",
    bug: "1722955",
    contentScripts: {
      matches: ["*://*.frontgate.com/*"],
      js: [
        {
          file: "lib/ua_helpers.js",
        },
        {
          file: "injections/js/bug1722955-frontgate.com-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1724764",
    platform: "android",
    domain: "Issues related to missing window.print",
    bug: "1724764",
    contentScripts: {
      matches: [
        "*://*.amextravel.com/*", // 1724764
        "*://*.edupage.org/*", // 1804477 and 1800118
      ],
      js: [
        {
          file: "injections/js/bug1724764-window-print.js",
        },
      ],
    },
  },
  {
    id: "bug1724868",
    platform: "android",
    domain: "news.yahoo.co.jp",
    bug: "1724868",
    contentScripts: {
      matches: ["*://news.yahoo.co.jp/articles/*", "*://s.yimg.jp/*"],
      js: [
        {
          file: "injections/js/bug1724868-news.yahoo.co.jp-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1741234",
    platform: "all",
    domain: "patient.alphalabs.ca",
    bug: "1741234",
    contentScripts: {
      matches: ["*://patient.alphalabs.ca/*"],
      css: [
        {
          file: "injections/css/bug1741234-patient.alphalabs.ca-height-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1748455",
    platform: "android",
    domain: "reddit.com",
    bug: "1748455",
    contentScripts: {
      matches: ["*://*.reddit.com/*"],
      css: [
        {
          file:
            "injections/css/bug1748455-reddit.com-gallery-image-width-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1739489",
    platform: "desktop",
    domain: "Sites using draft.js",
    bug: "1739489",
    contentScripts: {
      matches: [
        "*://draftjs.org/*", // Bug 1739489
        "*://www.facebook.com/*", // Bug 1739489
        "*://twitter.com/*", // Bug 1776229
        "*://mobile.twitter.com/*", // Bug 1776229
      ],
      js: [
        {
          file: "injections/js/bug1739489-draftjs-beforeinput.js",
        },
      ],
    },
  },
  {
    id: "bug1765947",
    platform: "android",
    domain: "veniceincoming.com",
    bug: "1765947",
    contentScripts: {
      matches: ["*://veniceincoming.com/*"],
      css: [
        {
          file: "injections/css/bug1765947-veniceincoming.com-left-fix.css",
        },
      ],
    },
  },
  {
    id: "bug11769762",
    platform: "all",
    domain: "tiktok.com",
    bug: "1769762",
    contentScripts: {
      matches: ["https://www.tiktok.com/*"],
      js: [
        {
          file: "injections/js/bug1769762-tiktok.com-plugins-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1770962",
    platform: "all",
    domain: "coldwellbankerhomes.com",
    bug: "1770962",
    contentScripts: {
      matches: ["*://*.coldwellbankerhomes.com/*"],
      css: [
        {
          file:
            "injections/css/bug1770962-coldwellbankerhomes.com-image-height.css",
        },
      ],
    },
  },
  {
    id: "bug1774490",
    platform: "all",
    domain: "rainews.it",
    bug: "1774490",
    contentScripts: {
      matches: ["*://www.rainews.it/*"],
      css: [
        {
          file: "injections/css/bug1774490-rainews.it-gallery-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1774005",
    platform: "all",
    domain: "Sites relying on window.InstallTrigger",
    bug: "1774005",
    contentScripts: {
      matches: [
        "*://*.crunchyroll.com/*", // Bug 1777597
        "*://*.ersthelfer.tv/*", // Bug 1817520
        "*://*.pixiv.net/*", // Bug 1774006
        "*://*.webex.com/*", // Bug 1788934
        "*://business.help.royalmail.com/app/webforms/*", // Bug 1786404
        "*://ifcinema.institutfrancais.com/*", // Bug 1806423
        "*://islamionline.islamicbank.ps/*", // Bug 1821439
        "*://mobilevikings.be/*/registration/*", // Bug 1797400
        "*://www.northcountrypublicradio.org/contact/subscribe.html*", // Bug 1778382,
        "*://www.schoolnutritionandfitness.com/*", // Bug 1793761
      ],
      js: [
        {
          file: "injections/js/bug1774005-installtrigger-shim.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1784302",
    platform: "android",
    domain: "open.toutiao.com",
    bug: "1784302",
    contentScripts: {
      matches: ["*://open.toutiao.com/*"],
      js: [
        {
          file: "injections/js/bug1784302-effectiveType-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1784141",
    platform: "android",
    domain: "aveeno.com and acuvue.com",
    bug: "1784141",
    contentScripts: {
      matches: [
        "*://*.aveeno.com/*",
        "*://*.aveeno.ca/*",
        "*://*.aveeno.com.au/*",
        "*://*.aveeno.co.kr/*",
        "*://*.aveeno.co.uk/*",
        "*://*.aveeno.ie/*",
        "*://*.acuvue.com/*", // 1804730
        "*://*.acuvue.com.ar/*",
        "*://*.acuvue.com.br/*",
        "*://*.acuvue.ca/*",
        "*://*.acuvue-fr.ca/*",
        "*://*.acuvue.cl/*",
        "*://*.acuvue.co.cr/*",
        "*://*.acuvue.com.co/*",
        "*://*.acuvue.com.do/*",
        "*://*.acuvue.com.pe/*",
        "*://*.acuvue.com.sv/*",
        "*://*.acuvue.com.gt/*",
        "*://*.acuvue.hn/*",
        "*://*.acuvue.com.mx/*",
        "*://*.acuvue.com.pa/*",
        "*://*.acuvue.com.py/*",
        "*://*.acuvue.com.pr/*",
        "*://*.acuvue.com.uy/*",
        "*://*.acuvue.com.au/*",
        "*://*.acuvue.com.cn/*",
        "*://*.acuvue.com.hk/*",
        "*://*.acuvue.co.in/*",
        "*://*.acuvue.co.id/*",
        "*://acuvuevision.jp/*",
        "*://*.acuvue.co.kr/*",
        "*://*.acuvue.com.my/*",
        "*://*.acuvue.co.nz/*",
        "*://*.acuvue.com.sg/*",
        "*://*.acuvue.com.tw/*",
        "*://*.acuvue.co.th/*",
        "*://*.acuvue.com.vn/*",
        "*://*.acuvue.at/*",
        "*://*.acuvue.be/*",
        "*://*.fr.acuvue.be/*",
        "*://*.acuvue-croatia.com/*",
        "*://*.acuvue.cz/*",
        "*://*.acuvue.dk/*",
        "*://*.acuvue.fi/*",
        "*://*.acuvue.fr/*",
        "*://*.acuvue.de/*",
        "*://*.acuvue.gr/*",
        "*://*.acuvue.hu/*",
        "*://*.acuvue.ie/*",
        "*://*.acuvue.co.il/*",
        "*://*.acuvue.it/*",
        "*://*.acuvuekz.com/*",
        "*://*.acuvue.lu/*",
        "*://*.en.acuvuearabia.com/*",
        "*://*.acuvuearabia.com/*",
        "*://*.acuvue.nl/*",
        "*://*.acuvue.no/*",
        "*://*.acuvue.pl/*",
        "*://*.acuvue.pt/*",
        "*://*.acuvue.ro/*",
        "*://*.acuvue.ru/*",
        "*://*.acuvue.sk/*",
        "*://*.acuvue.si/*",
        "*://*.acuvue.co.za/*",
        "*://*.jnjvision.com.tr/*",
        "*://*.acuvue.co.uk/*",
        "*://*.acuvue.ua/*",
        "*://*.acuvue.com.pe/*",
        "*://*.acuvue.es/*",
        "*://*.acuvue.se/*",
        "*://*.acuvue.ch/*",
      ],
      css: [
        {
          file:
            "injections/css/bug1784141-aveeno.com-acuvue.com-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1784199",
    platform: "all",
    domain: "Sites based on Entrata Platform",
    bug: "1784199",
    contentScripts: {
      matches: [
        "*://*.aptsovation.com/*",
        "*://*.avanabayview.com/*", // #118617
        "*://*.breakpointeandcoronado.com/*", // #117735
        "*://*.liveatlasathens.com/*", // #111189
        "*://*.liveobserverpark.com/*", // #105244
        "*://*.midwayurban.com/*", // #116523
        "*://*.nhcalaska.com/*",
        "*://*.prospectportal.com/*", // #115206
        "*://*.securityproperties.com/*",
        "*://*.theloftsorlando.com/*",
        "*://*.vanallenapartments.com/*", // #120056
      ],
      css: [
        {
          file: "injections/css/bug1784199-entrata-platform-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1789164",
    platform: "all",
    domain: "zdnet.com",
    bug: "1789164",
    contentScripts: {
      matches: ["*://www.zdnet.com/*"],
      css: [
        {
          file: "injections/css/bug1789164-zdnet.com-cropped-section.css",
        },
      ],
    },
  },
  {
    id: "bug1795490",
    platform: "android",
    domain: "www.china-airlines.com",
    bug: "1795490",
    contentScripts: {
      matches: ["*://www.china-airlines.com/*"],
      js: [
        {
          file:
            "injections/js/bug1795490-www.china-airlines.com-undisable-date-fields-on-mobile.js",
        },
      ],
    },
  },
  {
    id: "bug1799968",
    platform: "linux",
    domain: "www.samsung.com",
    bug: "1799968",
    contentScripts: {
      matches: ["*://www.samsung.com/*/watches/*/*"],
      js: [
        {
          file:
            "injections/js/bug1799968-www.samsung.com-appVersion-linux-fix.js",
        },
      ],
    },
  },
  {
    id: "bug1799980",
    platform: "all",
    domain: "healow.com",
    bug: "1799980",
    contentScripts: {
      matches: ["*://healow.com/*"],
      js: [
        {
          file: "injections/js/bug1799980-healow.com-infinite-loop-fix.js",
        },
      ],
    },
  },
  {
    id: "bug1799994",
    platform: "all",
    domain: "www.vivobarefoot.com",
    bug: "1799994",
    contentScripts: {
      matches: ["*://www.vivobarefoot.com/*"],
      css: [
        {
          file:
            "injections/css/bug1799994-www.vivobarefoot.com-product-filters-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1800000",
    platform: "all",
    domain: "www.honda.co.uk",
    bug: "1800000",
    contentScripts: {
      matches: ["*://www.honda.co.uk/cars/book-a-service.html*"],
      css: [
        {
          file:
            "injections/css/bug1800000-www.honda.co.uk-choose-dealer-button-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1800143",
    platform: "all",
    domain: "www.nintendo.co.jp",
    bug: "1800143",
    contentScripts: {
      matches: ["*://www.nintendo.co.jp/software/feature/*"],
      css: [
        {
          file:
            "injections/css/bug1800143-www.nintendo.co.jp-zoomed-in-image-scrolling-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1803976",
    platform: "desktop",
    domain: "www.youtube.com",
    bug: "1803976",
    contentScripts: {
      matches: ["*://www.youtube.com/*"],
      js: [
        {
          file:
            "injections/js/bug1803976-www.youtube.com-performance-now-precision.js",
        },
      ],
    },
  },
  {
    id: "bug1811325",
    platform: "desktop",
    domain: "www.bdo.com.ph",
    bug: "1811325",
    customFunc: "acceptLanguageFix",
    data: {
      urls: ["*://www.bdo.com.ph/*", "*://www.inmac-wstore.com/*"],
    },
  },
  {
    id: "bug1448747",
    platform: "android",
    domain: "FastClick breakage",
    bug: "1448747",
    contentScripts: {
      matches: [
        "*://*.co2meter.com/*", // 10959
        "*://*.franmar.com/*", // 27273
        "*://*.themusiclab.org/*", // 49667
        "*://*.oregonfoodbank.org/*", // 53203
        "*://*.fourbarrelcoffee.com/*", // 59427
        "*://bluetokaicoffee.com/*", // 99867
        "*://bathpublishing.com/*", // 100145
        "*://dylantalkstone.com/*", // 101356
        "*://renewd.com.au/*", // 104998
        "*://gofreeconcepts.de/*", // 105534
        "*://*.lamudi.co.id/*", // 106767
        "*://*.thehawksmoor.com/*", // 107549
        "*://weaversofireland.com/*", // 116816
        "*://*.iledefrance-mobilites.fr/*", // 117344
        "*://*.lawnmowerpartsworld.com/*", // 117577
        "*://*.discountcoffee.co.uk/*", // 118757
      ],
      js: [
        {
          file: "injections/js/bug1448747-fastclick-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1818818",
    platform: "android",
    domain: "FastClick breakage - legacy",
    bug: "1818818",
    contentScripts: {
      matches: [
        "*://*.chatiw.com/*", // 5544
        "*://*.marksandspencer.com/*", // 101811
        "*://*.wellcare.com/*", // 116595
      ],
      js: [
        {
          file: "injections/js/bug1818818-fastclick-legacy-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1819476",
    platform: "all",
    domain: "axisbank.com",
    bug: "1819476",
    contentScripts: {
      matches: ["*://*.axisbank.com/*"],
      js: [
        {
          file:
            "injections/js/bug1819476-axisbank.com-webkitSpeechRecognition-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1819450",
    platform: "android",
    domain: "cmbchina.com",
    bug: "1819450",
    contentScripts: {
      matches: ["*://www.cmbchina.com/*", "*://cmbchina.com/*"],
      js: [
        {
          file: "injections/js/bug1819450-cmbchina.com-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1819678",
    platform: "android",
    domain: "cnki.net",
    bug: "1819678",
    contentScripts: {
      matches: ["*://*.cnki.net/*"],
      js: [
        {
          file: "injections/js/bug1819678-cnki.net-undisable-search-field.js",
        },
      ],
    },
  },
  {
    id: "bug1827678-webc77727",
    platform: "android",
    domain: "free4talk.com",
    bug: "1827678",
    contentScripts: {
      matches: ["*://www.free4talk.com/*"],
      js: [
        {
          file: "injections/js/bug1819678-free4talk.com-window-chrome-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1827678-85474",
    platform: "desktop",
    domain: "elal.com",
    bug: "1827678",
    contentScripts: {
      matches: ["*://*.elal.com/*"],
      css: [
        {
          file: "injections/css/bug1819678-elal.com-unsupported-banner.css",
        },
      ],
    },
  },
  {
    id: "bug1827678-webc119017",
    platform: "desktop",
    domain: "nppes.cms.hhs.gov",
    bug: "1827678",
    contentScripts: {
      matches: ["*://nppes.cms.hhs.gov/*"],
      css: [
        {
          file:
            "injections/css/bug1819678-nppes.cms.hhs.gov-unsupported-banner.css",
        },
      ],
    },
  },
];

module.exports = AVAILABLE_INJECTIONS;
