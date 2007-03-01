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
 * The Original Code is the Mozilla Penelope project.
 *
 * The Initial Developer of the Original Code is
 * QUALCOMM incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Mark Charlebois <mcharleb@qualcomm.com> original author
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

var PenelopeToolbar = { 
    onLoad: function() 
    { 
        dump("in PenelopeToolbar.onLoad\n");
        var toolbox = document.getElementsByTagName("toolbox");
        var templateNode = toolbox[0].palette.firstChild;

        // restore the currentset that may have the moveto uris
        var toolbar = document.getElementById("mail-bar2");
        var currentset = toolbar.getAttribute("penelope_currentset");

        // Create any missing palette items
        var items = currentset.split(",");
        for (var i=0; i<items.length; ++i)
        {
            if (items[i].substring(0,9) == "p-moveto-")
            {
                var uri = items[i].substring(9);
                var set = items[i].split("/");
                var name = set[set.length-1];
                var newbutton = createFolderMovetoButton(uri, name);
                toolbox[0].palette.appendChild(newbutton);
            }
            else if (items[i].substring(0,7) == "p-open-")
            {
                var uri = items[i].substring(7);
                var set = items[i].split("/");
                var name = set[set.length-1];
                var newbutton = createFolderOpenButton(uri, name);
                toolbox[0].palette.appendChild(newbutton);
            }
            else if ((items[i].substring(0,12) == "p-recipient-") ||
                    (items[i].substring(0,10) == "p-forward-") ||
                    (items[i].substring(0,11) == "p-redirect-") ||
                    (items[i].substring(0,9)  == "p-insert-"))
            {
                var parts = items[i].split("-")
                var buttontype = parts.slice(0,2).join("-")+"-";
                var email = parts.slice(2).join("-");
                var name = email;
                var newbutton = createRecipientButton(email, name, buttontype);
                toolbox[0].palette.appendChild(newbutton);
            }
        }

        if (currentset)
        {
            toolbar.currentSet = currentset;
            dump("resetting currentset: "+currentset+"\n");
        }

    },

    unLoad: function() 
    { 
        dump("in PenelopeToolbar.unLoad\n");

        // save the currentset that may have the moveto uris
        var toolbar = document.getElementById("mail-bar2");
        var currentset = toolbar.getAttribute("currentset");
        dump("currentset: "+currentset+"\n");
        toolbar.setAttribute("penelope_currentset", currentset);
        document.persist(toolbar.id, "penelope_currentset");
    }
}; 

//load event handler
window.addEventListener("load", function(e) { PenelopeToolbar.onLoad(e); }, false); 
window.addEventListener("unload", function(e) { PenelopeToolbar.unLoad(e); }, false); 

function dumpNodes(nodes)
{
    for (var i = 0; i < nodes.length; i++)
    {
        var attrs = nodes[i].attributes;
        var text = ""; 
        if (attrs)
        {
            for(var j=attrs.length-1; j>=0; j--) 
            {
                text += attrs[j].name + "=" + "'"+attrs[j].value+"' ";
            }
        }
        dump("<"+nodes[i].nodeName+" "+text+">\n");

        if (nodes[i].nodeValue)
        {
            dump(nodes[i].nodeValue);

        }
        if (nodes[i].hasChildNodes())
        {
            dumpNodes(nodes[i].childNodes);
        }
        dump("</"+nodes[i].nodeName+">\n");
    }
    dump("\n");
}

