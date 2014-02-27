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

var l10n = require('../util/l10n');

exports.items = [
  {
    // A type for selecting a known setting
    item: 'type',
    name: 'global',
    parent: 'selection',
    remote: true,
    lookup: function(context) {
      var knownWindows = context.environment.window == null ?
                         [ ] : [ context.environment.window ];

      this.last = findWindows(knownWindows).map(function(window) {
        return { name: windowToString(window), value: window };
      });

      return this.last;
    }
  },
  {
    // A command to switch JS globals
    item: 'command',
    name: 'global',
    description: l10n.lookup('globalDesc'),
    params: [
      {
        name: 'window',
        type: 'global',
        description: l10n.lookup('globalWindowDesc'),
      }
    ],
    returnType: 'string',
    exec: function(args, context) {
      context.shell.global = args.window;
      return l10n.lookupFormat('globalOutput', [ windowToString(args.window) ]);
    }
  }
];

function windowToString(win) {
  return win.location ? win.location.href : 'NodeJS-Global';
}

function findWindows(knownWindows) {
  knownWindows.forEach(function(window) {
    addChildWindows(window, knownWindows);
  });
  return knownWindows;
}

function addChildWindows(win, knownWindows) {
  var iframes = win.document.querySelectorAll('iframe');
  [].forEach.call(iframes, function(iframe) {
    var iframeWin = iframe.contentWindow;
    if (knownWindows.indexOf(iframeWin) === -1) {
      knownWindows.push(iframeWin);
      addChildWindows(iframeWin, knownWindows);
    }
  });
}
