/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.IBinder;
import android.view.KeyEvent;

import org.mozilla.gecko.IGeckoEditableChild;

// Interface for GeckoEditable calls from child to parent
interface IGeckoEditableParent {
    // Notify an IME event of a type defined in GeckoEditableListener.
    void notifyIME(IGeckoEditableChild child, int type);

    // Notify a change in editor state or type.
    void notifyIMEContext(int state, String typeHint, String modeHint, String actionHint,
                          int flags);

    // Notify a change in editor selection.
    void onSelectionChange(IBinder token, int start, int end);

    // Notify a change in editor text.
    void onTextChange(IBinder token, in CharSequence text,
                      int start, int unboundedOldEnd);

    // Perform the default action associated with a key event.
    void onDefaultKeyEvent(IBinder token, in KeyEvent event);

    // Update the screen location of current composition.
    void updateCompositionRects(IBinder token, in RectF[] rects);
}
