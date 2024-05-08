/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class FormHandlerParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "FormHandler:EnsureParentExists": {
        // This dummy message is sent to make sure that the parent exists,
        // because we use the existence of the parent to determine whether to
        // notify the corresponding child when a page navigation occurs.
        break;
      }
      case "FormHandler:NotifyNavigatedSubtree": {
        this.onPageNavigated(message.data);
        break;
      }
      case "FormHandler:RegisterProgressListenerAtTopLevel": {
        this.registerProgressListenerAtTopLevel();
        break;
      }
    }
  }

  /**
   * Go through the subtree of the navigated browsing context and
   * let the existing FormHandler parents (same-origin and cross-origin)
   * notify their corresponding children of the detected page navigation
   * If the current browsing context is the navigated one, skip it,
   * because the page navigation was processed directly in the child.
   *
   * @param {BrowsingContext} navigatedBrowsingContext
   */
  onPageNavigated(navigatedBrowsingContext) {
    const browsingContexts =
      navigatedBrowsingContext.getAllBrowsingContextsInSubtree();

    if (this.browsingContext === navigatedBrowsingContext) {
      // Don't notify the child of the navigated process root,
      // since the page navigation was already processed in that child
      browsingContexts.shift();
    }

    for (const context of browsingContexts) {
      const windowGlobal = context.currentWindowGlobal;
      if (!windowGlobal) {
        continue;
      }

      // This next step doesn't create the FormHandler actor pair. We only
      // check whether the FormHandlerParent already exists.
      // If it exists, somebody in that window context registered an interest
      // in form submissions, so we send a message.
      // If it doesn't exist, then nobody in that window context is interested
      // in the form submissions, so we don't need to send a message.
      const formHandlerActor = windowGlobal.getExistingActor("FormHandler");
      formHandlerActor?.sendAsyncMessage(
        "FormHandler:FormSubmissionByNavigation"
      );
    }
  }

  /**
   * Send a dummy message to the FormHandlerChild of the top.
   * This is to make sure that the top child is being created and has
   * registered the progress listener that listens for page navigations.
   */
  registerProgressListenerAtTopLevel() {
    const topLevelFormHandler =
      this.browsingContext.top.currentWindowGlobal.getActor("FormHandler");
    topLevelFormHandler.sendAsyncMessage("FormHandler:EnsureChildExists");
  }
}
