/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 */

var manager;
var param;
var rval;

function OnLoad() 
{
    dump("In OnLoad \n");
    param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
    dump("After ParmBlock QI \n");
    dump( window.arguments );
    if ( window.arguments.length > 1)
    {
        manager = window.arguments[1];
        dump("After creation of manager \n");
        dump("manager = " + manager + "\n");
        manager.dialogOpened(window);
        dump("After call to DialogOpened \n");
    }
}

function onCancel()
{
    if (manager)
        manager.cancelInstall();

    window.close();
}

function onUnload()
{
    if (manager)
        manager.dialogClosed();
}