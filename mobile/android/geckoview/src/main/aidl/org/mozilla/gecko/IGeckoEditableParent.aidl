/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.IBinder;
import android.view.KeyEvent;

import org.mozilla.gecko.IGeckoEditableChild;

// Interface for GeckoEditable calls from child to parent
interface IGeckoEditableParent {
    // Set the default child to forward events to, when there is no focused child.
    void setDefaultChild(IGeckoEditableChild child);

    // Notify an IME event of a type defined in GeckoEditableListener.
    void notifyIME(IGeckoEditableChild child, int type);

    // Notify a change in editor state or type.
    void notifyIMEContext(IBinder token, int state, String typeHint, String modeHint,
                          String actionHint, String autocapitalize, int flags);

    // Notify a change in editor selection.
    void onSelectionChange(IBinder token, int start, int end, boolean causedOnlyByComposition);

    // Notify a change in editor text.
    void onTextChange(IBinder token, in CharSequence text,
                      int start, int unboundedOldEnd);

    // Perform the default action associated with a key event.
    void onDefaultKeyEvent(IBinder token, in KeyEvent event);

    // Update the screen location of current composition.
    void updateCompositionRects(IBinder token, in RectF[] rects);
}
