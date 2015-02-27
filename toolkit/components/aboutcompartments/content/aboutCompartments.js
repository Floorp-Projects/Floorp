/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});

function go() {
    let compartmentInfo = Cc["@mozilla.org/compartment-info;1"]
            .getService(Ci.nsICompartmentInfo);
    let compartments = compartmentInfo.getCompartments();
    let count = compartments.length;
    let addons = {};
    for (let i = 0; i < count; i++) {
        let compartment = compartments.queryElementAt(i, Ci.nsICompartment);
        if (addons[compartment.addonId]) {
            addons[compartment.addonId].time += compartment.time;
            addons[compartment.addonId].CPOWTime += compartment.CPOWTime;
            addons[compartment.addonId].compartments.push(compartment);
        } else {
            addons[compartment.addonId] = {
                time: compartment.time,
                CPOWTime: compartment.CPOWTime,
                compartments: [compartment]
            };
        }
    }
    let dataDiv = document.getElementById("data");
    for (let addon in addons) {
        let el = document.createElement("tr");
        let name = document.createElement("td");
        let time = document.createElement("td");
        let cpow = document.createElement("td");
        name.className = "addon";
        time.className = "time";
        cpow.className = "cpow";
        name.textContent = addon;
        AddonManager.getAddonByID(addon, function(a) {
            if (a) {
                name.textContent = a.name;
            }
        });
        time.textContent = addons[addon].time +"μs";
        cpow.textContent = addons[addon].CPOWTime +"μs";
        el.appendChild(time);
        el.appendChild(cpow);
        el.appendChild(name);
        let div = document.createElement("tr");
        for (let comp of addons[addon].compartments) {
            let c = document.createElement("tr");
            let name = document.createElement("td");
            let time = document.createElement("td");
            let cpow = document.createElement("td");
            name.className = "addon";
            time.className = "time";
            cpow.className = "cpow";
            name.textContent = comp.compartmentName;
            time.textContent = comp.time +"μs";
            cpow.textContent = comp.CPOWTime +"μs";
            c.appendChild(time);
            c.appendChild(cpow);
            c.appendChild(name);
            div.appendChild(c);
            div.className = "details";
        }
        el.addEventListener("click", function() { div.style.display = (div.style.display != "block" ? "block" : "none"); });
        el.appendChild(div);
        dataDiv.appendChild(el);
    }
}
