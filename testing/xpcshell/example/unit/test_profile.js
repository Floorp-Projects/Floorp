/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  let profd = do_get_profile();
  do_check_true(profd.exists());
  do_check_true(profd.isDirectory());
  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let profd2 = dirSvc.get("ProfD", Components.interfaces.nsILocalFile);
  do_check_true(profd2.exists());
  do_check_true(profd2.isDirectory());
  // make sure we got the same thing back...
  do_check_true(profd.equals(profd2));
}
