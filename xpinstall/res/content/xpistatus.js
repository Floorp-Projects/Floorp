/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gManager;
var gBundle;
var gCanClose = false;
var gCancelled = false;

// implements nsIXPIProgressDialog
var progressHooks =
{
    onStateChange: function( aIndex, aState, aValue )
    {
        const state = Components.interfaces.nsIXPIProgressDialog;
        var status = document.getElementById("status"+aIndex);
        var progress = document.getElementById("progress"+aIndex);

        switch( aState ) {
        case state.DOWNLOAD_START:
            status.setAttribute("value",
                        gBundle.getString("progress.downloading"));
            progress.setAttribute("value","0%");
            break;

        case state.DOWNLOAD_DONE:
            status.setAttribute("value",
                        gBundle.getString("progress.downloaded"));
            progress.setAttribute("value","100%");
            break;

        case state.INSTALL_START:
            status.setAttribute("value",
                        gBundle.getString("progress.installing"));
            progress.setAttribute("mode","undetermined");
            break;

        case state.INSTALL_DONE:
            progress.setAttribute("mode","determined");
            progress.hidden = true;
            var msg;
            try
            {
                msg = gBundle.getString("error"+aValue);
            }
            catch (e)
            {
                msg = gBundle.stringBundle.formatStringFromName(
                        "unknown.error", [aValue], 1 );
            }
            status.setAttribute("value",msg);
            break;

        case state.DIALOG_CLOSE:
            // nsXPInstallManager is done with us, but we'll let users
            // dismiss the dialog themselves so they can see the status
            // (unless we're closing because the user cancelled)
            document.documentElement.getButton("accept").disabled = false;
            document.documentElement.getButton("cancel").disabled = true;
            document.documentElement.getButton("accept").focus();
            gCanClose = true;

            if (gCancelled)
                window.close();

            break;
        }
    },

    onProgress: function( aIndex, aValue, aMaxValue )
    {
        var percent = Math.round( 100 * (aValue/aMaxValue) );
        var node = document.getElementById("progress"+aIndex);
        node.setAttribute("value", percent);
    },

    QueryInterface: function( iid )
    {
        if (iid.equals(Components.interfaces.nsISupports) ||
            iid.equals(Components.interfaces.nsIXPIProgressDialog))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    }
}


function onLoad()
{
    document.documentElement.getButton("accept").disabled = true;
    document.documentElement.getButton("cancel").focus();
    gBundle = document.getElementById("xpinstallBundle");

    var param = window.arguments[0].QueryInterface(
                    Components.interfaces.nsIDialogParamBlock );
    if ( !param )
        dump (" error getting param block interface \n");

    var i = 0;
    var row = 0;
    var numElements = param.GetInt(1);
    while ( i < numElements )
    {
        var moduleName = param.GetString(i++);
        var URL = param.GetString(i++);
        var IconURL = param.GetString(i++); // Advance the enumeration, parameter is unused just now.
        var certName = param.GetString(i++);
        addTreeItem(row++, moduleName, URL);
    }

    gManager = window.arguments[1];

    // inform nsXPInstallManager we're open for business
    gManager.observe( progressHooks, "xpinstall-progress", "open" );
}

function addTreeItem(aRow, aName, aUrl)
{
    // first column is the package name
    var item = document.createElement("description");
    item.setAttribute("class", "packageName");
    item.setAttribute("id", "package"+aRow);
    item.setAttribute("value", aName);
    item.setAttribute("tooltiptext", aUrl);

    // second column is the status
    var status = document.createElement('description');
    status.setAttribute("class", "packageStatus");
    status.setAttribute("id", "status"+aRow);
    status.setAttribute("value", gBundle.getString("progress.queued"));

    // third row is a progress meter
    var progress = document.createElement("progressmeter");
    progress.setAttribute("class", "packageProgress");
    progress.setAttribute("id", "progress"+aRow);
    progress.setAttribute("value", "0%");

    // create row and add it to the grid
    var row  = document.createElement("row");
    row.appendChild(item);
    row.appendChild(status);
    row.appendChild(progress);
    document.getElementById("xpirows").appendChild(row);
}

function onCancel()
{
    gCancelled = true;
    if (gManager)
        gManager.observe( progressHooks, "xpinstall-progress", "cancel");

    // window is closed by native impl after cleanup
    return gCanClose;
}
