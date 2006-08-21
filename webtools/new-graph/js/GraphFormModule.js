/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is new-graph code.
 *
 * The Initial Developer of the Original Code is
 *    Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var GraphFormModules = [];
var GraphFormModuleCount = 0;

function GraphFormModule(userConfig) {
    GraphFormModuleCount++;
    this.__proto__.__proto__.constructor.call(this, "graphForm" + GraphFormModuleCount, userConfig);
}

GraphFormModule.prototype = {
    __proto__: new YAHOO.widget.Module(),

    imageRoot: "",

    testId: null,
    baseline: false,
    average: false,
    color: "#000000",

    init: function (el, userConfig) {
        var self = this;

        this.__proto__.__proto__.init.call(this, el/*, userConfig*/);
        
        this.cfg = new YAHOO.util.Config(this);
        this.cfg.addProperty("testid", { suppressEvent: true });
        this.cfg.addProperty("baseline", { suppressEvent: true });

        if (userConfig)
            this.cfg.applyConfig(userConfig, true);

        var form, td, el;

        form = new DIV({ class: "graphform-line" });

        td = new SPAN();
/*
        el = new IMG({ src: "js/img/plus.png", class: "plusminus",
                       onclick: function(event) { addGraphForm(); } });
        td.appendChild(el);
*/
        el = new IMG({ src: "js/img/minus.png", class: "plusminus",
                       onclick: function(event) { self.remove(); } });
        td.appendChild(el);
        form.appendChild(td);

        td = new SPAN();
        el = new IMG({ style: "border: 1px solid black; vertical-align: middle; margin: 3px;",
                       width: 15,
                       height: 15,
                       src: "js/img/clear.png" });
        this.colorDiv = el;
        td.appendChild(el);
        form.appendChild(td);

        td = new SPAN();
        el = new SELECT({ name: "testname",
                          class: "testname",
                          onchange: function(event) { self.onChangeTest(); } });
        this.testSelect = el;
        td.appendChild(el);
        form.appendChild(td);

        td = new SPAN({ style: "padding-left: 10px;"});
        appendChildNodes(td, "Average:");
        el = new INPUT({ name: "average",
                         type: "checkbox",
                         onchange: function(event) { self.average = event.target.checked; } });
        this.averageCheckbox = el;
        td.appendChild(el);
        form.appendChild(td);

        this.setBody (form);

        var forceTestId = null;
        if (userConfig) {
            forceTestId = this.cfg.getProperty("testid");
            baseline = this.cfg.getProperty("baseline");
            if (baseline)
                this.onBaseLineRadioClick();
        }

        Tinderbox.requestTestList(function (tests) {
                                      var opts = [];
                                      // let's sort by machine name
                                      var sortedTests = Array.sort(tests, function (a, b) {
                                                                       if (a.machine < b.machine) return -1;
                                                                       if (a.machine > b.machine) return 1;
                                                                       if (a.test < b.test) return -1;
                                                                       if (a.test > b.test) return 1;
                                                                       if (a.test_type < b.test_type) return -1;
                                                                       if (a.test_type > b.test_type) return 1;
                                                                       return 0;
                                                                   });

                                      for each (var test in sortedTests) {
                                          var tstr = test.machine + " - " + test.test + " (" + test.test_type + ")";
                                          opts.push(new OPTION({ value: test.id }, tstr));
                                      }
                                      replaceChildNodes(self.testSelect, opts);

                                      if (forceTestId != null) {
                                          self.testSelect.value = forceTestId;
                                      } else {
                                          self.testSelect.value = sortedTests[0].id;
                                      }
                                      setTimeout(function () { self.onChangeTest(forceTestId); }, 0);
                                  });

        GraphFormModules.push(this);
    },

    getQueryString: function (prefix) {
        return prefix + "tid=" + this.testId + "&" + prefix + "bl=" + (this.baseline ? "1" : "0");
    },

/*
    handleQueryStringData: function (prefix, qsdata) {
        var tbox = qsdata[prefix + "tb"];
        var tname = qsdata[prefix + "tn"];
        var baseline = (qsdata[prefix + "bl"] == "1");

        if (baseline)
            this.onBaseLineRadioClick();

        this.forcedTinderbox = tbox;
        this.tinderboxSelect.value = tbox;

        this.forcedTestname = tname;
        this.testSelect.value = tname;
        this.onChangeTinderbox();
    },
*/

    onChangeTest: function (forceTestId) {
        this.testId = this.testSelect.value;
    },

    onBaseLineRadioClick: function () {
        GraphFormModules.forEach(function (g) { g.baseline = false; });
        this.baseline = true;
    },

    setColor: function (newcolor) {
        this.color = newcolor;
        this.colorDiv.style.backgroundColor = colorToRgbString(newcolor);
    },

    remove: function () {
        if (GraphFormModules.length == 1)
            return;

        var nf = [];
        for each (var f in GraphFormModules) {
            if (f != this)
                nf.push(f);
        }
        GraphFormModules = nf;
        this.destroy();
    },
};
