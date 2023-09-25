/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * Map the NodeFilter enums that are used by the TreeWalker into enums that make
 * sense for determining the status of the nodes for the TranslationsDocument process.
 * This aligns the meanings of the filtering for the translations process.
 */
const NodeStatus = {
  // This node is ready to translate as is.
  READY_TO_TRANSLATE: NodeFilter.FILTER_ACCEPT,

  // This node is a shadow host and needs to be subdivided further.
  SHADOW_HOST: NodeFilter.FILTER_ACCEPT,

  // This node contains too many block elements and needs to be subdivided further.
  SUBDIVIDE_FURTHER: NodeFilter.FILTER_SKIP,

  // This node should not be considered for translation.
  NOT_TRANSLATABLE: NodeFilter.FILTER_REJECT,
};

/**
 * @typedef {import("../translations").NodeVisibility} NodeVisibility
 * @typedef {(message: string) => Promise<string>} TranslationFunction
 */

/**
 * How often the DOM is updated with translations, in milliseconds.
 */
const DOM_UPDATE_INTERVAL_MS = 50;

/**
 * These tags are excluded from translation.
 */
const EXCLUDED_TAGS = new Set([
  // The following are elements that semantically should not be translated.
  "CODE",
  "KBD",
  "SAMP",
  "VAR",
  "ACRONYM",

  // The following are deprecated tags.
  "DIR",
  "APPLET",

  // The following are embedded elements, and are not supported (yet).
  "SVG",
  "MATH",
  "EMBED",
  "OBJECT",
  "IFRAME",

  // These are elements that are treated as opaque by Firefox which causes their
  // innerHTML property to be just the raw text node behind it. Any text that is sent as
  // HTML must be valid, and there is no guarantee that the innerHTML is valid.
  "NOSCRIPT",
  "NOEMBED",
  "NOFRAMES",

  // The title is handled separately, and a HEAD tag should not be considered.
  "HEAD",

  // These are not user-visible tags.
  "STYLE",
  "SCRIPT",
  "TEMPLATE",

  // Textarea elements contain user content, which should not be translated.
  "TEXTAREA",
]);

// Tags that are treated as assumed inline. This list has been created by heuristics
// and excludes some commonly inline tags, due to how they are used practically.
//
// An actual list of inline elements is available here:
// https://developer.mozilla.org/en-US/docs/Web/HTML/Inline_elements#list_of_inline_elements
const INLINE_TAGS = new Set([
  "ABBR",
  "B",
  "CODE",
  "DEL",
  "EM",
  "I",
  "INS",
  "KBD",
  "MARK",
  "MATH",
  "OUTPUT",
  "Q",
  "RUBY",
  "SMALL",
  "STRONG",
  "SUB",
  "SUP",
  "TIME",
  "U",
  "VAR",
  "WBR",

  // These are not really inline, but bergamot-translator treats these as
  // sentence-breaking.
  "BR",
  "TD",
  "TH",
  "LI",
]);

/**
 * Tags that can't reliably be assumed to be inline or block elements. They default
 * to inline, but are often used as block elements.
 */
const GENERIC_TAGS = new Set(["A", "SPAN"]);

/**
 * This class manages the process of translating the DOM from one language to another.
 * A translateHTML and a translateText function are injected into the constructor. This
 * class is responsible for subdividing a Node into small enough pieces to where it
 * contains a reasonable amount of text and inline elements for the translations engine
 * to translate. Once a node has been identified as a small enough chunk, its innerHTML
 * is read, and sent for translation. The async translation result comes back as an HTML
 * string. The DOM node is updated with the new text and potentially changed DOM ordering.
 *
 * This class also handles mutations of the DOM and will translate nodes as they are added
 * to the page, or the when the node's text is changed by content scripts.
 */
export class TranslationsDocument {
  /**
   * The BCP 47 language tag that is used on the page.
   *
   * @type {string} */
  documentLanguage;

  /**
   * The timeout between the first translation received and the call to update the DOM
   * with translations.
   */
  #updateTimeout = null;

  /**
   * The nodes that need translations. They are queued when the document tree is walked,
   * and then they are dispatched for translation based on their visibility. The viewport
   * nodes are given the highest priority.
   *
   * @type {Map<Node, NodeVisibility>}
   */
  #queuedNodes = new Map();

  /**
   * The count of how many pending translations have been sent to the translations
   * engine.
   */
  #pendingTranslationsCount = 0;

  /**
   * The list of nodes that need updating with the translated HTML. These are batched
   * into an update.
   *
   * @type {Set<{ node: Node, translatedHTML: string }}
   */
  #nodesWithTranslatedHTML = new Set();

