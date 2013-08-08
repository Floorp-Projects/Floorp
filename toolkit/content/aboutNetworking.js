/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

const gDashboard = Cc['@mozilla.org/network/dashboard;1'].
  getService(Ci.nsIDashboard);
const gPrefs = Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefService).getBranch("network.");

const REFRESH_INTERVAL_MS = 3000;

function col(element) {
  let col = document.createElement('td');
  let content = document.createTextNode(element);
  col.appendChild(content);
  return col;
}

function displayHttp(data) {
  let cont = document.getElementById('http_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'http_content');

  for (let i = 0; i < data.host.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.host[i]));
    row.appendChild(col(data.port[i]));
    row.appendChild(col(data.spdy[i]));
    row.appendChild(col(data.ssl[i]));
    row.appendChild(col(data.active[i].rtt.length));
    row.appendChild(col(data.idle[i].rtt.length));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displaySockets(data) {
  let cont = document.getElementById('sockets_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'sockets_content');

  for (let i = 0; i < data.host.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.host[i]));
    row.appendChild(col(data.port[i]));
    row.appendChild(col(data.tcp[i]));
    row.appendChild(col(data.active[i]));
    row.appendChild(col(data.socksent[i]));
    row.appendChild(col(data.sockreceived[i]));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayDns(data) {
  let cont = document.getElementById('dns_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'dns_content');

  for (let i = 0; i < data.hostname.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.hostname[i]));
    row.appendChild(col(data.family[i]));
    let column = document.createElement('td');

    for (let j = 0; j< data.hostaddr[i].length; j++) {
      column.appendChild(document.createTextNode(data.hostaddr[i][j]));
      column.appendChild(document.createElement('br'));
    }

    row.appendChild(column);
    row.appendChild(col(data.expiration[i]));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayWebsockets(data) {
  let cont = document.getElementById('websockets_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'websockets_content');

  for (let i = 0; i < data.hostport.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.hostport[i]));
    row.appendChild(col(data.encrypted[i]));
    row.appendChild(col(data.msgsent[i]));
    row.appendChild(col(data.msgreceived[i]));
    row.appendChild(col(data.sentsize[i]));
    row.appendChild(col(data.receivedsize[i]));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function requestNetworkingData() {
  gDashboard.requestSockets(displaySockets);
  gDashboard.requestHttpConnections(displayHttp);
  gDashboard.requestWebsocketConnections(displayWebsockets);
  gDashboard.requestDNSInfo(displayDns);
}

function init() {
  gDashboard.enableLogging = true;
  if (gPrefs.getBoolPref("warnOnAboutNetworking")) {
    let div = document.getElementById("warning_message");
    div.classList.add("active");
    document.getElementById("confpref").addEventListener("click", confirm);
  }

  requestNetworkingData();

  document.getElementById("autorefcheck").addEventListener("click", function() {
    let refrButton = document.getElementById("refreshButton");
    if (this.checked) {
      this.interval = setInterval(requestNetworkingData, REFRESH_INTERVAL_MS);
      refrButton.disabled = "disabled";
    } else {
      clearInterval(this.interval);
      refrButton.disabled = null;
    }
  });

  let refr = document.getElementById("refreshButton");
  refr.addEventListener("click", requestNetworkingData);
  if (document.getElementById("autorefcheck").checked)
    refr.disabled = "disabled";

  // Event delegation on #menu element
  let menu = document.getElementById("menu");
  menu.addEventListener("click", function click(e) {
    if (e.target)
      show(e.target);
  });
}

function confirm () {
  let div = document.getElementById("warning_message");
  div.classList.remove("active");
  let warnBox = document.getElementById("warncheck");
  gPrefs.setBoolPref("warnOnAboutNetworking", warnBox.checked);
}

function show(button) {
  let current_tab = document.querySelector(".active");
  let content = document.getElementById(button.value);
  if (current_tab == content)
    return;
  current_tab.classList.remove("active");
  content.classList.add("active");

  let current_button = document.querySelector(".selected");
  current_button.classList.remove("selected");
  button.classList.add("selected");
}

window.addEventListener("DOMContentLoaded", function load() {
  window.removeEventListener("DOMContentLoaded", load);
  init();
});
