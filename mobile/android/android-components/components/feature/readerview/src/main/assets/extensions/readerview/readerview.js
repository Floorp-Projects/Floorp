/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Avoid adding ID selector rules in this style sheet, since they could
 * inadvertently match elements in the article content. */

const supportedProtocols = ["http:", "https:"];

// Prevent false positives for these sites. This list is taken from Fennec:
// https://dxr.mozilla.org/mozilla-central/rev/7d47e7fa2489550ffa83aae67715c5497048923f/toolkit/components/reader/Readerable.js#45
const blockedHosts = [
  "amazon.com",
  "github.com",
  "mail.google.com",
  "pinterest.com",
  "reddit.com",
  "twitter.com",
  "youtube.com"
];

// Class names to preserve in the readerized output. We preserve these class
// names so that rules in readerview.css can match them. This list is taken from Fennec:
// https://dxr.mozilla.org/mozilla-central/rev/7d47e7fa2489550ffa83aae67715c5497048923f/toolkit/components/reader/ReaderMode.jsm#21
const preservedClasses = [
  "caption",
  "emoji",
  "hidden",
  "invisble",
  "sr-only",
  "visually-hidden",
  "visuallyhidden",
  "wp-caption",
  "wp-caption-text",
  "wp-smiley"
];

class ReaderView {

  static isReaderable() {
    if (!supportedProtocols.includes(location.protocol)) {
      return false;
    }

    if (blockedHosts.some(blockedHost => location.hostname.endsWith(blockedHost))) {
      return false;
    }

    if (location.pathname == "/") {
      return false;
    }

    return isProbablyReaderable(document, ReaderView._isNodeVisible);
  }

  static get MIN_FONT_SIZE() {
    return 1;
  }

  static get MAX_FONT_SIZE() {
    return 9;
  }

  static _isNodeVisible(node) {
    return node.clientHeight > 0 && node.clientWidth > 0;
  }

  show({fontSize = 4, fontType = "sans-serif", colorScheme = "light"} = {}) {
    let result = new Readability(document, {classesToPreserve: preservedClasses}).parse();
    result.language = document.documentElement.lang;

    let article = Object.assign(
      result,
      {url: location.hostname},
      {readingTime: this.getReadingTime(result.length, result.language)},
      {byline: this.getByline()},
      {dir: this.getTextDirection(result)}
    );

    document.documentElement.innerHTML = this.createHtml(article);

    this.setFontSize(fontSize);
    this.setFontType(fontType);
    this.setColorScheme(colorScheme);
  }

  hide() {
    location.reload(false)
  }

  /**
   * Allows adjusting the font size in discrete steps between ReaderView.MIN_FONT_SIZE
   * and ReaderView.MAX_FONT_SIZE.
   *
   * @param changeAmount e.g. +1, or -1.
   */
  changeFontSize(changeAmount) {
    var size = Math.max(ReaderView.MIN_FONT_SIZE, Math.min(ReaderView.MAX_FONT_SIZE, this.fontSize + changeAmount));
    this.setFontSize(size);
  }

  /**
   * Sets the font size.
   *
   * @param fontSize must be value between ReaderView.MIN_FONT_SIZE
   * and ReaderView.MAX_FONT_SIZE.
   */
  setFontSize(fontSize) {
    let size = (10 + 2 * fontSize) + "px";
    let readerView = document.getElementById("mozac-readerview-container");
    readerView.style.setProperty("font-size", size);
    this.fontSize = fontSize;
  }

  /**
   * Sets the font type.
   *
   * @param fontType the font type to use.
   */
  setFontType(fontType) {
    let bodyClasses = document.body.classList;

    if (this.fontType) {
      bodyClasses.remove(this.fontType)
    }

    this.fontType = fontType
    bodyClasses.add(this.fontType)
  }

  /**
   * Sets the color scheme
   *
   * @param colorScheme the color scheme to use, must be either light, dark
   * or sepia.
   */
  setColorScheme(colorScheme) {
    if(!['light', 'sepia', 'dark'].includes(colorScheme)) {
      console.error(`Invalid color scheme specified: ${colorScheme}`)
      return;
    }

    let bodyClasses = document.body.classList;

    if (this.colorScheme) {
      bodyClasses.remove(this.colorScheme)
    }

    this.colorScheme = colorScheme
    bodyClasses.add(this.colorScheme)
  }

  createHtml(article) {
    return `
      <head>
        <meta content="text/html; charset=UTF-8" http-equiv="content-type">
        <meta name="viewport" content="width=device-width; user-scalable=0">
        <title>${article.title}</title>
      </head>
      <body class="mozac-readerview-body">
        <div id="mozac-readerview-container" class="container" dir=${article.dir}>
          <div class="header">
            <a class="domain">${article.url}</a>
            <div class="domain-border"></div>
            <h1>${article.title}</h1>
            <div class="credits">${article.byline}</div>
            <div>
              <div>${article.readingTime}</div>
            </div>
          </div>
          <hr>

          <div class="content">
            <div class="mozac-readerview-content">${article.content}</div>
          </div>
        </div>
      </body>
    `
  }

