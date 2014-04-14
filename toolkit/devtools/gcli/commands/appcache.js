/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");

loader.lazyImporter(this, "AppCacheUtils", "resource:///modules/devtools/AppCacheUtils.jsm");

exports.items = [
  {
    name: "appcache",
    description: gcli.lookup("appCacheDesc")
  },
  {
    name: "appcache validate",
    description: gcli.lookup("appCacheValidateDesc"),
    manual: gcli.lookup("appCacheValidateManual"),
    returnType: "appcacheerrors",
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "uri",
          description: gcli.lookup("appCacheValidateUriDesc"),
          defaultValue: null,
        }
      ]
    }],
    exec: function(args, context) {
      let utils;
      let deferred = context.defer();

      if (args.uri) {
        utils = new AppCacheUtils(args.uri);
      } else {
        utils = new AppCacheUtils(context.environment.document);
      }

      utils.validateManifest().then(function(errors) {
        deferred.resolve([errors, utils.manifestURI || "-"]);
      });

      return deferred.promise;
    }
  },
  {
    item: "converter",
    from: "appcacheerrors",
    to: "view",
    exec: function([errors, manifestURI], context) {
      if (errors.length == 0) {
        return context.createView({
          html: "<span>" + gcli.lookup("appCacheValidatedSuccessfully") + "</span>"
        });
      }

      return context.createView({
        html:
          "<div>" +
          "  <h4>Manifest URI: ${manifestURI}</h4>" +
          "  <ol>" +
          "    <li foreach='error in ${errors}'>${error.msg}</li>" +
          "  </ol>" +
          "</div>",
        data: {
          errors: errors,
          manifestURI: manifestURI
        }
      });
    }
  },
  {
    name: "appcache clear",
    description: gcli.lookup("appCacheClearDesc"),
    manual: gcli.lookup("appCacheClearManual"),
    exec: function(args, context) {
      let utils = new AppCacheUtils(args.uri);
      utils.clearAll();

      return gcli.lookup("appCacheClearCleared");
    }
  },
  {
    name: "appcache list",
    description: gcli.lookup("appCacheListDesc"),
    manual: gcli.lookup("appCacheListManual"),
    returnType: "appcacheentries",
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "search",
          description: gcli.lookup("appCacheListSearchDesc"),
          defaultValue: null,
        },
      ]
    }],
    exec: function(args, context) {
      let utils = new AppCacheUtils();
      return utils.listEntries(args.search);
    }
  },
  {
    item: "converter",
    from: "appcacheentries",
    to: "view",
    exec: function(entries, context) {
      return context.createView({
        html: "" +
          "<ul class='gcli-appcache-list'>" +
          "  <li foreach='entry in ${entries}'>" +
          "    <table class='gcli-appcache-detail'>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListKey") + "</td>" +
          "        <td>${entry.key}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListFetchCount") + "</td>" +
          "        <td>${entry.fetchCount}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListLastFetched") + "</td>" +
          "        <td>${entry.lastFetched}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListLastModified") + "</td>" +
          "        <td>${entry.lastModified}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListExpirationTime") + "</td>" +
          "        <td>${entry.expirationTime}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListDataSize") + "</td>" +
          "        <td>${entry.dataSize}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("appCacheListDeviceID") + "</td>" +
          "        <td>${entry.deviceID} <span class='gcli-out-shortcut' " +
          "onclick='${onclick}' ondblclick='${ondblclick}' " +
          "data-command='appcache viewentry ${entry.key}'" +
          ">" + gcli.lookup("appCacheListViewEntry") + "</span>" +
          "        </td>" +
          "      </tr>" +
          "    </table>" +
          "  </li>" +
          "</ul>",
        data: {
          entries: entries,
          onclick: context.update,
          ondblclick: context.updateExec
        }
      });
    }
  },
  {
    name: "appcache viewentry",
    description: gcli.lookup("appCacheViewEntryDesc"),
    manual: gcli.lookup("appCacheViewEntryManual"),
    params: [
      {
        type: "string",
        name: "key",
        description: gcli.lookup("appCacheViewEntryKey"),
        defaultValue: null,
      }
    ],
    exec: function(args, context) {
      let utils = new AppCacheUtils();
      return utils.viewEntry(args.key);
    }
  }
];
