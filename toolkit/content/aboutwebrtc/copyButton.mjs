/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This creates a button that can be used to copy text to the clipboard.
 * Whenever the button is pressed the getCopyContentsFn passed into the
 * constructor is called and the resulting text is copied. It uses CSS
 * transitions to perform a short animation.
 */
class CopyButton {
  constructor(getCopyContentsFn) {
    const button = document.createElement("span");
    button.textContent = String.fromCodePoint(0x1f4cb);
    button.classList.add("copy-button", "copy-button-base");
    button.onclick = () => {
      if (!button.classList.contains("copy-button")) {
        return;
      }

      const handleAnimation = async () => {
        const switchFadeDirection = () => {
          if (button.classList.contains("copy-button-fade-out")) {
            // We just faded out so let's fade in
            button.classList.toggle("copy-button-fade-out");
            button.classList.toggle("copy-button-fade-in");
          } else {
            // We just faded in so let's fade out
            button.classList.toggle("copy-button-fade-out");
            button.classList.toggle("copy-button-fade-in");
          }
        };

        // Fade out clipboard icon
        // Fade out the clipboard character
        button.classList.toggle("copy-button-fade-out");
        // Wait for CSS transition to end
        await new Promise(r => (button.ontransitionend = r));

        // Fade in checkmark icon
        // This is the start of fade in.
        // Switch to the checkmark character
        button.textContent = String.fromCodePoint(0x2705);
        // Trigger CSS fade in transition
        switchFadeDirection();
        // Wait for CSS transition to end
        await new Promise(r => (button.ontransitionend = r));

        // Fade out clipboard icon
        // Trigger CSS fade out transition
        switchFadeDirection();
        // Wait for CSS transition to end
        await new Promise(r => (button.ontransitionend = r));

        // Fade in clipboard icon
        // This is the start of fade in.
        // Switch to the clipboard character
        button.textContent = String.fromCodePoint(0x1f4cb);
        // Trigger CSS fade in transition
        switchFadeDirection();
        // Wait for CSS transition to end
        await new Promise(r => (button.ontransitionend = r));

        // Remove fade
        button.classList.toggle("copy-button-fade-in");
        // Re-enable clicks and hidding when parent div has lost :hover
        button.classList.add("copy-button");
      };

      // Note the fade effect is handled in the CSS, we just need to swap
      // between the different CSS classes. This returns a promise that waits
      // for the current fade to end, starts the next fade, then resolves.

      navigator.clipboard.writeText(getCopyContentsFn());
      // Prevent animation from disappearing when parent div losses :hover,
      // and prevent additional clicks until the animation finishes.
      button.classList.remove("copy-button");
      handleAnimation(); // runs unawaited
    };
    this.element = button;
  }
}
export { CopyButton };
