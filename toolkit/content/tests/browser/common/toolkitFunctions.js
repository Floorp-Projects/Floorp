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
 * The Original Code is Mozilla XUL Toolkit Testing Code.
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
 * Provides a temporary save directory.
 *
 * @return
 *        nsIFile pointing to the new or existing directory.
 */
function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  return saveDir;
}

/**
 * Calls the provided save function while replacing the file picker component
 * with a mock implementation that returns a temporary file path and custom
 * filters, and the download component with an implementation that does not
 * depend on the download manager.
 *
 * @param aSaveFunction
 *        The function to call. This is usually the subject of the entire test
 *        being run.
 */
function callSaveWithMockObjects(aSaveFunction) {
  // Call the provided function while the mock object factories are in place and
  // ensure that, even in case of exceptions during the function's execution,
  // the mock object factories are unregistered before proceeding with the other
  // tests in the suite.
  mockFilePickerRegisterer.register();
  try {
    mockTransferForContinuingRegisterer.register();
    try {
      aSaveFunction();
    }
    finally {
      mockTransferForContinuingRegisterer.unregister();
    }
  }
  finally {
    mockFilePickerRegisterer.unregister();
  }
}

/**
 * Reads the contents of the provided short file (up to 1 MiB).
 *
 * @param aFile
 *        nsIFile object pointing to the file to be read.
 *
 * @return
 *        String containing the raw octets read from the file.
 */
function readShortFile(aFile) {
  var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(Ci.nsIFileInputStream);
  inputStream.init(aFile, -1, 0, 0);
  try {
    var scrInputStream = Cc["@mozilla.org/scriptableinputstream;1"].
                         createInstance(Ci.nsIScriptableInputStream);
    scrInputStream.init(inputStream);
    try {
      // Assume that the file is much shorter than 1 MiB.
      return scrInputStream.read(1048576);
    }
    finally {
      // Close the scriptable stream after reading, even if the operation
      // failed.
      scrInputStream.close();
    }
  }
  finally {
    // Close the stream after reading, if it is still open, even if the read
    // operation failed.
    inputStream.close();
  }
}