  /**
   * The set of nodes that have been subdivided and processed for translation. They
   * should not be submitted again unless their contents have been changed.
   *
   * @type {WeakSet<Node>}
   */
  #processedNodes = new WeakSet();

  /**
   * All root elements we're trying to translate. This should be the `document.body`
   * and the the `title` element.
   *
   * @type {Set<Node>}
   */
  #rootNodes = new Set();

  /**
   * This promise gets resolved when the initial viewport translations are done.
   * This is a key user-visible performance metric. It represents what the user
   * actually sees.
   *
   * @type {Promise<void> | null}
   */
  viewportTranslated = null;

  /**
   * Construct a new TranslationsDocument. It is tied to a specific Document and cannot
   * be re-used. The translation functions are injected since this class shouldn't
   * manage the life cycle of the translations engines.
   *
   * @param {Document} document
   * @param {string} documentLanguage - The BCP 47 language tag.
   * @param {number} innerWindowId - This is used for better profiler marker reporting.
   * @param {TranslationFunction} translateHTML
   * @param {TranslationFunction} translateText
   */
  constructor(
    document,
    documentLanguage,
    innerWindowId,
    translateHTML,
    translateText
  ) {
    /**
     * The language of the document. If elements are found that do not match this language,
     * then they are skipped.
     *
     * @type {string}
     */
    this.documentLanguage = documentLanguage;
    if (documentLanguage.length !== 2) {
      throw new Error(
        "Expected the language to be a valid 2 letter BCP 47 language tag: " +
          documentLanguage
      );
    }

    /** @type {TranslationFunction} */
    this.translateHTML = translateHTML;

    /** @type {TranslationFunction} */
    this.translateText = translateText;

    /** @type {number} */
    this.innerWindowId = innerWindowId;

    /** @type {DOMParser} */
    this.domParser = new document.ownerGlobal.DOMParser();

    /**
     * This selector runs to find child nodes that should be excluded. It should be
     * basically the same implementation of `isExcludedNode`, but as a selector.
     *
     * @type {string}
     */
    this.excludedNodeSelector = [
      // Use: [lang|=value] to match language codes.
      //
      // Per: https://developer.mozilla.org/en-US/docs/Web/CSS/Attribute_selectors
      //
      // The elements with an attribute name of attr whose value can be exactly
      // value or can begin with value immediately followed by a hyphen, - (U+002D).
      // It is often used for language subcode matches.
      `[lang]:not([lang|="${this.documentLanguage}"])`,
      `[translate=no]`,
      `.notranslate`,
      `[contenteditable="true"]`,
      `[contenteditable=""]`,
      [...EXCLUDED_TAGS].join(","),
    ].join(",");

    this.observer = new document.ownerGlobal.MutationObserver(mutationsList => {
      for (const mutation of mutationsList) {
        switch (mutation.type) {
          case "childList":
            for (const node of mutation.addedNodes) {
              this.#processedNodes.delete(node);
              this.subdivideNodeForTranslations(node);
            }
            break;
          case "characterData":
            this.#processedNodes.delete(mutation);
            this.subdivideNodeForTranslations(mutation.target);
            break;
          default:
            break;
        }
      }
    });
  }

  /**
   * Add a new element to start translating. This root is tracked for mutations and
   * kept up to date with translations. This will be the body element and title tag
   * for the document.
   *
   * @param {Element} [node]
   */
  addRootElement(node) {
    if (!node) {
      return;
    }

    if (node.nodeType !== Node.ELEMENT_NODE) {
      // This node is not an element, do not add it.
      return;
    }

    if (this.#rootNodes.has(node)) {
      // Exclude nodes that are already targetted.
      return;
    }

    this.#rootNodes.add(node);

    this.subdivideNodeForTranslations(node);

    this.observer.observe(node, {
      characterData: true,
      childList: true,
      subtree: true,
    });
  }

  /**
   * Add qualified nodes to queueNodeForTranslation by recursively walk
   * through the DOM tree of node, including elements in Shadow DOM.
   *
   * @param {Element} [node]
   */
  processSubdivide(node) {
    const nodeIterator = node.ownerDocument.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT,
      this.determineTranslationStatusForUnprocessedNodes
    );

