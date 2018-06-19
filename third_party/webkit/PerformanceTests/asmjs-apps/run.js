// vim: set ts=2 sw=2 tw=99 et:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var __BENCHMARK__;
var __ARGV__ = (function (argv) {
  var i = 0;
  if (argv[i] == '--')
    i++;
  if (argv.length > i)
    __BENCHMARK__ = argv[i++];
  return argv.slice(i, argv.length);
})(arguments || scriptArgs);

var scriptArgs = __ARGV__;
var arguments = __ARGV__;
var read = typeof(read) == 'undefined' ? readline : read;

function RunBenchmark(file)
{
  var begin = Date.now();
  var printErr = printErr || print;
  var p = print, perr = printErr;
  print = function(){}, perr = print;
  load(file);
  print = p, printErr = perr;
  return Date.now() - begin;
}

function main(argv)
{
  if (!__BENCHMARK__) {
    print('No file given!');
    return quit();
  }

  var file = __BENCHMARK__.replace('.js', '');

  print(RunBenchmark(file + '.js'));
}

main(arguments);

