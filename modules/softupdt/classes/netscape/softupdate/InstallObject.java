/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
package	netscape.softupdate	;

import netscape.softupdate.*;
import netscape.util.*;
import java.lang.*;

/*
 * InstallObject
 * abstract class for things that get installed
 * encapsulates a single action
 * The way your subclass is used:
 * Constructor should collect all the data needed to perform the action
 * Complete() is called to complete the action
 * Abort() is called when update is aborted
 * You are guaranteed to be called with Complete() or Abort() before finalize
 * If your Complete() throws, you'll also get called with an Abort()
 */

abstract class InstallObject {

    SoftwareUpdate softUpdate;

	InstallObject(SoftwareUpdate inSoftUpdate)
	{
	    softUpdate = (SoftwareUpdate) inSoftUpdate;
	}

    /* Override with your action */
	abstract protected void Complete() throws SoftUpdateException;

    /* Override with an explanatory string for the progress dialog */
    abstract public String toString();
	
	/* Override with your clean-up function */
	abstract protected void Abort();
}
