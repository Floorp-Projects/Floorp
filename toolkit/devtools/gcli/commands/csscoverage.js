/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");

const { gDevTools } = require("resource:///modules/devtools/gDevTools.jsm");
const promise = require("resource://gre/modules/Promise.jsm").Promise;

const domtemplate = require("gcli/util/domtemplate");
const csscoverage = require("devtools/server/actors/csscoverage");
const l10n = csscoverage.l10n;

/**
 * The commands/converters for GCLI
 */
exports.items = [
  {
    name: "csscoverage",
    hidden: true,
    description: l10n.lookup("csscoverageDesc"),
  },
  {
    name: "csscoverage start",
    hidden: true,
    description: l10n.lookup("csscoverageStartDesc"),
    exec: function*(args, context) {
      let usage = yield csscoverage.getUsage(context.environment.target);
      yield usage.start(context.environment.chromeWindow,
                        context.environment.target);
    }
  },
  {
    name: "csscoverage stop",
    hidden: true,
    description: l10n.lookup("csscoverageStopDesc"),
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);
      yield usage.stop();
      yield gDevTools.showToolbox(target, "styleeditor");
    }
  },
  {
    name: "csscoverage oneshot",
    hidden: true,
    description: l10n.lookup("csscoverageOneShotDesc"),
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);
      yield usage.oneshot();
      yield gDevTools.showToolbox(target, "styleeditor");
    }
  },
  {
    name: "csscoverage toggle",
    hidden: true,
    description: l10n.lookup("csscoverageToggleDesc"),
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);

      let running = yield usage.toggle();
      if (running) {
        return l10n.lookup("csscoverageRunningReply");
      }

      yield usage.stop();
      yield gDevTools.showToolbox(target, "styleeditor");
    }
  },
  {
    name: "csscoverage report",
    hidden: true,
    description: l10n.lookup("csscoverageReportDesc"),
    exec: function*(args, context) {
      let usage = yield csscoverage.getUsage(context.environment.target);
      return {
        isTypedData: true,
        type: "csscoveragePageReport",
        data: yield usage.createPageReport()
      };
    }
  },
  {
    item: "converter",
    from: "csscoveragePageReport",
    to: "dom",
    exec: function*(csscoveragePageReport, context) {
      let target = context.environment.target;

      let toolbox = yield gDevTools.showToolbox(target, "styleeditor");
      let panel = toolbox.getCurrentPanel();

      let host = panel._panelDoc.querySelector(".csscoverage-report");
      let templ = panel._panelDoc.querySelector(".csscoverage-template");

      templ = templ.cloneNode(true);
      templ.hidden = false;

      let data = {
        pages: csscoveragePageReport.pages,
        unusedRules: csscoveragePageReport.unusedRules,
        onback: () => {
          // The back button clears and hides .csscoverage-report
          while (host.hasChildNodes()) {
            host.removeChild(host.firstChild);
          }
          host.hidden = true;
        }
      };

      let addOnClick = rule => {
        rule.onclick = () => {
          panel.selectStyleSheet(rule.url, rule.start.line);
        };
      };

      data.pages.forEach(page => {
        page.preloadRules.forEach(addOnClick);
      });

      data.unusedRules.forEach(addOnClick);

      let options = { allowEval: true, stack: "styleeditor.xul" };
      domtemplate.template(templ, data, options);

      while (templ.hasChildNodes()) {
        host.appendChild(templ.firstChild);
      }
      host.hidden = false;
    }
  }
];