    // This iterator will contain each node that has been subdivided enough to
    // be translated.
    let currentNode;
    while ((currentNode = nodeIterator.nextNode())) {
      const shadowRoot = currentNode.openOrClosedShadowRoot;
      if (shadowRoot) {
        this.processSubdivide(shadowRoot);
      } else {
        this.queueNodeForTranslation(currentNode);
      }
    }
  }

  /**
   * Start walking down through a node's subtree and decide which nodes to queue for
   * translation. This first node could be the root nodes of the DOM, such as the
   * document body, or the title element, or it could be a mutation target.
   *
   * The nodes go through a process of subdivision until an appropriate sized chunk
   * of inline text can be found.
   *
   * @param {Node} node
   */
  subdivideNodeForTranslations(node) {
    if (!this.#rootNodes.has(node)) {
      // This is a non-root node, which means it came from a mutation observer.
      // Ensure that it is a valid node to translate by checking all of its ancestors.
      for (let parent of getAncestorsIterator(node)) {
        if (
          this.determineTranslationStatus(parent) ===
          NodeStatus.NOT_TRANSLATABLE
        ) {
          return;
        }
      }
    }

    switch (this.determineTranslationStatusForUnprocessedNodes(node)) {
      case NodeStatus.NOT_TRANSLATABLE:
        // This node is rejected as it shouldn't be translated.
        return;

      case NodeStatus.READY_TO_TRANSLATE:
        // This node is ready for translating, and doesn't need to be subdivided. There
        // is no reason to run the TreeWalker, it can be directly submitted for
        // translation.
        this.queueNodeForTranslation(node);
        break;

      case NodeStatus.SUBDIVIDE_FURTHER:
        // This node may be translatable, but it needs to be subdivided into smaller
        // pieces. Create a TreeWalker to walk the subtree, and find the subtrees/nodes
        // that contain enough inline elements to send to be translated.
        this.processSubdivide(node);
        break;
    }

    if (node.nodeName === "BODY") {
      this.reportWordsInViewport();
    }
    this.dispatchQueuedTranslations();
  }

  /**
   * Test whether this is an element we do not want to translate. These are things like
   * <code> elements, elements with a different "lang" attribute, and elements that
   * have a `translate=no` attribute.
   *
   * @param {Node} node
   */
  isExcludedNode(node) {
    // Property access be expensive, so destructure required properties so they are
    // not accessed multiple times.
    const { nodeType } = node;

    if (nodeType === Node.TEXT_NODE) {
      // Text nodes are never excluded.
      return false;
    }
    if (nodeType !== Node.ELEMENT_NODE) {
      // Only elements and and text nodes should be considered.
      return true;
    }

    const { nodeName } = node;

    if (EXCLUDED_TAGS.has(nodeName)) {
      // This is an excluded tag.
      return true;
    }

    if (!this.matchesDocumentLanguage(node)) {
      // Exclude nodes that don't match the fromLanguage.
      return true;
    }

    if (node.getAttribute("translate") === "no") {
      // This element has a translate="no" attribute.
      // https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/translate
      return true;
    }

    if (node.classList.contains("notranslate")) {
      // Google Translate skips translations if the classList contains "notranslate"
      // https://cloud.google.com/translate/troubleshooting
      return true;
    }

    if (node.isContentEditable) {
      // This field is editable, and so exclude it similar to the way that form input
      // fields are excluded.
      return true;
    }

    return false;
  }

  /**
   * Runs `determineTranslationStatus`, but only on unprocessed nodes.
   *
   * @param {Node} node
   * @return {number} - One of the NodeStatus values.
   */
  determineTranslationStatusForUnprocessedNodes = node => {
    if (this.#processedNodes.has(node)) {
      // Skip nodes that have already been processed.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    return this.determineTranslationStatus(node);
  };

  /**
   * Determines if a node should be submitted for translation, not translatable, or if
   * it should be subdivided further. It doesn't check if the node has already been
   * processed.
   *
   * The return result works as a TreeWalker NodeFilter as well.
   *
   * @param {Node} node
   * @returns {number} - One of the `NodeStatus` values. See that object
   *   for documentation. These values match the filters for the TreeWalker.
   *   These values also work as a `NodeFilter` value.
   */
  determineTranslationStatus(node) {
    if (node.openOrClosedShadowRoot) {
      return NodeStatus.SHADOW_HOST;
    }

    if (isNodeQueued(node, this.#queuedNodes)) {
      // This node or its parent was already queued, reject it.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (this.isExcludedNode(node)) {
      // This is an explicitly excluded node.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (node.textContent.trim().length === 0) {
      // Do not use subtrees that are empty of text. This textContent call is fairly
      // expensive.
      return !node.hasChildNodes()
        ? NodeStatus.NOT_TRANSLATABLE
        : NodeStatus.SUBDIVIDE_FURTHER;
    }

    if (nodeNeedsSubdividing(node)) {
      // Skip this node, and dig deeper into its tree to cut off smaller pieces
      // to translate. It is presumed to be a wrapper of block elements.
      return NodeStatus.SUBDIVIDE_FURTHER;
    }

    if (
      containsExcludedNode(node, this.excludedNodeSelector) &&
      !hasTextNodes(node)
    ) {
      // Skip this node, and dig deeper into its tree to cut off smaller pieces
      // to translate.
      return NodeStatus.SUBDIVIDE_FURTHER;
    }

    // This node can be treated as entire block to submit for translation.
    return NodeStatus.READY_TO_TRANSLATE;
  }

  /**
   * Queue a node for translation.
   * @param {Node} node
   */
  queueNodeForTranslation(node) {
    /** @type {NodeVisibility} */
    let visibility = "out-of-viewport";
    if (isNodeHidden(node)) {
      visibility = "hidden";
    } else if (isNodeInViewport(node)) {
      visibility = "in-viewport";
    }

    this.#queuedNodes.set(node, visibility);
  }

  /**
   * Submit the translations giving priority to nodes in the viewport.
   */
  async dispatchQueuedTranslations() {
    let inViewportCounts = 0;
    let outOfViewportCounts = 0;
    let hiddenCounts = 0;

    let inViewportTranslations;
    if (!this.viewportTranslated) {
      inViewportTranslations = [];
    }

    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "in-viewport") {
        inViewportCounts++;
        const promise = this.submitTranslation(node);
        if (inViewportTranslations) {
          inViewportTranslations.push(promise);
        }
      }
    }
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "out-of-viewport") {
        outOfViewportCounts++;
        this.submitTranslation(node);
      }
    }
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "hidden") {
        hiddenCounts++;
        this.submitTranslation(node);
      }
    }

    ChromeUtils.addProfilerMarker(
      "Translations",
      { innerWindowId: this.innerWindowId },
      `Translate ${this.#queuedNodes.size} nodes.\n\n` +
        `In viewport: ${inViewportCounts}\n` +
        `Out of viewport: ${outOfViewportCounts}\n` +
        `Hidden: ${hiddenCounts}\n`
    );

    this.#queuedNodes.clear();

    if (!this.viewportTranslated && inViewportTranslations) {
      // Provide a promise that can be used to determine when the initial viewport has
      // been translated. This is a key user-visible metric.
      this.viewportTranslated = Promise.allSettled(inViewportTranslations);
    }
  }

  /**
   * Record how many words were in the viewport, as this is the most important
   * user-visible translation content.
   */
  reportWordsInViewport() {
    if (
      // This promise gets created for the first dispatchQueuedTranslations
      this.viewportTranslated ||
      this.#queuedNodes.size === 0
    ) {
      return;
    }

    // TODO(Bug 1814195) - Add telemetry.
    // TODO(Bug 1820618) - This whitespace regex will not work in CJK-like languages.
    // This requires a segmenter for a proper implementation.

    const whitespace = /\s+/;
    let wordCount = 0;
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "in-viewport") {
        wordCount += node.textContent.trim().split(whitespace).length;
      }
    }

    const message = wordCount + " words are in the viewport.";
    lazy.console.log(message);
    ChromeUtils.addProfilerMarker(
      "Translations",
      { innerWindowId: this.innerWindowId },
      message
    );
  }

  /**
   * Submit a node for translation to the translations engine.
   *
   * @param {Node} node
   * @returns {Promise<void>}
   */
  async submitTranslation(node) {
    // Give each element an id that gets passed through the translation so it can be
    // reunited later on.
    if (node.nodeType === Node.ELEMENT_NODE) {
      node.querySelectorAll("*").forEach((el, i) => {
        el.dataset.mozTranslationsId = i;
      });
    }

    let text, translate;
    if (node.nodeType === Node.ELEMENT_NODE) {
      text = node.innerHTML;
      translate = this.translateHTML;
    } else {
      text = node.textContent;
      translate = this.translateText;
    }

    if (text.trim().length === 0) {
      return;
    }

    // Mark this node as not to be translated again unless the contents are changed
    // (which the observer will pick up on)
    this.#processedNodes.add(node);

    this.#pendingTranslationsCount++;
    try {
      const [translatedHTML] = await translate(text);
      this.#pendingTranslationsCount--;
      this.scheduleNodeUpdateWithTranslation(node, translatedHTML);
    } catch (error) {
      this.#pendingTranslationsCount--;
      lazy.console.error("Translation failed", error);
    }
  }

  /**
   * Start the mutation observer, for instance after applying the translations to the DOM.
   */
  startMutationObserver() {
    if (Cu.isDeadWrapper(this.observer)) {
      // This observer is no longer alive.
      return;
    }
    for (const node of this.#rootNodes) {
      if (Cu.isDeadWrapper(node)) {
        // This node is no longer alive.
        continue;
      }
      this.observer.observe(node, {
        characterData: true,
        childList: true,
        subtree: true,
      });
    }
  }

  /**
   * Stop the mutation observer, for instance to apply the translations to the DOM.
   */
  stopMutationObserver() {
    // Was the window already destroyed?
    if (!Cu.isDeadWrapper(this.observer)) {
      this.observer.disconnect();
    }
  }

  /**
   * This is called every `DOM_UPDATE_INTERVAL_MS` ms with translations for nodes.
   *
   * This function is called asynchronously, so nodes may already be dead. Before
   * accessing a node make sure and run `Cu.isDeadWrapper` to check that it is alive.
   */
  updateNodesWithTranslations() {
    // Stop the mutations so that the updates won't trigger observations.
    this.stopMutationObserver();

    for (const { node, translatedHTML } of this.#nodesWithTranslatedHTML) {
      if (Cu.isDeadWrapper(node)) {
        // The node is no longer alive.
        ChromeUtils.addProfilerMarker(
          "Translations",
          { innerWindowId: this.innerWindowId },
          "Node is no long alive."
        );
        continue;
      }
      switch (node.nodeType) {
        case Node.TEXT_NODE: {
          if (translatedHTML.trim().length !== 0) {
            // Only update the node if there is new text.
            node.textContent = translatedHTML;
          }
          break;
        }
        case Node.ELEMENT_NODE: {
          // TODO (Bug 1820625) - This is slow compared to the original implementation
          // in the addon which set the innerHTML directly. We can't set the innerHTML
          // here, but perhaps there is another way to get back some of the performance.
          const translationsDocument = this.domParser.parseFromString(
            `<!DOCTYPE html><div>${translatedHTML}</div>`,
            "text/html"
          );
          updateElement(translationsDocument, node);
          break;
        }
      }
    }

    this.#nodesWithTranslatedHTML.clear();
    this.#updateTimeout = null;

    // Done mutating the DOM.
    this.startMutationObserver();
  }

  /**
   * Schedule a node to be updated with a translation.
   *
   * @param {Node} node
   * @param {string} translatedHTML
   */
  scheduleNodeUpdateWithTranslation(node, translatedHTML) {
    // Add the nodes to be populated with the next translation update.
    this.#nodesWithTranslatedHTML.add({ node, translatedHTML });

    if (this.#pendingTranslationsCount === 0) {
      // No translations are pending, update the node.
      this.updateNodesWithTranslations();
    } else if (!this.#updateTimeout) {
      // Schedule an update.
      this.#updateTimeout = lazy.setTimeout(
        this.updateNodesWithTranslations.bind(this),
        DOM_UPDATE_INTERVAL_MS
      );
    } else {
      // An update has been previously scheduled, do nothing here.
    }
  }

  /**
   * Check to see if a language matches the document language.
   *
   * @param {Node} node
   */
  matchesDocumentLanguage(node) {
    if (!node.lang) {
      // No `lang` was present, so assume it matches the language.
      return true;
    }

    // First, cheaply check if language tags match, without canonicalizing.
    if (langTagsMatch(this.documentLanguage, node.lang)) {
      return true;
    }

    try {
      // Make sure the local is in the canonical form, and check again. This function
      // throws, so don't trust that the language tags are formatting correctly.
      const [language] = Intl.getCanonicalLocales(node.lang);

      return langTagsMatch(this.documentLanguage, language);
    } catch (_error) {
      return false;
    }
  }
}

