/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.view.KeyEvent;

// Interface for GeckoEditable calls from parent to child
interface IGeckoEditableChild {
    // Process a key event.
    void onKeyEvent(int action, int keyCode, int scanCode, int metaState,
                    int keyPressMetaState, long time, int domPrintableKeyValue,
                    int repeatCount, int flags, boolean isSynthesizedImeKey,
                    in KeyEvent event);

    // Request a callback to parent after performing any pending operations.
    void onImeSynchronize();

    // Replace part of current text.
    void onImeReplaceText(int start, int end, String text);

    // Store a composition range.
    void onImeAddCompositionRange(int start, int end, int rangeType, int rangeStyles,
                                  int rangeLineStyle, boolean rangeBoldLine,
                                  int rangeForeColor, int rangeBackColor, int rangeLineColor);

    // Change to a new composition using previously added ranges.
    void onImeUpdateComposition(int start, int end, int flags);

    // Request cursor updates from the child.
    void onImeRequestCursorUpdates(int requestMode);
}
