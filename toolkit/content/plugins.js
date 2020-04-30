/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/* JavaScript to enumerate and display all installed plug-ins

 * First, refresh plugins in case anything has been changed recently in
 * prefs: (The "false" argument tells refresh not to reload or activate
 * any plug-ins that would be active otherwise.  In contrast, one would
 * use "true" in the case of ASD instead of restarting)
 */
navigator.plugins.refresh(false);

RPMSendQuery("RequestPlugins", {}).then(aPlugins => {
  var fragment = document.createDocumentFragment();

  // "Installed plugins"
  var id, label;
  if (aPlugins.length) {
    id = "plugs";
    label = "installed-plugins-label";
  } else {
    id = "noplugs";
    label = "no-plugins-are-installed-label";
  }
  var enabledplugins = document.createElement("h1");
  enabledplugins.setAttribute("id", id);
  document.l10n.setAttributes(enabledplugins, label);
  fragment.appendChild(enabledplugins);

  var deprecation = document.createElement("p");
  var deprecationLink = document.createElement("a");
  let deprecationLink_href =
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "npapi";
  deprecationLink.setAttribute("data-l10n-name", "deprecation-link");
  deprecationLink.setAttribute("href", deprecationLink_href);
  deprecation.appendChild(deprecationLink);
  deprecation.setAttribute("class", "notice");
  document.l10n.setAttributes(deprecation, "deprecation-description");
  fragment.appendChild(deprecation);

  var stateNames = {};
  [
    "STATE_SOFTBLOCKED",
    "STATE_BLOCKED",
    "STATE_OUTDATED",
    "STATE_VULNERABLE_UPDATE_AVAILABLE",
    "STATE_VULNERABLE_NO_UPDATE",
  ].forEach(function(label) {
    stateNames[Ci.nsIBlocklistService[label]] = label;
  });

  for (var i = 0; i < aPlugins.length; i++) {
    var plugin = aPlugins[i];
    if (plugin) {
      // "Shockwave Flash"
      var plugname = document.createElement("h2");
      plugname.setAttribute("class", "plugname");
      plugname.appendChild(document.createTextNode(plugin.name));
      fragment.appendChild(plugname);

      var dl = document.createElement("dl");
      fragment.appendChild(dl);

      // "File: Flash Player.plugin"
      var fileDd = document.createElement("dd");
      var file = document.createElement("span");
      file.setAttribute("data-l10n-name", "file");
      file.setAttribute("class", "label");
      fileDd.appendChild(file);
      document.l10n.setAttributes(fileDd, "file-dd", {
        pluginLibraries: plugin.pluginLibraries[0],
      });
      dl.appendChild(fileDd);

      // "Path: /usr/lib/mozilla/plugins/libtotem-cone-plugin.so"
      var pathDd = document.createElement("dd");
      var path = document.createElement("span");
      path.setAttribute("data-l10n-name", "path");
      path.setAttribute("class", "label");
      pathDd.appendChild(path);
      document.l10n.setAttributes(pathDd, "path-dd", {
        pluginFullPath: plugin.pluginFullpath[0],
      });
      dl.appendChild(pathDd);

      // "Version: "
      var versionDd = document.createElement("dd");
      var version = document.createElement("span");
      version.setAttribute("data-l10n-name", "version");
      version.setAttribute("class", "label");
      versionDd.appendChild(version);
      document.l10n.setAttributes(versionDd, "version-dd", {
        version: plugin.version,
      });
      dl.appendChild(versionDd);

      // "State: "
      var stateDd = document.createElement("dd");
      var state = document.createElement("span");
      state.setAttribute("data-l10n-name", "state");
      state.setAttribute("label", "state");
      stateDd.appendChild(state);
      if (plugin.isActive) {
        if (plugin.blocklistState in stateNames) {
          document.l10n.setAttributes(
            stateDd,
            "state-dd-enabled-block-list-state",
            { blockListState: stateNames[plugin.blocklistState] }
          );
        } else {
          document.l10n.setAttributes(stateDd, "state-dd-enabled");
        }
      } else if (plugin.blocklistState in stateNames) {
        document.l10n.setAttributes(
          stateDd,
          "state-dd-disabled-block-list-state",
          { blockListState: stateNames[plugin.blocklistState] }
        );
      } else {
        document.l10n.setAttributes(stateDd, "state-dd-disabled");
      }
      dl.appendChild(stateDd);

      // Plugin Description
      var descDd = document.createElement("dd");
      descDd.appendChild(document.createTextNode(plugin.description));
      dl.appendChild(descDd);

      // MIME Type table
      var mimetypeTable = document.createElement("table");
      mimetypeTable.setAttribute("border", "1");
      mimetypeTable.setAttribute("class", "contenttable");
      fragment.appendChild(mimetypeTable);

      var thead = document.createElement("thead");
      mimetypeTable.appendChild(thead);
      var tr = document.createElement("tr");
      thead.appendChild(tr);

      // "MIME Type" column header
      var typeTh = document.createElement("th");
      typeTh.setAttribute("class", "type");
      document.l10n.setAttributes(typeTh, "mime-type-label");
      tr.appendChild(typeTh);

      // "Description" column header
      var descTh = document.createElement("th");
      descTh.setAttribute("class", "desc");
      document.l10n.setAttributes(descTh, "description-label");
      tr.appendChild(descTh);

      // "Suffixes" column header
      var suffixesTh = document.createElement("th");
      suffixesTh.setAttribute("class", "suff");
      document.l10n.setAttributes(suffixesTh, "suffixes-label");
      tr.appendChild(suffixesTh);

      var tbody = document.createElement("tbody");
      mimetypeTable.appendChild(tbody);

      var mimeTypes = plugin.pluginMimeTypes;
      for (var j = 0; j < mimeTypes.length; j++) {
        var mimetype = mimeTypes[j];
        if (mimetype) {
          var mimetypeRow = document.createElement("tr");
          tbody.appendChild(mimetypeRow);

          // "application/x-shockwave-flash"
          var typename = document.createElement("td");
          typename.appendChild(document.createTextNode(mimetype.type));
          mimetypeRow.appendChild(typename);

          // "Shockwave Flash"
          var description = document.createElement("td");
          description.appendChild(
            document.createTextNode(mimetype.description)
          );
          mimetypeRow.appendChild(description);

          // "swf"
          var suffixes = document.createElement("td");
          suffixes.appendChild(document.createTextNode(mimetype.suffixes));
          mimetypeRow.appendChild(suffixes);
        }
      }
    }
  }

  document.getElementById("outside").appendChild(fragment);
});