/**
 * This function needs to be fairly fast since it's used on many nodes when iterating
 * over the DOM to find nodes to translate.
 *
 * @param {Text | HTMLElement} node
 */
function isNodeHidden(node) {
  /** @type {HTMLElement} */
  const element = node.nodeType === Node.TEXT_NODE ? node.parentElement : node;

  // This flushes the style, which is a performance cost.
  const style = element.ownerGlobal.getComputedStyle(element);
  return style.display === "none" || style.visibility === "hidden";
}

/**
 * This function cheaply checks that language tags match.
 *
 * @param {string} knownLanguage
 * @param {string} otherLanguage
 */
function langTagsMatch(knownLanguage, otherLanguage) {
  if (knownLanguage === otherLanguage) {
    // A simple direct match.
    return true;
  }
  if (knownLanguage.length !== 2) {
    throw new Error("Expected the knownLanguage to be of length 2.");
  }
  // Check if the language tags part match, e.g. "en" and "en-US".
  return (
    knownLanguage[0] === otherLanguage[0] &&
    knownLanguage[1] === otherLanguage[1] &&
    otherLanguage[2] === "-"
  );
}

/**
 * This function runs when walking the DOM, which means it is a hot function. It runs
 * fairly fast even though it is computing the bounding box. This is all done in a tight
 * loop, and it is done on mutations. Care should be taken with reflows caused by
 * getBoundingClientRect, as this is a common performance issue.
 *
 * The following are the counts of how often this is run on a news site:
 *
 * Given:
 *  1573 DOM nodes
 *  504 Text nodes
 *  1069 Elements
 *
 * There were:
 *  209 calls to get this funcion.
 *
 * @param {Node} node
 */
