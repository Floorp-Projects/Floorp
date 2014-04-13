/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const gcli = require("gcli/index");
const cookieMgr = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);

/**
 * The cookie 'expires' value needs converting into something more readable
 */
function translateExpires(expires) {
  if (expires == 0) {
    return gcli.lookup("cookieListOutSession");
  }
  return new Date(expires).toLocaleString();
}

/**
 * Check if a given cookie matches a given host
 */
function isCookieAtHost(cookie, host) {
  if (cookie.host == null) {
    return host == null;
  }
  if (cookie.host.startsWith(".")) {
    return host.endsWith(cookie.host);
  }
  else {
    return cookie.host == host;
  }
}

exports.items = [
  {
    name: "cookie",
    description: gcli.lookup("cookieDesc"),
    manual: gcli.lookup("cookieManual")
  },
  {
    name: "cookie list",
    description: gcli.lookup("cookieListDesc"),
    manual: gcli.lookup("cookieListManual"),
    returnType: "cookies",
    exec: function(args, context) {
      let host = context.environment.document.location.host;
      if (host == null || host == "") {
        throw new Error(gcli.lookup("cookieListOutNonePage"));
      }

      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Ci.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          cookies.push({
            host: cookie.host,
            name: cookie.name,
            value: cookie.value,
            path: cookie.path,
            expires: cookie.expires,
            secure: cookie.secure,
            httpOnly: cookie.httpOnly,
            sameDomain: cookie.sameDomain
          });
        }
      }

      return cookies;
    }
  },
  {
    name: "cookie remove",
    description: gcli.lookup("cookieRemoveDesc"),
    manual: gcli.lookup("cookieRemoveManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: gcli.lookup("cookieRemoveKeyDesc"),
      }
    ],
    exec: function(args, context) {
      let host = context.environment.document.location.host;
      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Ci.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          if (cookie.name == args.name) {
            cookieMgr.remove(cookie.host, cookie.name, cookie.path, false);
          }
        }
      }
    }
  },
  {
    item: "converter",
    from: "cookies",
    to: "view",
    exec: function(cookies, context) {
      if (cookies.length == 0) {
        let host = context.environment.document.location.host;
        let msg = gcli.lookupFormat("cookieListOutNoneHost", [ host ]);
        return context.createView({ html: "<span>" + msg + "</span>" });
      }

      for (let cookie of cookies) {
        cookie.expires = translateExpires(cookie.expires);

        let noAttrs = !cookie.secure && !cookie.httpOnly && !cookie.sameDomain;
        cookie.attrs = (cookie.secure ? "secure" : " ") +
                       (cookie.httpOnly ? "httpOnly" : " ") +
                       (cookie.sameDomain ? "sameDomain" : " ") +
                       (noAttrs ? gcli.lookup("cookieListOutNone") : " ");
      }

      return context.createView({
        html:
          "<ul class='gcli-cookielist-list'>" +
          "  <li foreach='cookie in ${cookies}'>" +
          "    <div>${cookie.name}=${cookie.value}</div>" +
          "    <table class='gcli-cookielist-detail'>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("cookieListOutHost") + "</td>" +
          "        <td>${cookie.host}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("cookieListOutPath") + "</td>" +
          "        <td>${cookie.path}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("cookieListOutExpires") + "</td>" +
          "        <td>${cookie.expires}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + gcli.lookup("cookieListOutAttributes") + "</td>" +
          "        <td>${cookie.attrs}</td>" +
          "      </tr>" +
          "      <tr><td colspan='2'>" +
          "        <span class='gcli-out-shortcut' onclick='${onclick}'" +
          "            data-command='cookie set ${cookie.name} '" +
          "            >" + gcli.lookup("cookieListOutEdit") + "</span>" +
          "        <span class='gcli-out-shortcut'" +
          "            onclick='${onclick}' ondblclick='${ondblclick}'" +
          "            data-command='cookie remove ${cookie.name}'" +
          "            >" + gcli.lookup("cookieListOutRemove") + "</span>" +
          "      </td></tr>" +
          "    </table>" +
          "  </li>" +
          "</ul>",
        data: {
          options: { allowEval: true },
          cookies: cookies,
          onclick: context.update,
          ondblclick: context.updateExec
        }
      });
    }
  },
  {
    name: "cookie set",
    description: gcli.lookup("cookieSetDesc"),
    manual: gcli.lookup("cookieSetManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: gcli.lookup("cookieSetKeyDesc")
      },
      {
        name: "value",
        type: "string",
        description: gcli.lookup("cookieSetValueDesc")
      },
      {
        group: gcli.lookup("cookieSetOptionsDesc"),
        params: [
          {
            name: "path",
            type: { name: "string", allowBlank: true },
            defaultValue: "/",
            description: gcli.lookup("cookieSetPathDesc")
          },
          {
            name: "domain",
            type: "string",
            defaultValue: null,
            description: gcli.lookup("cookieSetDomainDesc")
          },
          {
            name: "secure",
            type: "boolean",
            description: gcli.lookup("cookieSetSecureDesc")
          },
          {
            name: "httpOnly",
            type: "boolean",
            description: gcli.lookup("cookieSetHttpOnlyDesc")
          },
          {
            name: "session",
            type: "boolean",
            description: gcli.lookup("cookieSetSessionDesc")
          },
          {
            name: "expires",
            type: "string",
            defaultValue: "Jan 17, 2038",
            description: gcli.lookup("cookieSetExpiresDesc")
          },
        ]
      }
    ],
    exec: function(args, context) {
      let host = context.environment.document.location.host;
      let time = Date.parse(args.expires) / 1000;

      cookieMgr.add(args.domain ? "." + args.domain : host,
                    args.path ? args.path : "/",
                    args.name,
                    args.value,
                    args.secure,
                    args.httpOnly,
                    args.session,
                    time);
    }
  }
];
