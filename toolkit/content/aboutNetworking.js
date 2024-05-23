/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FileUtils = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
).FileUtils;
const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);
const gDirServ = Cc["@mozilla.org/file/directory_service;1"].getService(
  Ci.nsIDirectoryServiceProvider
);
const gNetLinkSvc =
  Cc["@mozilla.org/network/network-link-service;1"] &&
  Cc["@mozilla.org/network/network-link-service;1"].getService(
    Ci.nsINetworkLinkService
  );

const gRequestNetworkingData = {
  http: gDashboard.requestHttpConnections,
  sockets: gDashboard.requestSockets,
  dns: gDashboard.requestDNSInfo,
  websockets: gDashboard.requestWebsocketConnections,
  dnslookuptool: () => {},
  rcwn: gDashboard.requestRcwnStats,
  networkid: displayNetworkID,
};
const gDashboardCallbacks = {
  http: displayHttp,
  sockets: displaySockets,
  dns: displayDns,
  websockets: displayWebsockets,
  rcwn: displayRcwnStats,
};

const REFRESH_INTERVAL_MS = 3000;

function col(element) {
  let col = document.createElement("td");
  let content = document.createTextNode(element);
  col.appendChild(content);
  return col;
}

function displayHttp(data) {
  let cont = document.getElementById("http_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "http_content");

  for (let i = 0; i < data.connections.length; i++) {
    let row = document.createElement("tr");
    row.appendChild(col(data.connections[i].host));
    row.appendChild(col(data.connections[i].port));
    row.appendChild(col(data.connections[i].httpVersion));
    row.appendChild(col(data.connections[i].ssl));
    row.appendChild(col(data.connections[i].active.length));
    row.appendChild(col(data.connections[i].idle.length));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displaySockets(data) {
  let cont = document.getElementById("sockets_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "sockets_content");

  for (let i = 0; i < data.sockets.length; i++) {
    let row = document.createElement("tr");
    row.appendChild(col(data.sockets[i].host));
    row.appendChild(col(data.sockets[i].port));
    row.appendChild(col(data.sockets[i].type));
    row.appendChild(col(data.sockets[i].active));
    row.appendChild(col(data.sockets[i].sent));
    row.appendChild(col(data.sockets[i].received));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayDns(data) {
  let suffixContent = document.getElementById("dns_suffix_content");
  let suffixParent = suffixContent.parentNode;
  let suffixes = [];
  try {
    suffixes = gNetLinkSvc.dnsSuffixList; // May throw
  } catch (e) {}
  let suffix_tbody = document.createElement("tbody");
  suffix_tbody.id = "dns_suffix_content";
  for (let suffix of suffixes) {
    let row = document.createElement("tr");
    row.appendChild(col(suffix));
    suffix_tbody.appendChild(row);
  }
  suffixParent.replaceChild(suffix_tbody, suffixContent);

  let trr_url_tbody = document.createElement("tbody");
  trr_url_tbody.id = "dns_trr_url";
  let trr_url = document.createElement("tr");
  trr_url.appendChild(col(Services.dns.currentTrrURI));
  trr_url.appendChild(col(Services.dns.currentTrrMode));
  trr_url_tbody.appendChild(trr_url);
  let prevURL = document.getElementById("dns_trr_url");
  prevURL.parentNode.replaceChild(trr_url_tbody, prevURL);

  let cont = document.getElementById("dns_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "dns_content");

  for (let i = 0; i < data.entries.length; i++) {
    let row = document.createElement("tr");
    row.appendChild(col(data.entries[i].hostname));
    row.appendChild(col(data.entries[i].family));
    row.appendChild(col(data.entries[i].trr));
    let column = document.createElement("td");

    for (let j = 0; j < data.entries[i].hostaddr.length; j++) {
      column.appendChild(document.createTextNode(data.entries[i].hostaddr[j]));
      column.appendChild(document.createElement("br"));
    }

    row.appendChild(column);
    row.appendChild(col(data.entries[i].expiration));
    row.appendChild(col(data.entries[i].originAttributesSuffix));
    row.appendChild(col(data.entries[i].flags));
    new_cont.appendChild(row);
  }

  parent.replaceChild(new_cont, cont);
}

function displayWebsockets(data) {
  let cont = document.getElementById("websockets_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "websockets_content");

  for (let i = 0; i < data.websockets.length; i++) {
    let row = document.createElement("tr");
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

function displayRcwnStats(data) {
  let status = Services.prefs.getBoolPref("network.http.rcwn.enabled");
  let linkType = Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN;
  try {
    linkType = gNetLinkSvc.linkType;
  } catch (e) {}
  if (
    !(
      linkType == Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN ||
      linkType == Ci.nsINetworkLinkService.LINK_TYPE_ETHERNET ||
      linkType == Ci.nsINetworkLinkService.LINK_TYPE_USB ||
      linkType == Ci.nsINetworkLinkService.LINK_TYPE_WIFI
    )
  ) {
    status = false;
  }

  let cacheWon = data.rcwnCacheWonCount;
  let netWon = data.rcwnNetWonCount;
  let total = data.totalNetworkRequests;
  let cacheSlow = data.cacheSlowCount;
  let cacheNotSlow = data.cacheNotSlowCount;

  document.getElementById("rcwn_status").innerText = status;
  document.getElementById("total_req_count").innerText = total;
  document.getElementById("rcwn_cache_won_count").innerText = cacheWon;
  document.getElementById("rcwn_cache_net_count").innerText = netWon;
  document.getElementById("rcwn_cache_slow").innerText = cacheSlow;
  document.getElementById("rcwn_cache_not_slow").innerText = cacheNotSlow;

  // Keep in sync with CachePerfStats::EDataType in CacheFileUtils.h
  const perfStatTypes = ["open", "read", "write", "entryopen"];

  const perfStatFieldNames = ["avgShort", "avgLong", "stddevLong"];

  for (let typeIndex in perfStatTypes) {
    for (let statFieldIndex in perfStatFieldNames) {
      document.getElementById(
        "rcwn_perfstats_" +
          perfStatTypes[typeIndex] +
          "_" +
          perfStatFieldNames[statFieldIndex]
      ).innerText =
        data.perfStats[typeIndex][perfStatFieldNames[statFieldIndex]];
    }
  }
}

function displayNetworkID() {
  try {
    let linkIsUp = gNetLinkSvc.isLinkUp;
    let linkStatusKnown = gNetLinkSvc.linkStatusKnown;
    let networkID = gNetLinkSvc.networkID;

    document.getElementById("networkid_isUp").innerText = linkIsUp;
    document.getElementById("networkid_statusKnown").innerText =
      linkStatusKnown;
    document.getElementById("networkid_id").innerText = networkID;
  } catch (e) {
    document.getElementById("networkid_isUp").innerText = "<unknown>";
    document.getElementById("networkid_statusKnown").innerText = "<unknown>";
    document.getElementById("networkid_id").innerText = "<unknown>";
  }
}

function requestAllNetworkingData() {
  for (let id in gRequestNetworkingData) {
    requestNetworkingDataForTab(id);
  }
}

function requestNetworkingDataForTab(id) {
  gRequestNetworkingData[id](gDashboardCallbacks[id]);
}

let gInited = false;
function init() {
  if (gInited) {
    return;
  }
  gInited = true;

  requestAllNetworkingData();

  let autoRefresh = document.getElementById("autorefcheck");
  if (autoRefresh.checked) {
    setAutoRefreshInterval(autoRefresh);
  }

  autoRefresh.addEventListener("click", function () {
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
  if (document.getElementById("autorefcheck").checked) {
    refr.disabled = "disabled";
  }

  // Event delegation on #categories element
  let menu = document.getElementById("categories");
  menu.addEventListener("click", function click(e) {
    if (e.target && e.target.parentNode == menu) {
      show(e.target);
    }
  });

  let clearHTTPCache = document.getElementById("clearHTTPCache");
  clearHTTPCache.addEventListener("click", async function () {
    Services.cache2.clear();
  });

  let dnsLookupButton = document.getElementById("dnsLookupButton");
  dnsLookupButton.addEventListener("click", function () {
    doLookup();
  });

  let clearDNSCache = document.getElementById("clearDNSCache");
  clearDNSCache.addEventListener("click", function () {
    Services.dns.clearCache(true);
  });

  if (location.hash) {
    let sectionButton = document.getElementById(
      "category-" + location.hash.substring(1)
    );
    if (sectionButton) {
      sectionButton.click();
    }
  }
}

function show(button) {
  let current_tab = document.querySelector(".active");
  let category = button.getAttribute("id").substring("category-".length);
  let content = document.getElementById(category);
  if (current_tab == content) {
    return;
  }
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
  location.hash = category;
}

function setAutoRefreshInterval(checkBox) {
  let active_tab = document.querySelector(".active");
  checkBox.interval = setInterval(function () {
    requestNetworkingDataForTab(active_tab.id);
  }, REFRESH_INTERVAL_MS);
}

// We use the pageshow event instead of onload. This is needed because sometimes
// the page is loaded via session-restore/bfcache. In such cases we need to call
// init() to keep the page behaviour consistent with the ticked checkboxes.
// Mostly the issue is with the autorefresh checkbox.
window.addEventListener("pageshow", function () {
  init();
});

function doLookup() {
  let host = document.getElementById("host").value;
  if (host) {
    try {
      gDashboard.requestDNSLookup(host, displayDNSLookup);
    } catch (e) {}
    try {
      gDashboard.requestDNSHTTPSRRLookup(host, displayHTTPSRRLookup);
    } catch (e) {}
  }
}

function displayDNSLookup(data) {
  let cont = document.getElementById("dnslookuptool_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "dnslookuptool_content");

  if (data.answer) {
    for (let address of data.address) {
      let row = document.createElement("tr");
      row.appendChild(col(address));
      new_cont.appendChild(row);
    }
  } else {
    new_cont.appendChild(col(data.error));
  }

  parent.replaceChild(new_cont, cont);
}

function displayHTTPSRRLookup(data) {
  let cont = document.getElementById("https_rr_content");
  let parent = cont.parentNode;
  let new_cont = document.createElement("tbody");
  new_cont.setAttribute("id", "https_rr_content");

  if (data.answer) {
    for (let record of data.records) {
      let row = document.createElement("tr");
      let alpn = record.alpn ? `alpn="${record.alpn.alpn}" ` : "";
      let noDefaultAlpn = record.noDefaultAlpn ? "noDefaultAlpn " : "";
      let port = record.port ? `port="${record.port.port}" ` : "";
      let echConfig = record.echConfig
        ? `echConfig="${record.echConfig.echConfig}" `
        : "";
      let ODoHConfig = record.ODoHConfig
        ? `odoh="${record.ODoHConfig.ODoHConfig}" `
        : "";
      let ipv4hint = "";
      let ipv6hint = "";
      if (record.ipv4Hint) {
        let ipv4Str = "";
        for (let addr of record.ipv4Hint.address) {
          ipv4Str += `${addr}, `;
        }
        // Remove ", " at the end.
        ipv4Str = ipv4Str.slice(0, -2);
        ipv4hint = `ipv4hint="${ipv4Str}" `;
      }
      if (record.ipv6Hint) {
        let ipv6Str = "";
        for (let addr of record.ipv6Hint.address) {
          ipv6Str += `${addr}, `;
        }
        // Remove ", " at the end.
        ipv6Str = ipv6Str.slice(0, -2);
        ipv6hint = `ipv6hint="${ipv6Str}" `;
      }

      let str = `${record.priority} ${record.targetName} `;
      str += `(${alpn}${noDefaultAlpn}${port}`;
      str += `${ipv4hint}${echConfig}${ipv6hint}`;
      str += `${ODoHConfig})`;
      row.appendChild(col(str));
      new_cont.appendChild(row);
    }
  } else {
    new_cont.appendChild(col(data.error));
  }

  parent.replaceChild(new_cont, cont);
}
