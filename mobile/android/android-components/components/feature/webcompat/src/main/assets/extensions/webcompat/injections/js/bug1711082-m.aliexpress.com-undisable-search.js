"use strict";

/**
 * Bug 1711082 - Cannot trigger search bar on AliExpress mobile page
 *
 * This patch ensures that the search input never has the [disabled]
 * attribute, so that users may tap/click on it to search.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1711082 for details.
 */

const SELECTOR = `[data-spm="header"] input[disabled]`;

function check(target) {
  if (target.nodeName === "INPUT" && target.matches(SELECTOR)) {
    target.removeAttribute("disabled");
    return true;
  }
  return false;
}

new MutationObserver(mutations => {
  for (const { addedNodes, target, attributeName } of mutations) {
    if (attributeName === "disabled") {
      check(target);
    } else {
      addedNodes?.forEach(node => {
        if (!check(node)) {
          node.querySelector?.(SELECTOR)?.removeAttribute("disabled");
        }
      });
    }
  }
}).observe(document, { attributes: true, childList: true, subtree: true });