function isNodeInViewport(node) {
  const window = node.ownerGlobal;
  const document = node.ownerDocument;

  /** @type {HTMLElement} */
  const element = node.nodeType === Node.TEXT_NODE ? node.parentElement : node;

  const rect = element.getBoundingClientRect();
  return (
    rect.top >= 0 &&
    rect.left >= 0 &&
    rect.bottom <=
      (window.innerHeight || document.documentElement.clientHeight) &&
    rect.right <= (window.innerWidth || document.documentElement.clientWidth)
  );
}

/**
 * Actually perform the update of the element with the translated node. This step
 * will detach all of the "live" nodes, and match them up in the correct order as provided
 * by the translations engine.
 *
 * @param {Document} translationsDocument
 * @param {Element} element
 * @returns {void}
 */
function updateElement(translationsDocument, element) {
  // This text should have the same layout as the target, but it's not completely
  // guaranteed since the content page could change at any time, and the translation process is async.
  //
  // The document has the following structure:
  //
  // <html>
  //   <head>
  //   <body>{translated content}</body>
  // </html>

  const originalHTML = element.innerHTML;

  /**
   * The Set of translation IDs for nodes that have been cloned.
   * @type {Set<number>}
   */
  const clonedNodes = new Set();

  merge(element, translationsDocument.body.firstChild);

  /**
   * Merge the live tree with the translated tree by re-using elements from the live tree.
   *
   * @param {Node} liveTree
   * @param {Node} translatedTree
   */
  function merge(liveTree, translatedTree) {
    /** @type {Map<number, Element>} */
    const liveElementsById = new Map();

    /** @type {Array<Text>} */
    const liveTextNodes = [];

    // Remove all the nodes from the liveTree, and categorize them by Text node or
    // Element node.
    let node;
    while ((node = liveTree.firstChild)) {
      node.remove();

      if (node.nodeType === Node.ELEMENT_NODE) {
        liveElementsById.set(node.dataset.mozTranslationsId, node);
      } else if (node.nodeType === Node.TEXT_NODE) {
        liveTextNodes.push(node);
      }
    }

    // The translated tree dictates the order.
    const translatedNodes = translatedTree.childNodes;
    for (
      let translatedIndex = 0;
      translatedIndex < translatedNodes.length;
      translatedIndex++
    ) {
      const translatedNode = translatedNodes[translatedIndex];

      if (translatedNode.nodeType === Node.TEXT_NODE) {
        // Copy the translated text to the original Text node and re-append it.
        let liveTextNode = liveTextNodes.shift();

        if (liveTextNode) {
          liveTextNode.data = translatedNode.data;
        } else {
          liveTextNode = translatedNode;
        }

        liveTree.appendChild(liveTextNode);
      } else if (translatedNode.nodeType === Node.ELEMENT_NODE) {
        const translationsId = translatedNode.dataset.mozTranslationsId;
        // Element nodes try to use the already existing DOM nodes.

        // Find the element in the live tree that matches the one in the translated tree.
        let liveElement = liveElementsById.get(translationsId);

        if (!liveElement) {
          lazy.console.warn("Could not find a corresponding live element", {
            path: createNodePath(translatedNode, translationsDocument.body),
            translationsId,
            liveElementsById,
            translatedNode,
          });
          continue;
        }

        // Has this element already been added to the list? Then duplicate it and re-add
        // it as a clone. The Translations Engine can sometimes duplicate HTML.
        if (liveElement.parentNode) {
          liveElement = liveElement.cloneNode(true /* deep clone */);
          clonedNodes.add(translationsId);
          lazy.console.warn(
            "Cloning a node because it was already inserted earlier",
            {
              path: createNodePath(translatedNode, translationsDocument.body),
              translatedNode,
              liveElement,
            }
          );
        }

        if (isNodeTextEmpty(translatedNode)) {
          // The original node had text, but the one that came out of translation
          // didn't have any text. This scenario might be caused by one of two causes:
          //
          //   1) The element was duplicated by translation but then not given text
          //      content. This happens on Wikipedia articles for example.
          //
          //   2) The translator messed up and could not translate the text. This
          //      happens on YouTube in the language selector. In that case, having the
          //      original text is much better than no text at all.
          //
          // To make sure it is case 1 and not case 2 check whether this is the only occurrence.
          for (let i = 0; i < translatedNodes.length; i++) {
            if (translatedIndex === i) {
              // This is the current node, not a sibling.
              continue;
            }
            const sibling = translatedNodes[i];
            if (
              // Only consider other element nodes.
              sibling.nodeType === Node.ELEMENT_NODE &&
              // If the sibling's translationsId matches, then use the sibling's
              // node instead.
              translationsId === sibling.dataset.mozTranslationsId
            ) {
              // This is case 1 from above. Remove this element's original text nodes,
              // since a sibling text node now has all of the text nodes.
              removeTextNodes(liveElement);
            }
          }

          // Report this issue to the console.
          lazy.console.warn(
            "The translated element has no text even though the original did.",
            {
              path: createNodePath(translatedNode, translationsDocument.body),
              translatedNode,
              liveElement,
            }
          );
        } else if (!isNodeTextEmpty(liveElement)) {
          // There are still text nodes to find and update, recursively merge.
          merge(liveElement, translatedNode);
        }

        // Put the live node back in the live branch. But now t has been synced with the
        // translated text and order.
        liveTree.appendChild(liveElement);
      }
    }

    const unhandledElements = [...liveElementsById].filter(
      ([, element]) => !element.parentNode
    );

    if (unhandledElements.length) {
      lazy.console.warn(
        `${createNodePath(
          translatedTree,
          translationsDocument.body
        )} Not all nodes unified`,
        {
          unhandledElements,
          clonedNodes,
          originalHTML,
          translatedHTML: translationsDocument.body.innerHTML,
          liveTree: liveTree.outerHTML,
          translatedTree: translatedTree.outerHTML,
        }
      );
    }
  }
}

