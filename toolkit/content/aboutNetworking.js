/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

const gDashboard = Cc['@mozilla.org/network/dashboard;1'].
  getService(Ci.nsIDashboard);
const gPrefs = Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefService).getBranch("network.");
const gRequestNetworkingData = {
  "http": gDashboard.requestHttpConnections,
  "sockets": gDashboard.requestSockets,
  "dns": gDashboard.requestDNSInfo,
  "websockets": gDashboard.requestWebsocketConnections
};
const gDashboardCallbacks = {
  "http": displayHttp,
  "sockets": displaySockets,
  "dns": displayDns,
  "websockets": displayWebsockets
};

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

  for (let i = 0; i < data.connections.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.connections[i].host));
    row.appendChild(col(data.connections[i].port));
    row.appendChild(col(data.connections[i].spdy));
    row.appendChild(col(data.connections[i].ssl));
    row.appendChild(col(data.connections[i].active.length));
    row.appendChild(col(data.connections[i].idle.length));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displaySockets(data) {
  let cont = document.getElementById('sockets_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'sockets_content');

  for (let i = 0; i < data.sockets.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.sockets[i].host));
    row.appendChild(col(data.sockets[i].port));
    row.appendChild(col(data.sockets[i].tcp));
    row.appendChild(col(data.sockets[i].active));
    row.appendChild(col(data.sockets[i].sent));
    row.appendChild(col(data.sockets[i].received));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayDns(data) {
  let cont = document.getElementById('dns_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'dns_content');

  for (let i = 0; i < data.entries.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.entries[i].hostname));
    row.appendChild(col(data.entries[i].family));
    let column = document.createElement('td');

    for (let j = 0; j < data.entries[i].hostaddr.length; j++) {
      column.appendChild(document.createTextNode(data.entries[i].hostaddr[j]));
      column.appendChild(document.createElement('br'));
    }

    row.appendChild(column);
    row.appendChild(col(data.entries[i].expiration));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayWebsockets(data) {
  let cont = document.getElementById('websockets_content');
  let parent = cont.parentNode;
  let new_cont = document.createElement('tbody');
  new_cont.setAttribute('id', 'websockets_content');

  for (let i = 0; i < data.websockets.length; i++) {
    let row = document.createElement('tr');
    row.appendChild(col(data.websockets[i].hostport));
    row.appendChild(col(data.websockets[i].encrypted));
    row.appendChild(col(data.websockets[i].msgsent));
    row.appendChild(col(data.websockets[i].msgreceived));
    row.appendChild(col(data.websockets[i].sentsize));
    row.appendChild(col(data.websockets[i].receivedsize));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function requestAllNetworkingData() {
  for (let id in gRequestNetworkingData)
    requestNetworkingDataForTab(id);
}

function requestNetworkingDataForTab(id) {
  gRequestNetworkingData[id](gDashboardCallbacks[id]);
}

function init() {
  gDashboard.enableLogging = true;
  if (gPrefs.getBoolPref("warnOnAboutNetworking")) {
    let div = document.getElementById("warning_message");
    div.classList.add("active");
    div.hidden = false;
    document.getElementById("confpref").addEventListener("click", confirm);
  }

  requestAllNetworkingData();

  let autoRefresh = document.getElementById("autorefcheck");
  if (autoRefresh.checked)
    setAutoRefreshInterval(autoRefresh);

  autoRefresh.addEventListener("click", function() {
    let refrButton = document.getElementById("refreshButton");
    if (this.checked) {
      setAutoRefreshInterval(this);
      refrButton.disabled = "disabled";
    } else {
      clearInterval(this.interval);
      refrButton.disabled = null;
    }
  });

  let refr = document.getElementById("refreshButton");
  refr.addEventListener("click", requestAllNetworkingData);
  if (document.getElementById("autorefcheck").checked)
    refr.disabled = "disabled";

  // Event delegation on #categories element
  let menu = document.getElementById("categories");
  menu.addEventListener("click", function click(e) {
    if (e.target && e.target.parentNode == menu)
      show(e.target);
  });
}

function confirm () {
  let div = document.getElementById("warning_message");
  div.classList.remove("active");
  div.hidden = true;
  let warnBox = document.getElementById("warncheck");
  gPrefs.setBoolPref("warnOnAboutNetworking", warnBox.checked);
}

function show(button) {
  let current_tab = document.querySelector(".active");
  let content = document.getElementById(button.getAttribute("value"));
  if (current_tab == content)
    return;
  current_tab.classList.remove("active");
  current_tab.hidden = true;
  content.classList.add("active");
  content.hidden = false;

  let current_button = document.querySelector("[selected=true]");
  current_button.removeAttribute("selected");
  button.setAttribute("selected", "true");

  let autoRefresh = document.getElementById("autorefcheck");
  if (autoRefresh.checked) {
    clearInterval(autoRefresh.interval);
    setAutoRefreshInterval(autoRefresh);
  }

  let title = document.getElementById("sectionTitle");
  title.textContent = button.children[0].textContent;
}

function setAutoRefreshInterval(checkBox) {
  let active_tab = document.querySelector(".active");
  checkBox.interval = setInterval(function() {
    requestNetworkingDataForTab(active_tab.id);
  }, REFRESH_INTERVAL_MS);
}

window.addEventListener("DOMContentLoaded", function load() {
  window.removeEventListener("DOMContentLoaded", load);
  init();
});
