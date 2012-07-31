/*
 * This code is heavily based on Arc90's readability.js (1.7.1) script
 * available at: http://code.google.com/p/arc90labs-readability
 *
 * readability.js is licensed under the Apache License, Version 2.0
 * Copyright (c) 2010 Arc90 Inc
**/

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

function dump(s) {
  Services.console.logStringMessage("Reader: (Readability) " + s);
};

var Readability = function(uri, doc) {
  this._uri = uri;
  this._doc = doc;
  this._biggestFrame = false;

  // Start with all flags set
  this._flags = this.FLAG_STRIP_UNLIKELYS |
                this.FLAG_WEIGHT_CLASSES |
                this.FLAG_CLEAN_CONDITIONALLY;

  // The list of pages we've parsed in this call of readability,
  // for autopaging. As a key store for easier searching.
  this._parsedPages = {};

  // A list of the ETag headers of pages we've parsed, in case they happen to match,
  // we'll know it's a duplicate.
  this._pageETags = {};

  // Make an AJAX request for each page and append it to the document.
  this._curPageNum = 1;
}

Readability.prototype = {
  FLAG_STRIP_UNLIKELYS: 0x1,
  FLAG_WEIGHT_CLASSES: 0x2,
  FLAG_CLEAN_CONDITIONALLY: 0x4,
  FLAG_READABILITY_CHECK: 0x8,

  // The maximum number of pages to loop through before we call
  // it quits and just show a link.
  MAX_PAGES: 5,

  // The number of iterations processed before yielding.
  GEN_ITERATIONS: 100,

  // All of the regular expressions in use within readability.
  // Defined up here so we don't instantiate them repeatedly in loops.
  REGEXPS: {
    unlikelyCandidates: /combx|comment|community|disqus|extra|foot|header|menu|remark|rss|shoutbox|sidebar|sponsor|ad-break|agegate|pagination|pager|popup|tweet|twitter/i,
    okMaybeItsACandidate: /and|article|body|column|main|shadow/i,
    positive: /article|body|content|entry|hentry|main|page|pagination|post|text|blog|story/i,
    negative: /combx|comment|com-|contact|foot|footer|footnote|masthead|media|meta|outbrain|promo|related|scroll|shoutbox|sidebar|sponsor|shopping|tags|tool|widget/i,
    extraneous: /print|archive|comment|discuss|e[\-]?mail|share|reply|all|login|sign|single/i,
    divToPElements: /<(a|blockquote|dl|div|img|ol|p|pre|table|ul)/i,
    replaceBrs: /(<br[^>]*>[ \n\r\t]*){2,}/gi,
    replaceFonts: /<(\/?)font[^>]*>/gi,
    trim: /^\s+|\s+$/g,
    normalize: /\s{2,}/g,
    killBreaks: /(<br\s*\/?>(\s|&nbsp;?)*){1,}/g,
    videos: /http:\/\/(www\.)?(youtube|vimeo)\.com/i,
    nextLink: /(next|weiter|continue|>([^\|]|$)|»([^\|]|$))/i,
    prevLink: /(prev|earl|old|new|<|«)/i
  },

  /**
   * Run any post-process modifications to article content as necessary.
   *
   * @param Element
   * @return void
  **/
  _postProcessContent: function(articleContent) {
    this._fixImageFloats(articleContent);

    // Readability cannot open relative uris so we convert them to absolute uris. 
    this._fixRelativeUris(articleContent);
  },

  /**
   * Some content ends up looking ugly if the image is too large to be floated.
   * If the image is wider than a threshold (currently 55%), no longer float it,
   * center it instead.
   *
   * @param Element
   * @return void
  **/
  _fixImageFloats: function(articleContent) {
    let imageWidthThreshold = Math.min(articleContent.offsetWidth, 800) * 0.55;
    let images = articleContent.getElementsByTagName('img');

    for (let i = 0, il = images.length; i < il; i += 1) {
      let image = images[i];

      if (image.offsetWidth > imageWidthThreshold)
        image.className += " blockImage";
    }
  },

  /**
   * Converts each <a> and <img> uri in the given element to an absolute URI.
   *
   * @param Element
   * @return void
   */
  _fixRelativeUris: function(articleContent) {
    let baseUri = this._uri;

    // Fix links.
    let links = articleContent.getElementsByTagName('a');
    for (let i = links.length - 1; i >= 0; i--) {
      links[i].href = this._newURIErrorWrapper(links[i].href, baseUri);
    }

    // Fix images.
    let images = articleContent.getElementsByTagName('img');
    for (let i = images.length - 1; i >= 0; i--) {
      images[i].src = this._newURIErrorWrapper(images[i].src, baseUri);
    }
  },

  /**
   * Converts the given parameters into a new nsIURI object and returns the "spec" attribute of it.
   * Catches errors of the newURI method and returns an appropriate value.
   *
   * @param string
   * @param nsIURI
   * @return string
   */
  _newURIErrorWrapper: function(aSpec, aBaseURI) {
    try {
      return Services.io.newURI(aSpec, null, aBaseURI).spec;
    } catch (err) {
      dump("_newURIErrorWrapper: " + err.message);
      return "";
    }
  },

  /**
   * Get the article title as an H1.
   *
   * @return void
   **/
  _getArticleTitle: function() {
    let doc = this._doc;
    let curTitle = "";
    let origTitle = "";

    try {
      curTitle = origTitle = doc.title;

      // If they had an element with id "title" in their HTML
      if (typeof curTitle !== "string")
        curTitle = origTitle = this._getInnerText(doc.getElementsByTagName('title')[0]);
    } catch(e) {}

    if (curTitle.match(/ [\|\-] /)) {
      curTitle = origTitle.replace(/(.*)[\|\-] .*/gi,'$1');

      if (curTitle.split(' ').length < 3)
        curTitle = origTitle.replace(/[^\|\-]*[\|\-](.*)/gi,'$1');
    } else if (curTitle.indexOf(': ') !== -1) {
      curTitle = origTitle.replace(/.*:(.*)/gi, '$1');

      if (curTitle.split(' ').length < 3)
        curTitle = origTitle.replace(/[^:]*[:](.*)/gi,'$1');
    } else if (curTitle.length > 150 || curTitle.length < 15) {
      let hOnes = doc.getElementsByTagName('h1');

      if (hOnes.length === 1)
        curTitle = this._getInnerText(hOnes[0]);
    }

    curTitle = curTitle.replace(this.REGEXPS.trim, "");

    if (curTitle.split(' ').length <= 4)
      curTitle = origTitle;

    let articleTitle = doc.createElement("H1");
    articleTitle.innerHTML = curTitle;

    return articleTitle;
  },

  /**
   * Prepare the HTML document for readability to scrape it.
   * This includes things like stripping javascript, CSS, and handling terrible markup.
   *
   * @return void
   **/
  _prepDocument: function() {
    if (this._flagIsActive(this.FLAG_READABILITY_CHECK))
      return;

    let doc = this._doc;

    // In some cases a body element can't be found (if the HTML is
    // totally hosed for example) so we create a new body node and
    // append it to the document.
    if (doc.body === null) {
      let body = doc.createElement("body");

      try {
        doc.body = body;
      } catch(e) {
        doc.documentElement.appendChild(body);
        dump(e);
      }
    }

    let frames = doc.getElementsByTagName('frame');
    if (frames.length > 0) {
      let bestFrame = null;

      // The frame to try to run readability upon. Must be on same domain.
      let bestFrameSize = 0;

      // Used for the error message. Can be on any domain.
      let biggestFrameSize = 0;

      for (let frameIndex = 0; frameIndex < frames.length; frameIndex += 1) {
        let frameSize = frames[frameIndex].offsetWidth + frames[frameIndex].offsetHeight;

        let canAccessFrame = false;
        try {
          let frameBody = frames[frameIndex].contentWindow.document.body;
          canAccessFrame = true;
        } catch(eFrames) {
          dump(eFrames);
        }

        if (frameSize > biggestFrameSize) {
          biggestFrameSize = frameSize;
          this._biggestFrame = frames[frameIndex];
        }

        if (canAccessFrame && frameSize > bestFrameSize) {
          bestFrame = frames[frameIndex];
          bestFrameSize = frameSize;
        }
      }

      if (bestFrame) {
        let newBody = doc.createElement('body');
        newBody.innerHTML = bestFrame.contentWindow.document.body.innerHTML;
        newBody.style.overflow = 'scroll';
        doc.body = newBody;

        let frameset = doc.getElementsByTagName('frameset')[0];
        if (frameset) {
          frameset.parentNode.removeChild(frameset);
        }
      }
    }

    // Remove all stylesheets
    for (let k = 0; k < doc.styleSheets.length; k += 1) {
      doc.styleSheets[k].disabled = true;
    }

    // Remove all style tags in head
    let styleTags = doc.getElementsByTagName("style");
    for (let st = 0; st < styleTags.length; st += 1) {
      styleTags[st].textContent = "";
    }

    // Turn all double br's into p's. Note, this is pretty costly as far
    // as processing goes. Maybe optimize later.
    doc.body.innerHTML =
        doc.body.innerHTML.replace(this.REGEXPS.replaceBrs, '</p><p>').
            replace(this.REGEXPS.replaceFonts, '<$1span>');
  },

  /**
   * Prepare the article node for display. Clean out any inline styles,
   * iframes, forms, strip extraneous <p> tags, etc.
   *
   * @param Element
   * @return void
   **/
  _prepArticle: function(articleContent) {
    this._cleanStyles(this._doc, articleContent);
    this._killBreaks(articleContent);

    // Clean out junk from the article content
    this._cleanConditionally(articleContent, "form");
    this._clean(articleContent, "object");
    this._clean(articleContent, "h1");

    // If there is only one h2, they are probably using it as a header
    // and not a subheader, so remove it since we already have a header.
    if (articleContent.getElementsByTagName('h2').length === 1)
      this._clean(articleContent, "h2");

    this._clean(articleContent, "iframe");
    this._cleanHeaders(articleContent);

    // Do these last as the previous stuff may have removed junk
    // that will affect these
    this._cleanConditionally(articleContent, "table");
    this._cleanConditionally(articleContent, "ul");
    this._cleanConditionally(articleContent, "div");

    // Remove extra paragraphs
    let articleParagraphs = articleContent.getElementsByTagName('p');
    for (let i = articleParagraphs.length - 1; i >= 0; i -= 1) {
      let imgCount = articleParagraphs[i].getElementsByTagName('img').length;
      let embedCount = articleParagraphs[i].getElementsByTagName('embed').length;
      let objectCount = articleParagraphs[i].getElementsByTagName('object').length;

      if (imgCount === 0 &&
        embedCount === 0 &&
        objectCount === 0 &&
        this._getInnerText(articleParagraphs[i], false) === '') {
        articleParagraphs[i].parentNode.removeChild(articleParagraphs[i]);
      }
    }

    articleContent.innerHTML = articleContent.innerHTML.replace(/<br[^>]*>\s*<p/gi, '<p');
  },

  /**
   * Initialize a node with the readability object. Also checks the
   * className/id for special names to add to its score.
   *
   * @param Element
   * @return void
  **/
  _initializeNode: function(node) {
    node.readability = {"contentScore": 0};

    switch(node.tagName) {
      case 'DIV':
        node.readability.contentScore += 5;
        break;

      case 'PRE':
      case 'TD':
      case 'BLOCKQUOTE':
        node.readability.contentScore += 3;
        break;

      case 'ADDRESS':
      case 'OL':
      case 'UL':
      case 'DL':
      case 'DD':
      case 'DT':
      case 'LI':
      case 'FORM':
        node.readability.contentScore -= 3;
        break;

      case 'H1':
      case 'H2':
      case 'H3':
      case 'H4':
      case 'H5':
      case 'H6':
      case 'TH':
        node.readability.contentScore -= 5;
        break;
    }

    node.readability.contentScore += this._getClassWeight(node);
  },

  _grabArticle: function (callback) {
    let gen = this._grabArticleGenerator();
    let iterate = function () {
      for (let i = this.GEN_ITERATIONS; i--;) {
        let result;
        try {
          // Parse can be interrupted if document changes (will throw dead
          // object exception)
          result = gen.next();
        } catch (e) {
          dump("Caught exception while grabbing article, aborting");
          result = null;
        }
        if (result !== undefined) {
          callback(result);
          return;
        }
      }
      setTimeout(iterate, 0);
    }.bind(this);
    iterate();
  },

  /***
   * grabArticleGenerator - Using a variety of metrics (content score, classname, element types), find the content that is
   *         most likely to be the stuff a user wants to read. Then return it wrapped up in a div.
   *
   * @param page a document to run upon. Needs to be a full document, complete with body.
   * @return Element
  **/
  _grabArticleGenerator: function(page) {
    while (true) {
      let doc = this._doc;
      let stripUnlikelyCandidates = this._flagIsActive(this.FLAG_STRIP_UNLIKELYS);
      let isChecking = this._flagIsActive(this.FLAG_READABILITY_CHECK);
      let isPaging = (page !== null ? true: false);

      page = page ? page : this._doc.body;

      let pageCacheHtml = page.innerHTML;
      let allElements = page.getElementsByTagName('*');

      yield;

      // First, node prepping. Trash nodes that look cruddy (like ones with the
      // class name "comment", etc), and turn divs into P tags where they have been
      // used inappropriately (as in, where they contain no other block level elements.)
      //
      // Note: Assignment from index for performance. See http://www.peachpit.com/articles/article.aspx?p=31567&seqNum=5
      // TODO: Shouldn't this be a reverse traversal?
      let node = null;
      let nodesToScore = [];

      for (let nodeIndex = 0; (node = allElements[nodeIndex]); nodeIndex += 1) {
        // Remove unlikely candidates
        if (stripUnlikelyCandidates) {
          let unlikelyMatchString = node.className + node.id;
          if (unlikelyMatchString.search(this.REGEXPS.unlikelyCandidates) !== -1 &&
            unlikelyMatchString.search(this.REGEXPS.okMaybeItsACandidate) === -1 &&
            node.tagName !== "BODY") {
            dump("Removing unlikely candidate - " + unlikelyMatchString);
            node.parentNode.removeChild(node);
            nodeIndex -= 1;
            continue;
          }
        }

        if (node.tagName === "P" || node.tagName === "TD" || node.tagName === "PRE")
          nodesToScore[nodesToScore.length] = node;

        // Turn all divs that don't have children block level elements into p's
        if (node.tagName === "DIV") {
          if (node.innerHTML.search(this.REGEXPS.divToPElements) === -1) {
            if (!isChecking) {
              let newNode = doc.createElement('p');
              newNode.innerHTML = node.innerHTML;
              node.parentNode.replaceChild(newNode, node);
              nodeIndex -= 1;
            }

            nodesToScore[nodesToScore.length] = node;
          } else if (!isChecking) {
            // EXPERIMENTAL
            for (let i = 0, il = node.childNodes.length; i < il; i += 1) {
              let childNode = node.childNodes[i];
              if (!childNode)
                continue;

              if (childNode.nodeType === 3) { // Node.TEXT_NODE
                let p = doc.createElement('p');
                p.innerHTML = childNode.nodeValue;
                p.style.display = 'inline';
                p.className = 'readability-styled';

                childNode.parentNode.replaceChild(p, childNode);
              }
            }
          }
        }

        yield;
      }

      /**
       * Loop through all paragraphs, and assign a score to them based on how content-y they look.
       * Then add their score to their parent node.
       *
       * A score is determined by things like number of commas, class names, etc. Maybe eventually link density.
      **/
      let candidates = [];
      for (let pt = 0; pt < nodesToScore.length; pt += 1) {
        let parentNode = nodesToScore[pt].parentNode;
        let grandParentNode = parentNode ? parentNode.parentNode : null;
        let innerText = this._getInnerText(nodesToScore[pt]);

        if (!parentNode || typeof(parentNode.tagName) === 'undefined')
          continue;

        // If this paragraph is less than 25 characters, don't even count it.
        if (innerText.length < 25)
          continue;

        // Initialize readability data for the parent.
        if (typeof parentNode.readability === 'undefined') {
          this._initializeNode(parentNode);
          candidates.push(parentNode);
        }

        // Initialize readability data for the grandparent.
        if (grandParentNode &&
          typeof(grandParentNode.readability) === 'undefined' &&
          typeof(grandParentNode.tagName) !== 'undefined') {
          this._initializeNode(grandParentNode);
          candidates.push(grandParentNode);
        }

        let contentScore = 0;

        // Add a point for the paragraph itself as a base.
        contentScore += 1;

        // Add points for any commas within this paragraph.
        contentScore += innerText.split(',').length;

        // For every 100 characters in this paragraph, add another point. Up to 3 points.
        contentScore += Math.min(Math.floor(innerText.length / 100), 3);

        // Add the score to the parent. The grandparent gets half.
        parentNode.readability.contentScore += contentScore;

        if (grandParentNode)
          grandParentNode.readability.contentScore += contentScore / 2;

        yield;
      }

      // After we've calculated scores, loop through all of the possible
      // candidate nodes we found and find the one with the highest score.
      let topCandidate = null;
      for (let c = 0, cl = candidates.length; c < cl; c += 1) {
        // Scale the final candidates score based on link density. Good content
        // should have a relatively small link density (5% or less) and be mostly
        // unaffected by this operation.
        candidates[c].readability.contentScore =
            candidates[c].readability.contentScore * (1 - this._getLinkDensity(candidates[c]));

        dump('Candidate: ' + candidates[c] + " (" + candidates[c].className + ":" +
          candidates[c].id + ") with score " +
          candidates[c].readability.contentScore);

        if (!topCandidate ||
          candidates[c].readability.contentScore > topCandidate.readability.contentScore) {
          topCandidate = candidates[c];
        }

        yield;
      }

      // If we still have no top candidate, just use the body as a last resort.
      // We also have to copy the body node so it is something we can modify.
      if (topCandidate === null || topCandidate.tagName === "BODY") {
        // If we couldn't find a candidate for article content at this point,
        // it's very unlikely to be a convertible page, just bail the check.
        if (isChecking) {
          dump('No top candidate found, failed readability check');
          yield null;
        }

        topCandidate = doc.createElement("DIV");
        topCandidate.innerHTML = page.innerHTML;

        page.innerHTML = "";
        page.appendChild(topCandidate);

        this._initializeNode(topCandidate);
      } else if (isChecking) {
        dump('Found a top candidate, passed readability check');

        // Just return a non-null value, no need to post-process the article content
        // as we're just checking for readability.
        yield {};
      }

      // Now that we have the top candidate, look through its siblings for content
      // that might also be related. Things like preambles, content split by ads
      // that we removed, etc.
      let articleContent = doc.createElement("DIV");
      if (isPaging)
        articleContent.id = "readability-content";

      let siblingScoreThreshold = Math.max(10, topCandidate.readability.contentScore * 0.2);
      let siblingNodes = topCandidate.parentNode.childNodes;

      for (let s = 0, sl = siblingNodes.length; s < sl; s += 1) {
        let siblingNode = siblingNodes[s];
        let append = false;

        dump("Looking at sibling node: " + siblingNode + " (" + siblingNode.className + ":" + siblingNode.id + ")" + ((typeof siblingNode.readability !== 'undefined') ? (" with score " + siblingNode.readability.contentScore) : ''));
        dump("Sibling has score " + (siblingNode.readability ? siblingNode.readability.contentScore : 'Unknown'));

        if (siblingNode === topCandidate)
          append = true;

        let contentBonus = 0;

        // Give a bonus if sibling nodes and top candidates have the example same classname
        if (siblingNode.className === topCandidate.className && topCandidate.className !== "")
          contentBonus += topCandidate.readability.contentScore * 0.2;

        if (typeof siblingNode.readability !== 'undefined' &&
          (siblingNode.readability.contentScore+contentBonus) >= siblingScoreThreshold) {
          append = true;
        }

        if (siblingNode.nodeName === "P") {
          let linkDensity = this._getLinkDensity(siblingNode);
          let nodeContent = this._getInnerText(siblingNode);
          let nodeLength = nodeContent.length;

          if (nodeLength > 80 && linkDensity < 0.25) {
            append = true;
          } else if (nodeLength < 80 && linkDensity === 0 && nodeContent.search(/\.( |$)/) !== -1) {
            append = true;
          }
        }

        if (append) {
          dump("Appending node: " + siblingNode);

          let nodeToAppend = null;
          if (siblingNode.nodeName !== "DIV" && siblingNode.nodeName !== "P") {
            // We have a node that isn't a common block level element, like a form or td tag.
            // Turn it into a div so it doesn't get filtered out later by accident. */
            dump("Altering siblingNode of " + siblingNode.nodeName + ' to div.');

            nodeToAppend = doc.createElement("DIV");
            nodeToAppend.id = siblingNode.id;
            nodeToAppend.innerHTML = siblingNode.innerHTML;
          } else {
            nodeToAppend = siblingNode;
            s -= 1;
            sl -= 1;
          }

          // To ensure a node does not interfere with readability styles,
          // remove its classnames.
          nodeToAppend.className = "";

          // Append sibling and subtract from our list because it removes
          // the node when you append to another node.
          articleContent.appendChild(nodeToAppend);
        }

        yield;
      }

      // So we have all of the content that we need. Now we clean it up for presentation.
      this._prepArticle(articleContent);

      yield;

      if (this._curPageNum === 1)
        articleContent.innerHTML = '<div id="readability-page-1" class="page">' + articleContent.innerHTML + '</div>';

      // Now that we've gone through the full algorithm, check to see if
      // we got any meaningful content. If we didn't, we may need to re-run
      // grabArticle with different flags set. This gives us a higher likelihood of
      // finding the content, and the sieve approach gives us a higher likelihood of
      // finding the -right- content.
      if (this._getInnerText(articleContent, false).length < 250) {
        page.innerHTML = pageCacheHtml;

        if (this._flagIsActive(this.FLAG_STRIP_UNLIKELYS)) {
          this._removeFlag(this.FLAG_STRIP_UNLIKELYS);
        } else if (this._flagIsActive(this.FLAG_WEIGHT_CLASSES)) {
          this._removeFlag(this.FLAG_WEIGHT_CLASSES);
        } else if (this._flagIsActive(this.FLAG_CLEAN_CONDITIONALLY)) {
          this._removeFlag(this.FLAG_CLEAN_CONDITIONALLY);
        } else {
          yield null;
        }
      } else {
        yield articleContent;
      }
    }
  },

  /**
   * Removes script tags from the document.
   *
   * @param Element
  **/
  _removeScripts: function(doc) {
    if (this._flagIsActive(this.FLAG_READABILITY_CHECK))
      return;

    let scripts = doc.getElementsByTagName('script');
    for (let i = scripts.length - 1; i >= 0; i -= 1) {
      scripts[i].nodeValue="";
      scripts[i].removeAttribute('src');

      if (scripts[i].parentNode)
          scripts[i].parentNode.removeChild(scripts[i]);
    }
  },

  /**
   * Get the inner text of a node - cross browser compatibly.
   * This also strips out any excess whitespace to be found.
   *
   * @param Element
   * @return string
  **/
  _getInnerText: function(e, normalizeSpaces) {
    let textContent = "";

    if (typeof(e.textContent) === "undefined" && typeof(e.innerText) === "undefined")
      return "";

    normalizeSpaces = (typeof normalizeSpaces === 'undefined') ? true : normalizeSpaces;
    textContent = e.textContent.replace(this.REGEXPS.trim, "");

    if (normalizeSpaces) {
      return textContent.replace(this.REGEXPS.normalize, " ");
    } else {
      return textContent;
    }
  },

  /**
   * Get the number of times a string s appears in the node e.
   *
   * @param Element
   * @param string - what to split on. Default is ","
   * @return number (integer)
  **/
  _getCharCount: function(e,s) {
    s = s || ",";
    return this._getInnerText(e).split(s).length - 1;
  },

  /**
   * Remove the style attribute on every e and under.
   * TODO: Test if getElementsByTagName(*) is faster.
   *
   * @param Element
   * @return void
  **/
  _cleanStyles: function(e) {
    e = e || this._doc;
    let cur = e.firstChild;

    if (!e)
      return;

    // Remove any root styles, if we're able.
    if (typeof e.removeAttribute === 'function' && e.className !== 'readability-styled')
      e.removeAttribute('style');

    // Go until there are no more child nodes
    while (cur !== null) {
      if (cur.nodeType === 1) {
        // Remove style attribute(s) :
        if (cur.className !== "readability-styled")
          cur.removeAttribute("style");

        this._cleanStyles(cur);
      }

      cur = cur.nextSibling;
    }
  },

  /**
   * Get the density of links as a percentage of the content
   * This is the amount of text that is inside a link divided by the total text in the node.
   *
   * @param Element
   * @return number (float)
  **/
  _getLinkDensity: function(e) {
    let links = e.getElementsByTagName("a");
    let textLength = this._getInnerText(e).length;
    let linkLength = 0;

    for (let i = 0, il = links.length; i < il; i += 1) {
      linkLength += this._getInnerText(links[i]).length;
    }

    return linkLength / textLength;
  },

  /**
   * Find a cleaned up version of the current URL, to use for comparing links for possible next-pageyness.
   *
   * @author Dan Lacy
   * @return string the base url
  **/
  _findBaseUrl: function() {
    let uri = this._uri;
    let noUrlParams = uri.path.split("?")[0];
    let urlSlashes = noUrlParams.split("/").reverse();
    let cleanedSegments = [];
    let possibleType = "";

    for (let i = 0, slashLen = urlSlashes.length; i < slashLen; i += 1) {
      let segment = urlSlashes[i];

      // Split off and save anything that looks like a file type.
      if (segment.indexOf(".") !== -1) {
        possibleType = segment.split(".")[1];

        // If the type isn't alpha-only, it's probably not actually a file extension.
        if (!possibleType.match(/[^a-zA-Z]/))
          segment = segment.split(".")[0];
      }

      // EW-CMS specific segment replacement. Ugly.
      // Example: http://www.ew.com/ew/article/0,,20313460_20369436,00.html
      if (segment.indexOf(',00') !== -1)
        segment = segment.replace(',00', '');

      // If our first or second segment has anything looking like a page number, remove it.
      if (segment.match(/((_|-)?p[a-z]*|(_|-))[0-9]{1,2}$/i) && ((i === 1) || (i === 0)))
        segment = segment.replace(/((_|-)?p[a-z]*|(_|-))[0-9]{1,2}$/i, "");

      let del = false;

      // If this is purely a number, and it's the first or second segment,
      // it's probably a page number. Remove it.
      if (i < 2 && segment.match(/^\d{1,2}$/))
        del = true;

      // If this is the first segment and it's just "index", remove it.
      if (i === 0 && segment.toLowerCase() === "index")
        del = true;

      // If our first or second segment is smaller than 3 characters,
      // and the first segment was purely alphas, remove it.
      if (i < 2 && segment.length < 3 && !urlSlashes[0].match(/[a-z]/i))
        del = true;

      // If it's not marked for deletion, push it to cleanedSegments.
      if (!del)
        cleanedSegments.push(segment);
    }

    // This is our final, cleaned, base article URL.
    return uri.scheme + "://" + uri.host + cleanedSegments.reverse().join("/");
  },

  /**
   * Look for any paging links that may occur within the document.
   *
   * @param body
   * @return object (array)
  **/
  _findNextPageLink: function(elem) {
    let uri = this._uri;
    let possiblePages = {};
    let allLinks = elem.getElementsByTagName('a');
    let articleBaseUrl = this._findBaseUrl();

    // Loop through all links, looking for hints that they may be next-page links.
    // Things like having "page" in their textContent, className or id, or being a child
    // of a node with a page-y className or id.
    //
    // Also possible: levenshtein distance? longest common subsequence?
    //
    // After we do that, assign each page a score, and
    for (let i = 0, il = allLinks.length; i < il; i += 1) {
      let link = allLinks[i];
      let linkHref = allLinks[i].href.replace(/#.*$/, '').replace(/\/$/, '');

      // If we've already seen this page, ignore it.
      if (linkHref === "" ||
        linkHref === articleBaseUrl ||
        linkHref === uri.spec ||
        linkHref in this._parsedPages) {
        continue;
      }

      // If it's on a different domain, skip it.
      if (uri.host !== linkHref.split(/\/+/g)[1])
        continue;

      let linkText = this._getInnerText(link);

      // If the linkText looks like it's not the next page, skip it.
      if (linkText.match(this.REGEXPS.extraneous) || linkText.length > 25)
        continue;

      // If the leftovers of the URL after removing the base URL don't contain
      // any digits, it's certainly not a next page link.
      let linkHrefLeftover = linkHref.replace(articleBaseUrl, '');
      if (!linkHrefLeftover.match(/\d/))
        continue;

      if (!(linkHref in possiblePages)) {
        possiblePages[linkHref] = {"score": 0, "linkText": linkText, "href": linkHref};
      } else {
        possiblePages[linkHref].linkText += ' | ' + linkText;
      }

      let linkObj = possiblePages[linkHref];

      // If the articleBaseUrl isn't part of this URL, penalize this link. It could
      // still be the link, but the odds are lower.
      // Example: http://www.actionscript.org/resources/articles/745/1/JavaScript-and-VBScript-Injection-in-ActionScript-3/Page1.html
      if (linkHref.indexOf(articleBaseUrl) !== 0)
        linkObj.score -= 25;

      let linkData = linkText + ' ' + link.className + ' ' + link.id;
      if (linkData.match(this.REGEXPS.nextLink))
        linkObj.score += 50;

      if (linkData.match(/pag(e|ing|inat)/i))
        linkObj.score += 25;

      if (linkData.match(/(first|last)/i)) {
        // -65 is enough to negate any bonuses gotten from a > or » in the text,
        // If we already matched on "next", last is probably fine.
        // If we didn't, then it's bad. Penalize.
        if (!linkObj.linkText.match(this.REGEXPS.nextLink))
          linkObj.score -= 65;
      }

      if (linkData.match(this.REGEXPS.negative) || linkData.match(this.REGEXPS.extraneous))
        linkObj.score -= 50;

      if (linkData.match(this.REGEXPS.prevLink))
        linkObj.score -= 200;

      // If a parentNode contains page or paging or paginat
      let parentNode = link.parentNode;
      let positiveNodeMatch = false;
      let negativeNodeMatch = false;

      while (parentNode) {
        let parentNodeClassAndId = parentNode.className + ' ' + parentNode.id;

        if (!positiveNodeMatch && parentNodeClassAndId && parentNodeClassAndId.match(/pag(e|ing|inat)/i)) {
          positiveNodeMatch = true;
          linkObj.score += 25;
        }

        if (!negativeNodeMatch && parentNodeClassAndId && parentNodeClassAndId.match(this.REGEXPS.negative)) {
          // If this is just something like "footer", give it a negative.
          // If it's something like "body-and-footer", leave it be.
          if (!parentNodeClassAndId.match(this.REGEXPS.positive)) {
            linkObj.score -= 25;
            negativeNodeMatch = true;
          }
        }

        parentNode = parentNode.parentNode;
      }

      // If the URL looks like it has paging in it, add to the score.
      // Things like /page/2/, /pagenum/2, ?p=3, ?page=11, ?pagination=34
      if (linkHref.match(/p(a|g|ag)?(e|ing|ination)?(=|\/)[0-9]{1,2}/i) || linkHref.match(/(page|paging)/i))
        linkObj.score += 25;

      // If the URL contains negative values, give a slight decrease.
      if (linkHref.match(this.REGEXPS.extraneous))
        linkObj.score -= 15;

      /**
       * Minor punishment to anything that doesn't match our current URL.
       * NOTE: I'm finding this to cause more harm than good where something is exactly 50 points.
       *     Dan, can you show me a counterexample where this is necessary?
       * if (linkHref.indexOf(window.location.href) !== 0) {
       *  linkObj.score -= 1;
       * }
      **/

      // If the link text can be parsed as a number, give it a minor bonus, with a slight
      // bias towards lower numbered pages. This is so that pages that might not have 'next'
      // in their text can still get scored, and sorted properly by score.
      let linkTextAsNumber = parseInt(linkText, 10);
      if (linkTextAsNumber) {
        // Punish 1 since we're either already there, or it's probably
        // before what we want anyways.
        if (linkTextAsNumber === 1) {
          linkObj.score -= 10;
        } else {
          linkObj.score += Math.max(0, 10 - linkTextAsNumber);
        }
      }
    }

    // Loop thrugh all of our possible pages from above and find our top
    // candidate for the next page URL. Require at least a score of 50, which
    // is a relatively high confidence that this page is the next link.
    let topPage = null;
    for (let page in possiblePages) {
      if (possiblePages.hasOwnProperty(page)) {
        if (possiblePages[page].score >= 50 &&
          (!topPage || topPage.score < possiblePages[page].score)) {
          topPage = possiblePages[page];
        }
      }
    }

    if (topPage) {
      let nextHref = topPage.href.replace(/\/$/,'');

      dump('NEXT PAGE IS ' + nextHref);
      this._parsedPages[nextHref] = true;
      return nextHref;
    } else {
      return null;
    }
  },

  _successfulRequest: function(request) {
    return (request.status >= 200 && request.status < 300) ||
        request.status === 304 ||
         (request.status === 0 && request.responseText);
  },

  _ajax: function(url, options) {
    let request = new XMLHttpRequest();

    function respondToReadyState(readyState) {
      if (request.readyState === 4) {
        if (this._successfulRequest(request)) {
          if (options.success) {
            options.success(request);
          }
        } else {
          if (options.error) {
            options.error(request);
          }
        }
      }
    }

    if (typeof options === 'undefined')
      options = {};

    request.onreadystatechange = respondToReadyState;

    request.open('get', url, true);
    request.setRequestHeader('Accept', 'text/html');

    try {
      request.send(options.postBody);
    } catch (e) {
      if (options.error)
        options.error();
    }

    return request;
  },

  _appendNextPage: function(nextPageLink) {
    let doc = this._doc;
    this._curPageNum += 1;

    let articlePage = doc.createElement("DIV");
    articlePage.id = 'readability-page-' + this._curPageNum;
    articlePage.className = 'page';
    articlePage.innerHTML = '<p class="page-separator" title="Page ' + this._curPageNum + '">&sect;</p>';

    doc.getElementById("readability-content").appendChild(articlePage);

    if (this._curPageNum > this.MAX_PAGES) {
      let nextPageMarkup = "<div style='text-align: center'><a href='" + nextPageLink + "'>View Next Page</a></div>";
      articlePage.innerHTML = articlePage.innerHTML + nextPageMarkup;
      return;
    }

    // Now that we've built the article page DOM element, get the page content
    // asynchronously and load the cleaned content into the div we created for it.
    (function(pageUrl, thisPage) {
      this._ajax(pageUrl, {
        success: function(r) {

          // First, check to see if we have a matching ETag in headers - if we do, this is a duplicate page.
          let eTag = r.getResponseHeader('ETag');
          if (eTag) {
            if (eTag in this._pageETags) {
              dump("Exact duplicate page found via ETag. Aborting.");
              articlePage.style.display = 'none';
              return;
            } else {
              this._pageETags[eTag] = 1;
            }
          }

          // TODO: this ends up doubling up page numbers on NYTimes articles. Need to generically parse those away.
          let page = doc.createElement("DIV");

          // Do some preprocessing to our HTML to make it ready for appending.
          // - Remove any script tags. Swap and reswap newlines with a unicode
          //   character because multiline regex doesn't work in javascript.
          // - Turn any noscript tags into divs so that we can parse them. This
          //   allows us to find any next page links hidden via javascript.
          // - Turn all double br's into p's - was handled by prepDocument in the original view.
          //   Maybe in the future abstract out prepDocument to work for both the original document
          //   and AJAX-added pages.
          let responseHtml = r.responseText.replace(/\n/g,'\uffff').replace(/<script.*?>.*?<\/script>/gi, '');
          responseHtml = responseHtml.replace(/\n/g,'\uffff').replace(/<script.*?>.*?<\/script>/gi, '');
          responseHtml = responseHtml.replace(/\uffff/g,'\n').replace(/<(\/?)noscript/gi, '<$1div');
          responseHtml = responseHtml.replace(this.REGEXPS.replaceBrs, '</p><p>');
          responseHtml = responseHtml.replace(this.REGEXPS.replaceFonts, '<$1span>');

          page.innerHTML = responseHtml;

          // Reset all flags for the next page, as they will search through it and
          // disable as necessary at the end of grabArticle.
          this._flags = 0x1 | 0x2 | 0x4;

          let nextPageLink = this._findNextPageLink(page);
          
          // NOTE: if we end up supporting _appendNextPage(), we'll need to
          // change this call to be async
          let content = this._grabArticle(page);

          if (!content) {
            dump("No content found in page to append. Aborting.");
            return;
          }

          // Anti-duplicate mechanism. Essentially, get the first paragraph of our new page.
          // Compare it against all of the the previous document's we've gotten. If the previous
          // document contains exactly the innerHTML of this first paragraph, it's probably a duplicate.
          let firstP = content.getElementsByTagName("P").length ? content.getElementsByTagName("P")[0] : null;
          if (firstP && firstP.innerHTML.length > 100) {
            for (let i = 1; i <= this._curPageNum; i += 1) {
              let rPage = doc.getElementById('readability-page-' + i);
              if (rPage && rPage.innerHTML.indexOf(firstP.innerHTML) !== -1) {
                dump('Duplicate of page ' + i + ' - skipping.');
                articlePage.style.display = 'none';
                this._parsedPages[pageUrl] = true;
                return;
              }
            }
          }

          this._removeScripts(content);

          thisPage.innerHTML = thisPage.innerHTML + content.innerHTML;

          // After the page has rendered, post process the content. This delay is necessary because,
          // in webkit at least, offsetWidth is not set in time to determine image width. We have to
          // wait a little bit for reflow to finish before we can fix floating images.
          setTimeout((function() {
            this._postProcessContent(thisPage);
          }).bind(this), 500);


          if (nextPageLink)
            this._appendNextPage(nextPageLink);
        }
      });
    }).bind(this)(nextPageLink, articlePage);
  },

  /**
   * Get an elements class/id weight. Uses regular expressions to tell if this 
   * element looks good or bad.
   *
   * @param Element
   * @return number (Integer)
  **/
  _getClassWeight: function(e) {
    if (!this._flagIsActive(this.FLAG_WEIGHT_CLASSES))
      return 0;

    let weight = 0;

    // Look for a special classname
    if (typeof(e.className) === 'string' && e.className !== '') {
      if (e.className.search(this.REGEXPS.negative) !== -1)
        weight -= 25;

      if (e.className.search(this.REGEXPS.positive) !== -1)
        weight += 25;
    }

    // Look for a special ID
    if (typeof(e.id) === 'string' && e.id !== '') {
      if (e.id.search(this.REGEXPS.negative) !== -1)
        weight -= 25;

      if (e.id.search(this.REGEXPS.positive) !== -1)
        weight += 25;
    }

    return weight;
  },

  /**
   * Remove extraneous break tags from a node.
   *
   * @param Element
   * @return void
   **/
  _killBreaks: function(e) {
    e.innerHTML = e.innerHTML.replace(this.REGEXPS.killBreaks,'<br />');
  },

  /**
   * Clean a node of all elements of type "tag".
   * (Unless it's a youtube/vimeo video. People love movies.)
   *
   * @param Element
   * @param string tag to clean
   * @return void
   **/
  _clean: function(e, tag) {
    let targetList = e.getElementsByTagName(tag);
    let isEmbed = (tag === 'object' || tag === 'embed');

    for (let y = targetList.length - 1; y >= 0; y -= 1) {
      // Allow youtube and vimeo videos through as people usually want to see those.
      if (isEmbed) {
        let attributeValues = "";
        for (let i = 0, il = targetList[y].attributes.length; i < il; i += 1) {
          attributeValues += targetList[y].attributes[i].value + '|';
        }

        // First, check the elements attributes to see if any of them contain youtube or vimeo
        if (attributeValues.search(this.REGEXPS.videos) !== -1)
          continue;

        // Then check the elements inside this element for the same.
        if (targetList[y].innerHTML.search(this.REGEXPS.videos) !== -1)
          continue;
      }

      targetList[y].parentNode.removeChild(targetList[y]);
    }
  },

  /**
   * Clean an element of all tags of type "tag" if they look fishy.
   * "Fishy" is an algorithm based on content length, classnames, link density, number of images & embeds, etc.
   *
   * @return void
   **/
  _cleanConditionally: function(e, tag) {
    if (!this._flagIsActive(this.FLAG_CLEAN_CONDITIONALLY))
      return;

    let tagsList = e.getElementsByTagName(tag);
    let curTagsLength = tagsList.length;

    // Gather counts for other typical elements embedded within.
    // Traverse backwards so we can remove nodes at the same time
    // without effecting the traversal.
    //
    // TODO: Consider taking into account original contentScore here.
    for (let i = curTagsLength-1; i >= 0; i -= 1) {
      let weight = this._getClassWeight(tagsList[i]);
      let contentScore = (typeof tagsList[i].readability !== 'undefined') ? tagsList[i].this._contentScore : 0;

      dump("Cleaning Conditionally " + tagsList[i] + " (" + tagsList[i].className + ":" + tagsList[i].id + ")" + ((typeof tagsList[i].readability !== 'undefined') ? (" with score " + tagsList[i].this._contentScore) : ''));

      if (weight + contentScore < 0) {
        tagsList[i].parentNode.removeChild(tagsList[i]);
      } else if (this._getCharCount(tagsList[i],',') < 10) {
        // If there are not very many commas, and the number of
        // non-paragraph elements is more than paragraphs or other
        // ominous signs, remove the element.
        let p = tagsList[i].getElementsByTagName("p").length;
        let img = tagsList[i].getElementsByTagName("img").length;
        let li = tagsList[i].getElementsByTagName("li").length-100;
        let input = tagsList[i].getElementsByTagName("input").length;

        let embedCount = 0;
        let embeds = tagsList[i].getElementsByTagName("embed");
        for (let ei = 0, il = embeds.length; ei < il; ei += 1) {
          if (embeds[ei].src.search(this.REGEXPS.videos) === -1)
            embedCount += 1;
        }

        let linkDensity = this._getLinkDensity(tagsList[i]);
        let contentLength = this._getInnerText(tagsList[i]).length;
        let toRemove = false;

        if (img > p) {
          toRemove = true;
        } else if (li > p && tag !== "ul" && tag !== "ol") {
          toRemove = true;
        } else if ( input > Math.floor(p/3) ) {
          toRemove = true;
        } else if (contentLength < 25 && (img === 0 || img > 2) ) {
          toRemove = true;
        } else if (weight < 25 && linkDensity > 0.2) {
          toRemove = true;
        } else if (weight >= 25 && linkDensity > 0.5) {
          toRemove = true;
        } else if ((embedCount === 1 && contentLength < 75) || embedCount > 1) {
          toRemove = true;
        }

        if (toRemove)
          tagsList[i].parentNode.removeChild(tagsList[i]);
      }
    }
  },

  /**
   * Clean out spurious headers from an Element. Checks things like classnames and link density.
   *
   * @param Element
   * @return void
  **/
  _cleanHeaders: function(e) {
    for (let headerIndex = 1; headerIndex < 3; headerIndex += 1) {
      let headers = e.getElementsByTagName('h' + headerIndex);
      for (let i = headers.length - 1; i >= 0; i -= 1) {
        if (this._getClassWeight(headers[i]) < 0 || this._getLinkDensity(headers[i]) > 0.33)
          headers[i].parentNode.removeChild(headers[i]);
      }
    }
  },

  _flagIsActive: function(flag) {
    return (this._flags & flag) > 0;
  },

  _addFlag: function(flag) {
    this._flags = this._flags | flag;
  },

  _removeFlag: function(flag) {
    this._flags = this._flags & ~flag;
  },

  /**
   * Runs readability.
   *
   * Workflow:
   *  1. Prep the document by removing script tags, css, etc.
   *  2. Build readability's DOM tree.
   *  3. Grab the article content from the current dom tree.
   *  4. Replace the current DOM tree with the new one.
   *  5. Read peacefully.
   *
   * @return void
   **/
  parse: function (callback) {
    let uri = this._uri;
    if ((uri.prePath + "/") === uri.spec) {
      callback(null);
      return;
    }

    // Remove script tags from the document.
    this._removeScripts(this._doc);

    // FIXME: Disabled multi-page article support for now as it
    // needs more work on infrastructure.

    // Make sure this document is added to the list of parsed pages first,
    // so we don't double up on the first page.
    // this._parsedPages[uri.spec.replace(/\/$/, '')] = true;

    // Pull out any possible next page link first.
    // let nextPageLink = this._findNextPageLink(doc.body);

    this._prepDocument();

    let articleTitle = this._getArticleTitle();
    this._grabArticle(function (articleContent) {
      if (!articleContent) {
        callback(null);
        return;
      }

      // If we're simply checking whether the document is convertible
      // or not, we don't need to do any post-processing on the article
      // content, just return a non-null value (see check() method)
      if (this._flagIsActive(this.FLAG_READABILITY_CHECK)) {
        callback({});
        return;
      }

      this._postProcessContent(articleContent);

      // if (nextPageLink) {
      //   // Append any additional pages after a small timeout so that people
      //   // can start reading without having to wait for this to finish processing.
      //   setTimeout((function() {
      //     this._appendNextPage(nextPageLink);
      //   }).bind(this), 500);
      // }

      callback({ title: this._getInnerText(articleTitle),
                 content: articleContent.innerHTML });
    }.bind(this));
  },

  check: function (callback) {
    // Set proper flags for parsing document in readability check mode, skipping
    // any DOM manipulation.
    this._flags = this.FLAG_READABILITY_CHECK;

    this.parse(function (result) {
      callback(result != null);
    });
  }
};
