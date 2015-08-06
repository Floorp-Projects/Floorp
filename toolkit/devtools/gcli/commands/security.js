/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Security devtool supports the following arguments:
 * * Security CSP
 *   Provides feedback about the current CSP
 */

"use strict";

const { Cc, Ci, Cu, CC } = require("chrome");
const l10n = require("gcli/l10n");
const CSP = Cc["@mozilla.org/cspcontext;1"].getService(Ci.nsIContentSecurityPolicy);

const GOOD_IMG_SRC = "chrome://browser/content/gcli_sec_good.svg";
const MOD_IMG_SRC = "chrome://browser/content/gcli_sec_moderate.svg";
const BAD_IMG_SRC = "chrome://browser/content/gcli_sec_bad.svg";


// special handling within policy
const POLICY_REPORT_ONLY = "report-only"

// special handling of directives
const DIR_UPGRADE_INSECURE = "upgrade-insecure-requests";

// special handling of sources
const SRC_UNSAFE_INLINE = "'unsafe-inline'";
const SRC_UNSAFE_EVAL = "'unsafe-eval'";

const WILDCARD_MSG = l10n.lookup("securityCSPRemWildCard");
const XSS_WARNING_MSG = l10n.lookup("securityCSPPotentialXSS");
const NO_CSP_ON_PAGE_MSG = l10n.lookup("securityCSPNoCSPOnPage");
const CONTENT_SECURITY_POLICY_MSG = l10n.lookup("securityCSPHeaderOnPage");
const CONTENT_SECURITY_POLICY_REPORT_ONLY_MSG = l10n.lookup("securityCSPROHeaderOnPage");

exports.items = [
  {
    // --- General Security information
    name: "security",
    description: l10n.lookup("securityDesc"),
    manual: l10n.lookup("securityManual")
  },
  {
    // --- CSP specific Security information
    item: "command",
    runAt: "server",
    name: "security csp",
    description: l10n.lookup("securityCSPDesc"),
    manual: l10n.lookup("securityCSPManual"),
    returnType: "securityCSPInfo",
    exec: function(args, context) {

      var cspJSON = context.environment.document.nodePrincipal.cspJSON;
      var cspOBJ = JSON.parse(cspJSON);

      var outPolicies = [];

      var policies = cspOBJ["csp-policies"];

      // loop over all the different policies
      for (var csp in policies) {
        var curPolicy = policies[csp];

        // loop over all the directive-values within that policy
        var outDirectives = [];
        var outHeader = CONTENT_SECURITY_POLICY_MSG;
        for (var dir in curPolicy) {
          var curDir = curPolicy[dir];

          // when iterating properties within the obj we might also
          // encounter the 'report-only' flag, which is not a csp directive.
          if (dir === POLICY_REPORT_ONLY) {
            outHeader = curPolicy[POLICY_REPORT_ONLY] === true ?
                          CONTENT_SECURITY_POLICY_REPORT_ONLY_MSG :
                          CONTENT_SECURITY_POLICY_MSG;
            continue;
          }

          // loop over all the directive-sources within that directive
          var outSrcs = [];

          // special case handling for upgrade-insecure-requests
          // which does not have any srcs
          if (dir === DIR_UPGRADE_INSECURE) {
            outSrcs.push({
              icon: GOOD_IMG_SRC,
              src: "", // no src for upgrade-insecure-requests
              desc: "" // no description for upgrade-insecure-requests
            });
          }

          for (var src in curDir) {
            var curSrc = curDir[src];

            // the default icon and descritpion of the directive-src
            var outIcon = GOOD_IMG_SRC;
            var outDesc = "";

            if (curSrc.indexOf("*") > -1) {
              outIcon = MOD_IMG_SRC;
              outDesc = WILDCARD_MSG;
            }
            if (curSrc == SRC_UNSAFE_INLINE || curSrc == SRC_UNSAFE_EVAL) {
              outIcon = BAD_IMG_SRC;
              outDesc = XSS_WARNING_MSG;
            }
            outSrcs.push({
              icon: outIcon,
              src: curSrc,
              desc: outDesc
            });
          }
          // append info about that directive to the directives array
          outDirectives.push({
            dirValue: dir,
            dirSrc: outSrcs
          });
        }
        // append info about the policy to the policies array
        outPolicies.push({
          header: outHeader,
          directives: outDirectives
        });
      }
      return outPolicies;
    }
  },
  {
    item: "converter",
    from: "securityCSPInfo",
    to: "view",
    exec: function(cspInfo, context) {
      var uri = context.environment.document.documentURI;

      if (cspInfo.length == 0) {
        return context.createView({
          html:
            "<table class='gcli-csp-detail' cellspacing='10' valign='top'>" +
            "  <tr>" +
            "    <td> <img src='chrome://browser/content/gcli_sec_bad.svg' width='20px' /> </td> " +
            "    <td>" + NO_CSP_ON_PAGE_MSG + " <b>" + uri + "</b></td>" +
            "  </tr>" +
            "</table>"});
      }

      return context.createView({
        html:
          "<table class='gcli-csp-detail' cellspacing='10' valign='top'>" +
          // iterate all policies
          "  <tr foreach='csp in ${cspinfo}' >" +
          "    <td> ${csp.header} <b>" + uri + "</b><br/><br/>" +
          "      <table class='gcli-csp-dir-detail' valign='top'>" +
          // >> iterate all directives
          "        <tr foreach='dir in ${csp.directives}' >" +
          "          <td valign='top'> ${dir.dirValue} </td>" +
          "          <td valign='top'>" +
          "            <table class='gcli-csp-src-detail' valign='top'>" +
          // >> >> iterate all srs
          "              <tr foreach='src in ${dir.dirSrc}' >" +
          "                <td valign='center' width='20px'> <img src= \"${src.icon}\" width='20px' /> </td> " +
          "                <td valign='center' width='200px'> ${src.src} </td>" +
          "                <td valign='center'> ${src.desc} </td>" +
          "              </tr>" +
          "            </table>" +
          "          </td>" +
          "        </tr>" +
          "      </table>" +
          "    </td>" +
          "  </tr>" +
          "</table>",
          data: {
            cspinfo: cspInfo,
          }
        });
    }
  },
];
