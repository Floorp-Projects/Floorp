"use strict";

/**
 * A wrapper for the findbar's method "close", which is not synchronous
 * because of animation.
 */
function closeFindbarAndWait(findbar) {
  return new Promise((resolve) => {
    if (findbar.hidden)
      return resolve();
    findbar.addEventListener("transitionend", function cont(aEvent) {
      if (aEvent.propertyName != "visibility") {
        return;
      }
      findbar.removeEventListener("transitionend", cont);
      resolve();
    });
    findbar.close();
  });
}