/**
 * For debug purposes, compute a string path to an element.
 *
 * e.g. "div/div#header/p.bold.string/a"
 *
 * @param {Node} node
 * @param {Node | null} root
 */
function createNodePath(node, root) {
  if (root === null) {
    root = node.ownerDocument.body;
  }
  let path =
    node.parentNode && node.parentNode !== root
      ? createNodePath(node.parentNode)
      : "";
  path += `/${node.nodeName}`;
  if (node.id) {
    path += `#${node.id}`;
  } else if (node.className) {
    for (const className of node.classList) {
      path += "." + className;
    }
  }
  return path;
}

/**
 * @param {Node} node
 * @returns {boolean}
 */
function isNodeTextEmpty(node) {
  if ("innerText" in node) {
    return node.innerText.trim().length === 0;
  }
  if (node.nodeType === Node.TEXT_NODE && node.nodeValue) {
    return node.nodeValue.trim().length === 0;
  }
  return true;
}

/**
 * @param {Node} node
 */
function removeTextNodes(node) {
  for (const child of node.childNodes) {
    switch (child.nodeType) {
      case Node.TEXT_NODE:
        node.removeChild(child);
        break;
      case Node.ELEMENT_NODE:
        removeTextNodes(child);
        break;
      default:
        break;
    }
  }
}

