/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Automated Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * Test for bug 508030 <https://bugzilla.mozilla.org/show_bug.cgi?id=508030>:
 * nsIMIMEService.getTypeFromExtension fails to find a match in the
 * "ext-to-type-mapping" category if the provided extension is not lowercase.
 */
function run_test() {
  // --- Common services ---

  const mimeService = Cc["@mozilla.org/mime;1"].
                      getService(Ci.nsIMIMEService);

  const categoryManager = Cc["@mozilla.org/categorymanager;1"].
                          getService(Ci.nsICategoryManager);

  // --- Test procedure ---

  const kTestExtension          = "testextension";
  const kTestExtensionMixedCase = "testExtensIon";
  const kTestMimeType           = "application/x-testextension";

  // Ensure that the test extension is not initially recognized by the operating
  // system or the "ext-to-type-mapping" category.
  try {
    // Try and get the MIME type associated with the extension.
    mimeService.getTypeFromExtension(kTestExtension);
    // The line above should have thrown an exception.
    do_throw("nsIMIMEService.getTypeFromExtension succeeded unexpectedly");
  } catch (e if (e instanceof Ci.nsIException &&
                 e.result == Cr.NS_ERROR_NOT_AVAILABLE)) {
    // This is an expected exception, thrown if the type can't be determined.
    // Any other exception would cause the test to fail.
  }

  // Add a temporary category entry mapping the extension to the MIME type.
  categoryManager.addCategoryEntry("ext-to-type-mapping", kTestExtension,
                                   kTestMimeType, false, true);

  // Check that the mapping is recognized in the simple case.
  var type = mimeService.getTypeFromExtension(kTestExtension);
  do_check_eq(type, kTestMimeType);

  // Check that the mapping is recognized even if the extension has mixed case.
  type = mimeService.getTypeFromExtension(kTestExtensionMixedCase);
  do_check_eq(type, kTestMimeType);

  // Clean up after ourselves.
  categoryManager.deleteCategoryEntry("ext-to-type-mapping", kTestExtension, false);
}
