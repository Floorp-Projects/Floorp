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
    // 'pref' command
    item: 'command',
    name: 'pref',
    description: l10n.lookup('prefDesc'),
    manual: l10n.lookup('prefManual')
  },
  {
    // 'pref show' command
    item: 'command',
    name: 'pref show',
    runAt: 'client',
    description: l10n.lookup('prefShowDesc'),
    manual: l10n.lookup('prefShowManual'),
    params: [
      {
        name: 'setting',
        type: 'setting',
        description: l10n.lookup('prefShowSettingDesc'),
        manual: l10n.lookup('prefShowSettingManual')
      }
    ],
    exec: function(args, context) {
      return l10n.lookupFormat('prefShowSettingValue',
                               [ args.setting.name, args.setting.value ]);
    }
  },
  {
    // 'pref set' command
    item: 'command',
    name: 'pref set',
    runAt: 'client',
    description: l10n.lookup('prefSetDesc'),
    manual: l10n.lookup('prefSetManual'),
    params: [
      {
        name: 'setting',
        type: 'setting',
        description: l10n.lookup('prefSetSettingDesc'),
        manual: l10n.lookup('prefSetSettingManual')
      },
      {
        name: 'value',
        type: 'settingValue',
        description: l10n.lookup('prefSetValueDesc'),
        manual: l10n.lookup('prefSetValueManual')
      }
    ],
    exec: function(args, context) {
      args.setting.value = args.value;
    }
  },
  {
    // 'pref reset' command
    item: 'command',
    name: 'pref reset',
    runAt: 'client',
    description: l10n.lookup('prefResetDesc'),
    manual: l10n.lookup('prefResetManual'),
    params: [
      {
        name: 'setting',
        type: 'setting',
        description: l10n.lookup('prefResetSettingDesc'),
        manual: l10n.lookup('prefResetSettingManual')
      }
    ],
    exec: function(args, context) {
      args.setting.setDefault();
    }
  }
];
