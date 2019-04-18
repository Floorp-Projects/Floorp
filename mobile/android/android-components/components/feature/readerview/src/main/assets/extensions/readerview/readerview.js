/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Avoid adding ID selector rules in this style sheet, since they could
 * inadvertently match elements in the article content. */

class ReaderView {

  static isReaderable() {
    return isProbablyReaderable(document);
  }

  static get MIN_FONT_SIZE() {
    return 1;
  }

  static get MAX_FONT_SIZE() {
    return 9;
  }

  constructor(document) {
    this.document = document;
    this.originalBody = document.body.outerHTML;
  }

  show({fontSize = 4, fontType = "sans-serif", colorScheme = "light"} = {}) {
    var documentClone = document.cloneNode(true);
    var result = new Readability(documentClone).parse();
    result.language = document.documentElement.lang;

    var article = Object.assign(
      result,
      {url: location.hostname},
      {readingTime: this.getReadingTime(result.length, result.language)},
      {byline: this.getByline()},
      {dir: this.getTextDirection(result)}
    );

    document.body.outerHTML = this.createHtml(article);
    this.setFontSize(fontSize);
    this.setFontType(fontType);
    this.setColorScheme(colorScheme);
  }

  hide() {
    document.body.outerHTML = this.originalBody;
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
    let readerView = document.getElementById("readerview");
    readerView.style.setProperty("font-size", size);
    this.fontSize = fontSize;
  }

  /**
   * Sets the font type.
   *
   * @param fontType the font type to use.
   */
  setFontType(fontType) {
    if (this.fontType === fontType) {
      return;
    }

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

    if (this.colorScheme === colorScheme) {
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
      <body class="readerview-body">
        <div id="readerview" class="container" dir=${article.dir}>
          <div class="header reader-header">
            <a class="domain reader-domain">${article.url}</a>
            <div class="domain-border"></div>
            <h1 class="reader-title">${article.title}</h1>
            <div class="credits reader-credits">${article.byline}</div>
            <div class="meta-data">
              <div class="reader-estimated-time">${article.readingTime}</div>
            </div>
          </div>
          <hr>

          <div class="content">
            <div class="moz-reader-content">${article.content}</div>
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

// TODO remove hostname check (for testing purposes only)
// e.g. https://blog.mozilla.org/firefox/reader-view
if (ReaderView.isReaderable() && location.hostname.endsWith("blog.mozilla.org")) {
  // TODO send message to app to inform that readerview is available
  // For now we show reader view for every page on blog.mozilla.org
  let readerView = new ReaderView(document);
  // TODO Parameters need to be passed down in message to display readerview
  readerView.show({fontSize: 3, fontType: "serif", colorScheme: "light"});
}