/**
 * Test whether any of the direct child text nodes of are non-whitespace
 * text nodes.
 *
 * For example:
 *   - `<p>test</p>`: yes
 *   - `<p> </p>`: no
 *   - `<p><b>test</b></p>`: no
 * @param {Node} node
 * @returns {boolean}
 */
function hasTextNodes(node) {
  if (node.nodeType !== Node.ELEMENT_NODE) {
    // Only check element nodes.
    return false;
  }

  for (const child of node.childNodes) {
    if (child.nodeType === Node.TEXT_NODE) {
      if (child.textContent.trim() === "") {
        // This is just whitespace.
        continue;
      }
      // A text node with content was found.
      return true;
    }
  }

  // No text nodes were found.
  return false;
}

/**
 * Like `isExcludedNode` but looks at the full subtree. Used to see whether
 * we can submit a subtree, or whether we should split it into smaller
 * branches first to try to exclude more of the non-translatable content.
 *
 * @param {Node} node
 * @param {string} excludedNodeSelector
 * @returns {boolean}
 */
function containsExcludedNode(node, excludedNodeSelector) {
  return (
    node.nodeType === Node.ELEMENT_NODE &&
    node.querySelector(excludedNodeSelector)
  );
}

/**
 * Check if this node has already been queued to be translated. This can be because
 * the node is itself is queued, or its parent node is queued.
 *
 * @param {Node} node
 * @param {Map<Node, any>} queuedNodes
 * @returns {boolean}
 */
