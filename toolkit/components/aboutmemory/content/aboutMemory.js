/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is about:memory
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

var gMemReporters = { };

function $(n) {
    return document.getElementById(n);
}

function makeTableCell(s, c) {
    var td = document.createElement("td");
    var text = document.createTextNode(s);
    td.appendChild(text);
    if (c)
        td.setAttribute("class", c);

    return td;
}

function makeAbbrNode(str, title) {
    var abbr = document.createElement("abbr");
    var text = document.createTextNode(str);
    abbr.appendChild(text);
    abbr.setAttribute("title", title);

    return abbr;
}

function makeTableRow() {
    var row = document.createElement("tr");

    for (var i = 0; i < arguments.length; ++i) {
        var arg = arguments[i];
        if (typeof(arg) == "string") {
            row.appendChild(makeTableCell(arg));
        } else if (arg.__proto__ == Array.prototype) {
            row.appendChild(makeAbbrNode(arg[0], arg[1]));
        } else {
            row.appendChild(arg);
        }
    }

    return row;
}

function setTextContent(node, s) {
    while (node.lastChild)
        node.removeChild(node.lastChild);

    node.appendChild(document.createTextNode(s));
}

function formatNumber(n) {
    var s = "";
    var neg = false;
    if (n < 0) {
        neg = true;
        n = -n;
    }

    do {
        var k = n % 1000;
        if (n > 999) {
            if (k > 99)
                s = k + s;
            else if (k > 9)
                s = "0" + k + s;
            else
                s = "00" + k + s;
        } else {
            s = k + s;
        }

        n = Math.floor(n / 1000);
        if (n > 0)
            s = "," + s;
    } while (n > 0);

    return s;
}

function updateMemoryStatus()
{
    // if we have the standard reporters for mapped/allocated, put
    // those at the top
    if ("malloc/mapped" in gMemReporters &&
        "malloc/allocated" in gMemReporters)
    {
        // committed is the total amount of memory that we've touched, that is that we have
        // some kind of backing store for
        setTextContent($("memMappedValue"),
                       formatNumber(gMemReporters["malloc/mapped"].memoryUsed));

        // allocated is the amount of committed memory that we're actively using (i.e., that isn't free)
        setTextContent($("memInUseValue"),
                       formatNumber(gMemReporters["malloc/allocated"].memoryUsed));
    } else {
        $("memOverview").style.display = "none";
    }

    var mo = $("memOtherRows");
    while (mo.lastChild)
        mo.removeChild(mo.lastChild);

    var otherCount = 0;
    for each (var rep in gMemReporters) {
        var row = makeTableRow([rep.path, rep.description],
                               makeTableCell(formatNumber(rep.memoryUsed), "memValue"));

        mo.appendChild(row);

        otherCount++;
    }

    if (otherCount == 0) {
        var row = makeTableRow("No other information available.");
        mo.appendChild(row);
    }
}

function doLoad()
{
    var mgr = Components
        .classes["@mozilla.org/memory-reporter-manager;1"]
        .getService(Components.interfaces.nsIMemoryReporterManager);

    var e = mgr.enumerateReporters();
    while (e.hasMoreElements()) {
        var mr = e.getNext().QueryInterface(Components.interfaces.nsIMemoryReporter);
        gMemReporters[mr.path] = mr;
    }

    updateMemoryStatus();
}

