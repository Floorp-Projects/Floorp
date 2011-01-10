/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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

 /**
  * Test preventive maintenance checkAndFixDatabase.
  */

// Include PlacesDBUtils module.
Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

function run_test() {
  do_test_pending();
  PlacesDBUtils.checkAndFixDatabase(function(aLog) {
    let sections = [];
    let positives = [];
    let negatives = [];
    let infos = [];

    aLog.forEach(function (aMsg) {
      print (aMsg);
      switch (aMsg.substr(0, 1)) {
        case "+":
          positives.push(aMsg);
          break;
        case "-":
          negatives.push(aMsg);
          break;
        case ">":
          sections.push(aMsg);
          break;
        default:
          infos.push(aMsg);
      }
    });

    print("Check that we have run all sections.");
    do_check_eq(sections.length, 5);
    print("Check that we have no negatives.");
    do_check_false(!!negatives.length);
    print("Check that we have positives.");
    do_check_true(!!positives.length);
    print("Check that we have info.");
    do_check_true(!!infos.length);

    do_test_finished();
  });
}