  /**
   * Returns the estimated reading time as localized string.
   *
   * @param length of the article (number of chars).
   * @param optional language of the article, defaults to en.
   */
  getReadingTime(length, lang = "en") {
    const readingSpeed = this.getReadingSpeedForLanguage(lang);
    const charactersPerMinuteLow = readingSpeed.cpm - readingSpeed.variance;
    const charactersPerMinuteHigh = readingSpeed.cpm + readingSpeed.variance;
    const readingTimeMinsSlow = Math.ceil(length / charactersPerMinuteLow);
    const readingTimeMinsFast  = Math.ceil(length / charactersPerMinuteHigh);

    // TODO remove moment.js: https://github.com/mozilla-mobile/android-components/issues/2923
    // We only want to show minutes and not have it converted to hours.
    moment.relativeTimeThreshold('m', Number.MAX_SAFE_INTEGER);

    if (readingTimeMinsSlow == readingTimeMinsFast) {
      return moment.duration(readingTimeMinsFast, 'minutes').locale(lang).humanize();
    }

    return `${readingTimeMinsFast} - ${moment.duration(readingTimeMinsSlow, 'minutes').locale(lang).humanize()}`;
  }

  /**
   * Returns the reading speed of a selection of languages with likely variance.
   *
   * Reading speed estimated from a study done on reading speeds in various languages.
   * study can be found here: http://iovs.arvojournals.org/article.aspx?articleid=2166061
   *
   * @return object with characters per minute and variance. Defaults to English
   * if no suitable language is found in the collection.
   */
  getReadingSpeedForLanguage(lang) {
    const readingSpeed = new Map([
      [ "en", {cpm: 987,  variance: 118 } ],
      [ "ar", {cpm: 612,  variance: 88 } ],
      [ "de", {cpm: 920,  variance: 86 } ],
      [ "es", {cpm: 1025, variance: 127 } ],
      [ "fi", {cpm: 1078, variance: 121 } ],
      [ "fr", {cpm: 998,  variance: 126 } ],
      [ "he", {cpm: 833,  variance: 130 } ],
      [ "it", {cpm: 950,  variance: 140 } ],
      [ "jw", {cpm: 357,  variance: 56 } ],
      [ "nl", {cpm: 978,  variance: 143 } ],
      [ "pl", {cpm: 916,  variance: 126 } ],
      [ "pt", {cpm: 913,  variance: 145 } ],
      [ "ru", {cpm: 986,  variance: 175 } ],
      [ "sk", {cpm: 885,  variance: 145 } ],
      [ "sv", {cpm: 917,  variance: 156 } ],
      [ "tr", {cpm: 1054, variance: 156 } ],
      [ "zh", {cpm: 255,  variance: 29 } ],
    ]);

    return readingSpeed.get(lang) || readingSpeed.get("en");
   }

   /**
    * Attempts to find author information in the meta elements of the current page.
    */
   getByline() {
     var metadata = {};
     var values = {};
     var metaElements = [...document.getElementsByTagName("meta")];

     // property is a space-separated list of values
     var propertyPattern = /\s*(dc|dcterm|og|twitter)\s*:\s*(author|creator|description|title|site_name)\s*/gi;

     // name is a single value
     var namePattern = /^\s*(?:(dc|dcterm|og|twitter|weibo:(article|webpage))\s*[\.:]\s*)?(author|creator|description|title|site_name)\s*$/i;

     // Find description tags.
     metaElements.forEach((element) => {
       var elementName = element.getAttribute("name");
       var elementProperty = element.getAttribute("property");
       var content = element.getAttribute("content");
       if (!content) {
         return;
       }
       var matches = null;
       var name = null;

       if (elementProperty) {
         matches = elementProperty.match(propertyPattern);
         if (matches) {
           for (var i = matches.length - 1; i >= 0; i--) {
             // Convert to lowercase, and remove any whitespace
             // so we can match below.
             name = matches[i].toLowerCase().replace(/\s/g, "");
             // multiple authors
             values[name] = content.trim();
           }
         }
       }
       if (!matches && elementName && namePattern.test(elementName)) {
         name = elementName;
         if (content) {
           // Convert to lowercase, remove any whitespace, and convert dots
           // to colons so we can match below.
           name = name.toLowerCase().replace(/\s/g, "").replace(/\./g, ":");
           values[name] = content.trim();
         }
       }
     });

     return values["dc:creator"] || values["dcterm:creator"] || values["author"] || "";
   }

   /**
    * Attempts to read the optional text direction from the article and uses
    * language mapping to detect rtl, if missing.
    */
   getTextDirection(article) {
     if (article.dir) {
       return article.dir
     }

     if (["ar", "fa", "he", "ug", "ur"].includes(article.language)) {
       return "rtl";
     }

     return "ltr";
   }
}

let readerView = new ReaderView();

let port = browser.runtime.connectNative("mozacReaderview");
port.onMessage.addListener((message) => {
    switch (message.action) {
      case 'show':
        readerView.show(message.value);
        break;
      case 'hide':
        readerView.hide();
        break;
      case 'setColorScheme':
        readerView.setColorScheme(message.value.toLowerCase());
        break;
      case 'changeFontSize':
        readerView.changeFontSize(message.value);
        break;
      case 'setFontType':
        readerView.setFontType(message.value.toLowerCase());
        break;
      case 'checkReaderable':
        port.postMessage({readerable: ReaderView.isReaderable()});
        break;
      default:
        console.error(`Received invalid action ${message.action}`);
    }
});

window.addEventListener("unload", (event) => { port.disconnect() }, false);
