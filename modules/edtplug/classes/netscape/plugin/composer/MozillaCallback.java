/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.plugin.composer;

import java.io.*;


/** Allows Java to call back into the Mozilla thread.
 * This is a general purpose utility, not just a
 * composer-specific utility. It should be in some standard place.
 * To use, Create the callback, then enqueue it.
 */

abstract class MozillaCallback {
    public MozillaCallback(int mozenv){
        this.mozenv = mozenv;
    }
    /** Equeues the callback method.
     */
    public void enqueue(){
        pEnqueue(mozenv);
    }

    /** Override to perform an action in the Mozilla thread.
     */

    abstract protected void perform();

    private native void pEnqueue(int mozenv);
    private int mozenv;
}
