/****** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# David Hyatt.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   David Hyatt (hyatt@apple.com)
#   Blake Ross (blaker@netscape.com)
#   Joe Hewitt (hewitt@netscape.com)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****/


function createFolderOpenButton(uri, name)
{
    dump("In addFolderOpenButton\n");
    var paletteItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                        "toolbarbutton");

    paletteItem.id = "p-open-"+uri;
    paletteItem.setAttribute("class", "toolbarbutton-1");
    paletteItem.setAttribute("label", name);
    paletteItem.setAttribute("link", uri);
    paletteItem.setAttribute("tooltiptext", "Open "+name);
    paletteItem.setAttribute("oncommand", 'goMailbox("'+uri+'")');
    paletteItem.setAttribute("foldertype", "open-folder");

    return paletteItem;
}

function createFolderMovetoButton(uri, name)
{
    dump("In addFolderMovetoButton\n");
    var paletteItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                        "toolbarbutton");

    paletteItem.id = "p-moveto-"+uri;
    paletteItem.setAttribute("class", "toolbarbutton-1");
    paletteItem.setAttribute("label", name);
    paletteItem.setAttribute("link", uri);
    paletteItem.setAttribute("tooltiptext", "Move to "+name);
    paletteItem.setAttribute("oncommand", '"MsgMoveMessage("'+uri+'")');
    paletteItem.setAttribute("foldertype", "moveto-folder");

    return paletteItem;
}

function createRecipientButton(email, name, buttontype)
{
    dump("In addRecipientButton\n");
    var paletteItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                        "toolbarbutton");

    paletteItem.id = buttontype+email;
    paletteItem.setAttribute("class", "toolbarbutton-1");
    paletteItem.setAttribute("label", name);
    paletteItem.setAttribute("link", email);
    paletteItem.setAttribute("foldertype", buttontype);

    switch(buttontype)
    {
    case "p-recipient-":
        paletteItem.setAttribute("tooltiptext", "Create new mail to "+name);
        paletteItem.setAttribute("oncommand", 'MessageTo("'+email+'", false)');
        break;
    case "p-forward-":
        paletteItem.setAttribute("tooltiptext", "Forward mail to "+name);
        paletteItem.setAttribute("oncommand", 'MessageTo("'+email+'", true)');
        break;
    case "p-redirect-":
        paletteItem.setAttribute("tooltiptext", "Redirect mail to "+name);
        paletteItem.setAttribute("oncommand", 'alert("Not Implemented")');
        break;
    case "p-insert-":
        paletteItem.setAttribute("tooltiptext", "Insert "+name+" into current text field");
        paletteItem.setAttribute("oncommand", 'alert("Not Implemented")');
        break;
    default:
        return null;
    }

    return paletteItem;
}

function MessageTo(email, isForward)
{
    var folder = GetLoadedMsgFolder();
    var messageArray = GetSelectedMessages();
    
    var msgComposeType = Components.interfaces.nsIMsgCompType;
    var msgComposeFormat = Components.interfaces.nsIMsgCompFormat;
    var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
    msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

    var params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
    if (params)
    {
        var composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
        params.format = msgComposeFormat.Default;
        if (composeFields)
        {
            composeFields.to = email;
            params.composeFields = composeFields;

            if (isForward)
            {
                params.type = msgComposeType.ForwardAsAttachment;
                var uri = null;
                if (messageArray && messageArray.length > 0)
                {
                    var server;
                    params.originalMsgURI = "";
                    for (var i = 0; i < messageArray.length; ++i)
                    {
                        var messageUri = messageArray[i];

                        var hdr = messenger.msgHdrFromURI(messageUri);
                        // If we treat reply from sent specially, do we check for that folder flag here ?
                        var hintForIdentity = hdr.recipients + hdr.ccList;
                        var customIdentity = null;
                        if (folder)
                        {
                          server = folder.server;
                          customIdentity = folder.customIdentity;
                        }
                        
                        var accountKey = hdr.accountKey;
                        if (accountKey.length > 0)
                        {
                            var account = accountManager.getAccount(accountKey);
                            if (account)
                            {
                                server = account.incomingServer;
                            }
                        }

                        identity = (server && !customIdentity) 
                          ? getIdentityForServer(server, hintForIdentity)
                          : customIdentity;                         

                        var messageID = hdr.messageId;
                        if (i)
                        {
                            params.originalMsgURI += ","
                        }
                        params.originalMsgURI += messageUri;
                    }
                }
            }
            else
            {
                params.type = msgComposeType.New;
            }
            msgComposeService.OpenComposeWindowWithParams(null, params);
        }
    }
}

