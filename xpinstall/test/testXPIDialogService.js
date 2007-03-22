/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla XPInstall.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Daniel Veditz <dveditz@netscape.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* implementation of a test XPInstall Dialog Service */

// -----------------------------------------------------------------------
// Test the XPInstall embedding API's by dropping this component into
// the Mozilla components directory and registering it.
//
// Do not export as part of a normal build since this will override the
// built-in Mozilla UI we want to use.
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------
const XPIDIALOGSERVICE_CONTRACTID =
    "@mozilla.org/embedui/xpinstall-dialog-service;1";

const XPIDIALOGSERVICE_CID =
    Components.ID("{9A5BEF68-3FDA-4926-9809-87A5A1CC8505}");

const XPI_TOPIC = "xpinstall-progress";
const OPEN      = "open";
const CANCEL    = "cancel";


// -----------------------------------------------------------------------
// XPInstall Dialog Service
// -----------------------------------------------------------------------

function testXPIDialogService() {}

testXPIDialogService.prototype =
{
    QueryInterface: function( iid )
    {
        if (iid.equals(Components.interfaces.nsIXPIDialogService) ||
            iid.equals(Components.interfaces.nsIXPIProgressDialog) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    confirmInstall: function( parent, packages, count )
    {
        // stash parent window for use later
        this.mParent = parent;

        // quick and dirty data display
        var str = "num packages: " + count/2 + "\n\n";
        for ( i = 0; i < count; ++i)
            str += packages[i++] + ' -- ' + packages[i] + '\n';

        str += "\nDo you want to install?";

        return parent.confirm(str);
    },

    openProgressDialog: function( packages, count, mgr )
    {
        this.dlg = this.mParent.open();
        mgr.observe( this, XPI_TOPIC, OPEN );
    },

    onStateChange: function( index, state, error )
    {
        dump("---XPIDlg--- State: "+index+', '+state+', '+error+'\n');
    },

    onProgress: function( index, value, max )
    {
        dump("---XPIDlg---     "+index+": "+value+' of '+max+'\n');
    }
};



// -----------------------------------------------------------------------
// XPInstall Dialog Service Module and Factory
// -----------------------------------------------------------------------

// --- module entry point ---
function NSGetModule(compMgr, fileSpec) { return XPIDlgSvcModule; }


// --- module ---
var XPIDlgSvcModule =
{
    registerSelf: function( compMgr, fileSpec, location, type )
    {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        compMgr.registerFactoryLocation(XPIDIALOGSERVICE_CID,
            'XPInstall Dialog Service test component',
            XPIDIALOGSERVICE_CONTRACTID, fileSpec,
            location, type);
    },

    unregisterSelf: function( compMgr, fileSpec, location )
    {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.unregisterFactoryLocation(XPIDIALOGSERVICE_CID, fileSpec);
    },

    getClassObject: function( compMgr, cid, iid )
    {
        if (!cid.equals(XPIDIALOGSERVICE_CID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return XPIDlgSvcFactory;
    },

    canUnload: function( compMgr ) { return true; }
};


// --- factory ---
var XPIDlgSvcFactory =
{
    createInstance: function( outer, iid )
    {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        if (!iid.equals(Components.interfaces.nsIXPIDialogService) &&
            !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

        return new testXPIDialogService();
    }
};
