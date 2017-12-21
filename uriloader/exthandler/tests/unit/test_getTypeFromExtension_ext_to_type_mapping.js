/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  } catch (e) {
    if (!(e instanceof Ci.nsIException) ||
        e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw e;
    }
    // This is an expected exception, thrown if the type can't be determined.
    // Any other exception would cause the test to fail.
  }

  // Add a temporary category entry mapping the extension to the MIME type.
  categoryManager.addCategoryEntry("ext-to-type-mapping", kTestExtension,
                                   kTestMimeType, false, true);

  // Check that the mapping is recognized in the simple case.
  var type = mimeService.getTypeFromExtension(kTestExtension);
  Assert.equal(type, kTestMimeType);

  // Check that the mapping is recognized even if the extension has mixed case.
  type = mimeService.getTypeFromExtension(kTestExtensionMixedCase);
  Assert.equal(type, kTestMimeType);

  // Clean up after ourselves.
  categoryManager.deleteCategoryEntry("ext-to-type-mapping", kTestExtension, false);
}
