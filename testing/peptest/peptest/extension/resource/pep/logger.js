/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Simple logging module that dumps to the console
 */

var EXPORTED_SYMBOLS = ['debug', 'info', 'warning', 'error', 'log'];

function debug(msg) {
  dump('PEP DEBUG ' + msg + '\n');
};

function info(msg) {
  dump('PEP INFO ' + msg + '\n');
};

function warning(msg) {
  dump('PEP WARNING ' + msg + '\n');
};

function error(msg) {
  dump('PEP ERROR ' + msg + '\n');
};

function log(level, msg) {
  dump('PEP ' + level + ' ' + msg + '\n');
};
