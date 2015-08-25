/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Security devtool supports the following arguments:
 * * Security CSP
 *   Provides feedback about the current CSP
 *
 *  * Security referrer
 *    Provides information about the current referrer policy
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

const NEXT_URI_HEADER = l10n.lookup("securityReferrerNextURI");
const CALCULATED_REFERRER_HEADER = l10n.lookup("securityReferrerCalculatedReferrer");
/* The official names from the W3C Referrer Policy Draft http://www.w3.org/TR/referrer-policy/ */
const REFERRER_POLICY_NAMES = [ "None When Downgrade", "None", "Origin Only", "Origin When Cross-Origin", "Unsafe URL" ];

exports.items = [
  {
    // --- General Security information
    name: "security",
    description: l10n.lookup("securityPrivacyDesc"),
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
  {
    // --- Referrer Policy specific Security information
    item: "command",
    runAt: "server",
    name: "security referrer",
    description: l10n.lookup("securityReferrerPolicyDesc"),
    manual: l10n.lookup("securityReferrerPolicyManual"),
    returnType: "securityReferrerPolicyInfo",
    exec: function(args, context) {
      var doc = context.environment.document;

      var referrerPolicy = doc.referrerPolicy;

      var pageURI = doc.documentURIObject;
      var sameDomainReferrer = "";
      var otherDomainReferrer = "";
      var downgradeReferrer = "";
      var origin = pageURI.prePath;

      switch (referrerPolicy) {
        case Ci.nsIHttpChannel.REFERRER_POLICY_NO_REFERRER:
          // sends no referrer
          sameDomainReferrer = otherDomainReferrer = downgradeReferrer = "(no referrer)";
          break;
        case Ci.nsIHttpChannel.REFERRER_POLICY_ORIGIN:
          // only sends the origin of the referring URL
          sameDomainReferrer = otherDomainReferrer = downgradeReferrer = origin;
          break;
        case Ci.nsIHttpChannel.REFERRER_POLICY_ORIGIN_WHEN_XORIGIN:
          // same as default, but reduced to ORIGIN when cross-origin.
          sameDomainReferrer = pageURI.spec;
          otherDomainReferrer = origin;
          downgradeReferrer = "(no referrer)";
          break;
        case Ci.nsIHttpChannel.REFERRER_POLICY_UNSAFE_URL:
          // always sends the referrer, even on downgrade.
          sameDomainReferrer = otherDomainReferrer = downgradeReferrer = pageURI.spec;
          break;
        case Ci.nsIHttpChannel.REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE:
          // default state, doesn't send referrer from https->http
          sameDomainReferrer = otherDomainReferrer = pageURI.spec;
          downgradeReferrer = "(no referrer)";
          break;
        default:
          // this is a new referrer policy which we do not know about
          sameDomainReferrer = otherDomainReferrer = downgradeReferrer = "(unknown Referrer Policy)";
          break;
      }

      var sameDomainUri = origin + "/*";

      var referrerUrls = [
        // add the referrer uri 'referrer' we would send when visiting 'uri'
        {uri: 'http://example.com/', referrer: otherDomainReferrer},
        {uri: sameDomainUri, referrer: sameDomainReferrer}
      ];

      if (pageURI.schemeIs('https')) {
        // add the referrer we would send on downgrading http->https
        referrerUrls.push({uri: "http://"+pageURI.hostPort+"/*", referrer: downgradeReferrer});
      }

      return {
        header: l10n.lookupFormat("securityReferrerPolicyReportHeader", [pageURI.spec]),
        policyName: REFERRER_POLICY_NAMES[referrerPolicy],
        urls: referrerUrls
      }
    }
  },
  {
    item: "converter",
    from: "securityReferrerPolicyInfo",
    to: "view",
    exec: function(referrerPolicyInfo, context) {
      return context.createView({
          html:
          "<div class='gcli-referrer-policy'>" +
          "  <strong> ${rpi.header} </strong> <br />" +
          "  ${rpi.policyName} <br />" +
          "  <table class='gcli-referrer-policy-detail' cellspacing='10' >" +
          "    <tr><th> " + NEXT_URI_HEADER + " </th><th> " + CALCULATED_REFERRER_HEADER + " </th></tr>" +
          // iterate all policies
          "    <tr foreach='nextURI in ${rpi.urls}' >" +
          "      <td> ${nextURI.uri} </td>" +
          "      <td> ${nextURI.referrer} </td>" +
          "    </tr>" +
          "  </table>" +
          "</div>",
          data: {
            rpi: referrerPolicyInfo,
          }
        });
     }
  }
];
