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

var host = require('../util/host');
var l10n = require('../util/l10n');
var cli = require('../cli');

exports.items = [
  {
    // 'cd' command
    item: 'command',
    name: 'cd',
    description: l10n.lookup('cdDesc'),
    manual: l10n.lookup('cdManual'),
    params: [
      {
        name: 'directory',
        type: {
          name: 'file',
          filetype: 'directory',
          existing: 'yes'
        },
        description: l10n.lookup('cdDirectoryDesc')
      }
    ],
    returnType: 'string',
    exec: function(args, context) {
      context.shell.cwd = args.directory;
      return l10n.lookupFormat('cdOutput', [ context.shell.cwd ]);
    }
  },
  {
    // 'exec' command
    item: 'command',
    name: 'exec',
    description: l10n.lookup('execDesc'),
    manual: l10n.lookup('execManual'),
    params: [
      {
        name: 'command',
        type: 'string',
        description: l10n.lookup('execCommandDesc')
      }
    ],
    returnType: 'output',
    exec: function(args, context) {
      var cmdArgs = cli.tokenize(args.command).map(function(arg) {
        return arg.text;
      });
      var cmd = cmdArgs.shift();

      var execSpec = {
        cmd: cmd,
        args: cmdArgs,
        env: context.shell.env,
        cwd: context.shell.cwd
      };

      return host.exec(execSpec).then(function(output) {
        if (output.code === 0) {
          return output;
        }

        throw output.data;
      }, function(output) {
        throw output.data;
      });
    }
  },
  {
    // How we display the output of a generic exec command: we have to assume
    // that it is a string to be displayed in a monospaced font
    item: 'converter',
    from: 'output',
    to: 'view',
    exec: function(output, context) {
      return {
        html: '<pre>${output.data}</pre>',
        data: { output: output }
      };
    }
  },
  {
    item: 'converter',
    from: 'output',
    to: 'string',
    exec: function(output, context) {
      return output.data;
    }
  }
];
