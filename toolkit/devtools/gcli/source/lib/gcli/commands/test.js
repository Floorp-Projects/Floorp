/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

require('../test/suite');

var examiner = require('../testharness/examiner');
var stati = require('../testharness/status').stati;
var helpers = require('../test/helpers');
var cli = require('../cli');
var Requisition = require('../cli').Requisition;
var createRequisitionAutomator = require('../test/automators/requisition').createRequisitionAutomator;

exports.optionsContainer = [];

exports.items = [
  {
    item: 'type',
    name: 'suite',
    parent: 'selection',
    cacheable: true,
    lookup: function() {
      return Object.keys(examiner.suites).map(function(name) {
        return { name: name, value: examiner.suites[name] };
      });
    }
  },
  {
    item: 'command',
    name: 'test',
    description: 'Run GCLI unit tests',
    params: [
      {
        name: 'suite',
        type: 'suite',
        description: 'Test suite to run.',
        defaultValue: examiner
      },
      {
        name: 'usehost',
        type: 'boolean',
        description: 'Run the unit tests in the host window',
        option: true
      }
    ],
    returnType: 'examiner-output',
    noRemote: true,
    exec: function(args, context) {
      if (args.usehost && exports.optionsContainer.length === 0) {
        throw new Error('Can\'t use --usehost without injected options');
      }

      var options;
      if (args.usehost) {
        options = exports.optionsContainer[0];
      }
      else {
        options = {
          isNode: (typeof(process) !== 'undefined' && process.title === 'node'),
          isFirefox: false,
          isPhantomjs: false,
          isNoDom: true,
          requisition: new Requisition(context.system)
        };
        options.automator = createRequisitionAutomator(options.requisition);
      }

      var requisition = options.requisition;
      requisition.system.commands.get('mocks').on(requisition);
      helpers.resetResponseTimes();
      examiner.reset();

      return args.suite.run(options).then(function() {
        requisition.system.commands.get('mocks').off(requisition);
        var output = context.typedData('examiner-output', examiner.toRemote());

        if (output.data.summary.status === stati.pass) {
          return output;
        }
        else {
          cli.logErrors = false;
          throw output;
        }
      });
    }
  },
  {
    item: 'converter',
    from: 'examiner-output',
    to: 'string',
    exec: function(output, conversionContext) {
      return '\n' + examiner.detailedResultLog('NodeJS/NoDom') +
             '\n' + helpers.timingSummary;
    }
  },
  {
    item: 'converter',
    from: 'examiner-output',
    to: 'view',
    exec: function(output, conversionContext) {
      return {
        html:
          '<div>\n' +
          '  <table class="gcliTestResults">\n' +
          '    <thead>\n' +
          '      <tr>\n' +
          '        <th class="gcliTestSuite">Suite</th>\n' +
          '        <th>Test</th>\n' +
          '        <th>Results</th>\n' +
          '        <th>Checks</th>\n' +
          '        <th>Notes</th>\n' +
          '      </tr>\n' +
          '    </thead>\n' +
          '    <tbody foreach="suite in ${suites}">\n' +
          '      <tr foreach="test in ${suite.tests}" title="${suite.name}.${test.name}()">\n' +
          '        <td class="gcliTestSuite">${suite.name}</td>\n' +
          '        <td class="gcliTestTitle">${test.title}</td>\n' +
          '        <td class="gcliTest${test.status.name}">${test.status.name}</td>\n' +
          '        <td class="gcliTestChecks">${test.checks}</td>\n' +
          '        <td class="gcliTestMessages">\n' +
          '          <div foreach="failure in ${test.failures}">\n' +
          '            ${failure.message}\n' +
          '            <ul if="${failure.params}">\n' +
          '              <li>P1: ${failure.p1}</li>\n' +
          '              <li>P2: ${failure.p2}</li>\n' +
          '            </ul>\n' +
          '          </div>\n' +
          '        </td>\n' +
          '      </tr>\n' +
          '    </tbody>\n' +
          '    <tfoot>\n' +
          '      <tr>\n' +
          '        <th></th>\n' +
          '        <th>Total</th>\n' +
          '        <th>${summary.status.name}</th>\n' +
          '        <th class="gcliTestChecks">${summary.checks}</th>\n' +
          '        <th></th>\n' +
          '      </tr>\n' +
          '    </tfoot>\n' +
          '  </table>\n' +
          '</div>',
        css:
          '.gcliTestSkipped {\n' +
          '  background-color: #EEE;\n' +
          '  color: #000;\n' +
          '}\n' +
          '\n' +
          '.gcliTestExecuting {\n' +
          '  background-color: #888;\n' +
          '  color: #FFF;\n' +
          '}\n' +
          '\n' +
          '.gcliTestWaiting {\n' +
          '  background-color: #FFA;\n' +
          '  color: #000;\n' +
          '}\n' +
          '\n' +
          '.gcliTestPass {\n' +
          '  background-color: #8F8;\n' +
          '  color: #000;\n' +
          '}\n' +
          '\n' +
          '.gcliTestFail {\n' +
          '  background-color: #F00;\n' +
          '  color: #FFF;\n' +
          '}\n' +
          '\n' +
          'td.gcliTestSuite {\n' +
          '  font-family: monospace;\n' +
          '  font-size: 90%;\n' +
          '  text-align: right;\n' +
          '}\n' +
          '\n' +
          '.gcliTestResults th.gcliTestSuite,\n' +
          '.gcliTestResults .gcliTestChecks {\n' +
          '  text-align: right;\n' +
          '}\n' +
          '\n' +
          '.gcliTestResults th {\n' +
          '  text-align: left;\n' +
          '}\n' +
          '\n' +
          '.gcliTestMessages ul {\n' +
          '  margin: 0 0 10px;\n' +
          '  padding-left: 20px;\n' +
          '  list-style-type: square;\n' +
          '}\n',
        cssId: 'gcli-test',
        data: output,
        options: { allowEval: true, stack: 'test.html' }
      };
    }
  }
];