function isNodeQueued(node, queuedNodes) {
  if (queuedNodes.has(node)) {
    return true;
  }

  // If the immediate parent is the body, it is allowed.
  if (node.parentNode === node.ownerDocument.body) {
    return false;
  }

  // Accessing the parentNode is expensive here according to performance profilling. This
  // is due to XrayWrappers. Minimize reading attributes by storing a reference to the
  // `parentNode` in a named variable, rather than re-accessing it.
  let parentNode;
  let lastNode = node;
  while ((parentNode = lastNode.parentNode)) {
    if (queuedNodes.has(parentNode)) {
      return parentNode;
    }
    lastNode = parentNode;
  }

  return false;
}

/**
 * Test whether this node should be treated as a wrapper of text, e.g.
 * a `<p>`, or as a wrapper for block elements, e.g. `<div>`, based on
 * its ratio of assumed inline elements, and assumed "block" elements. If it is a wrapper
 * of block elements, then it needs more subdividing. This algorithm is based on
 * heuristics and is a best effort attempt at sorting contents without actually computing
 * the style of every element.
 *
 * If it's a Text node, it's inline and doesn't need subdividing.
 *
 *  "Lorem ipsum"
 *
 * If it is mostly filled with assumed "inline" elements, treat it as inline.
 *   <p>
 *     Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 *     <b>Nullam ut finibus nibh</b>, at tincidunt tellus.
 *   </p>
 *
 *   Since it has 3 "inline" elements.
 *     1. "Lorem ipsum dolor sit amet, consectetur adipiscing elit."
 *     2. <b>Nullam ut finibus nibh</b>
 *     3. ", at tincidunt tellus."
 *
 * If it's mostly filled with block elements, do not treat it as inline, as it will
 * need more subdividing.
 *
 *   <section>
 *     Lorem ipsum <strong>dolor sit amet.</strong>
 *     <div>Nullam ut finibus nibh, at tincidunt tellus.</div>
 *     <div>Morbi pharetra mauris sed nisl mollis molestie.</div>
 *     <div>Donec et nibh sit amet velit tincidunt auctor.</div>
 *   </section>
 *
 *   This node has 2 presumed "inline" elements:
 *       1 "Lorem ipsum"
 *       2. <strong>dolor sit amet.</strong>.
 *
 *   And the 3 div "block" elements. Since 3 "block" elements > 2 "inline" elements,
 *   it is presumed to be "inline".
 *
 * @param {Node} node
 * @returns {boolean}
 */
function nodeNeedsSubdividing(node) {
  if (node.nodeType === Node.TEXT_NODE) {
    // Text nodes are fully subdivided.
    return false;
  }

  let inlineElements = 0;
  let blockElements = 0;

  if (node.nodeName === "TR") {
    // TR elements always need subdividing, since the cells are the individual "inline"
    // units. For instance the following would be invalid markup:
    //
    //   <tr>
    //     This is <b>invalid</b>
    //   </tr>
    //
    // You will always have the following, which will need more subdividing.
    //
    //   <tr>
    //     <td>This is <b>valid</b>.</td>
    //     <td>This is still valid.</td>
    //   </tr>
    return true;
  }

  for (let child of node.childNodes) {
    switch (child.nodeType) {
      case Node.TEXT_NODE:
        if (!isNodeTextEmpty(child)) {
          inlineElements += 1;
        }
        break;
      case Node.ELEMENT_NODE: {
        // Property access can be expensive, so destructure the required properties.
        const { nodeName } = child;
        if (INLINE_TAGS.has(nodeName)) {
          inlineElements += 1;
        } else if (GENERIC_TAGS.has(nodeName) && !nodeNeedsSubdividing(child)) {
          inlineElements += 1;
        } else {
          blockElements += 1;
        }
        break;
      }
      default:
        break;
    }
  }

  return inlineElements < blockElements;
}

/**
 * Returns an iterator of a node's ancestors.
 *
 * @param {Node} node
 * @returns {Generator<ParentNode>}
 */
function* getAncestorsIterator(node) {
  const document = node.ownerDocument;
  for (
    let parent = node.parentNode;
    parent && parent !== document.documentElement;
    parent = parent.parentNode
  ) {
    yield parent;
  }
}
