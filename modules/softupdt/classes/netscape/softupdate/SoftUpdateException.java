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
/*
 * SoftUpdateException.java
 *
 * 1/7/97 - atotic created the class
 */

 package netscape.softupdate;

 /*
  * FolderSpec exception class
  */

 class SoftUpdateException extends Exception {

    int err;    // error code

    /* Constructor
     * reason is the error code
     */
    SoftUpdateException(String message, int reason)
    {
        super(message);
        err	= reason;
        if ( (message != null) && ( message.length() > 0 ))
            System.out.println(message + " " + String.valueOf(err));
       // printStackTrace();
    }

    int GetError()
    {
        return err;
    }
}
