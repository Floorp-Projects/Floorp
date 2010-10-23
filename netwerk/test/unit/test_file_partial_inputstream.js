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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mats Palmgren.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

// Test nsIPartialFileInputStream
// NOTE! These tests often use do_check_true(a == b) rather than
//       do_check_eq(a, b) to avoid outputting characters which confuse
//       the console

const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");
const PR_RDONLY = 0x1;  // see prio.h

// We need the profile directory so the test harness will clean up our test
// files.
do_get_profile();

var binary_test_file_name = "data/image.png";
var text_test_file_name = "test_file_partial_inputstream.js";
// This is a global variable since if it's passed as an argument stack traces
// become unreadable.
var test_file_data;

function run_test()
{
  // Binary tests
  let binaryFile = do_get_file(binary_test_file_name);
  let size = binaryFile.fileSize;
  // Want to make sure we're working with a large enough file
  dump("**** binary file size is: " + size + " ****\n");
  do_check_true(size > 65536);

  let binaryStream = new BinaryInputStream(new_file_input_stream(binaryFile));
  test_file_data = "";
  while ((avail = binaryStream.available()) > 0) {
    test_file_data += binaryStream.readBytes(avail);
  }
  do_check_eq(test_file_data.length, size);
  binaryStream.close();

  test_binary_portion(0, 10);
  test_binary_portion(0, 20000);
  test_binary_portion(0, size);
  test_binary_portion(20000, 10);
  test_binary_portion(20000, 20000);
  test_binary_portion(20000, size-20000);
  test_binary_portion(size-10, 10);
  test_binary_portion(size-20000, 20000);
  test_binary_portion(0, 0);
  test_binary_portion(20000, 0);
  test_binary_portion(size-1, 1);


  // Text-file tests
  let textFile = do_get_file(binary_test_file_name);
  size = textFile.fileSize;
  // Want to make sure we're working with a large enough file
  dump("**** text file size is: " + size + " ****\n");
  do_check_true(size > 7000);

  let textStream = new BinaryInputStream(new_file_input_stream(textFile));
  test_file_data = "";
  while ((avail = textStream.available()) > 0)
    test_file_data += textStream.readBytes(avail);
  do_check_eq(test_file_data.length, size);
  textStream.close();

  test_text_portion(0, 100);
  test_text_portion(0, size);
  test_text_portion(5000, 1000);
  test_text_portion(size-10, 10);
  test_text_portion(size-5000, 5000);
  test_text_portion(10, 0);
  test_text_portion(size-1, 1);

  // Test auto-closing files
  // Test behavior when *not* autoclosing
  let tempFile = create_temp_file("01234567890123456789");
  let tempInputStream = new_partial_file_input_stream(tempFile, 5, 10);
  tempInputStream.QueryInterface(Ci.nsILineInputStream);
  do_check_eq(read_line_stream(tempInputStream)[1], "5678901234");
  try {
    // This fails on some platforms
    tempFile.remove(false);
  }
  catch (ex) {
  }
  tempInputStream.QueryInterface(Ci.nsISeekableStream);
  tempInputStream.seek(SET, 1);
  do_check_eq(read_line_stream(tempInputStream)[1], "678901234");

  // Test removing the file when autoclosing
  tempFile = create_temp_file("01234567890123456789");
  tempInputStream = new_partial_file_input_stream(tempFile, 5, 10,
                                                  Ci.nsIFileInputStream.CLOSE_ON_EOF |
                                                  Ci.nsIFileInputStream.REOPEN_ON_REWIND);
  tempInputStream.QueryInterface(Ci.nsILineInputStream);
  do_check_eq(read_line_stream(tempInputStream)[1], "5678901234");
  tempFile.remove(false);
  tempInputStream.QueryInterface(Ci.nsISeekableStream);
  try {
    // The seek should reopen the file, which should fail.
    tempInputStream.seek(SET, 1);
    do_check_true(false);
  }
  catch (ex) {
  }

  // Test editing the file when autoclosing
  tempFile = create_temp_file("01234567890123456789");
  tempInputStream = new_partial_file_input_stream(tempFile, 5, 10,
                                                  Ci.nsIFileInputStream.CLOSE_ON_EOF |
                                                  Ci.nsIFileInputStream.REOPEN_ON_REWIND);
  tempInputStream.QueryInterface(Ci.nsILineInputStream);
  do_check_eq(read_line_stream(tempInputStream)[1], "5678901234");
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(tempFile, 0x02 | 0x08 | 0x20, // write, create, truncate
               0666, 0);
  let newData = "abcdefghijklmnopqrstuvwxyz";
  ostream.write(newData, newData.length);
  ostream.close();
  tempInputStream.QueryInterface(Ci.nsISeekableStream);
  tempInputStream.seek(SET, 1);
  do_check_eq(read_line_stream(tempInputStream)[1], newData.substr(6,9));

  // Test auto-delete and auto-close together
  tempFile = create_temp_file("01234567890123456789");
  tempInputStream = new_partial_file_input_stream(tempFile, 5, 10,
                                                  Ci.nsIFileInputStream.CLOSE_ON_EOF |
                                                  Ci.nsIFileInputStream.DELETE_ON_CLOSE);
  tempInputStream.QueryInterface(Ci.nsILineInputStream);
  do_check_eq(read_line_stream(tempInputStream)[1], "5678901234");
  do_check_false(tempFile.exists());
}

function test_binary_portion(start, length) {
  let subFile = create_temp_file(test_file_data.substr(start, length));

  let streamTests = [
    test_4k_read,
    test_max_read,
    test_seek,
    test_seek_then_read,
  ];

  for each(test in streamTests) {
    let fileStream = new_file_input_stream(subFile);
    let partialStream = new_partial_file_input_stream(do_get_file(binary_test_file_name),
                                                      start, length);
    test(fileStream, partialStream, length);
    fileStream.close();
    partialStream.close();
  }
}

function test_4k_read(fileStreamA, fileStreamB) {
  fileStreamA.QueryInterface(Ci.nsISeekableStream);
  fileStreamB.QueryInterface(Ci.nsISeekableStream);
  let streamA = new BinaryInputStream(fileStreamA);
  let streamB = new BinaryInputStream(fileStreamB);

  while(1) {
    do_check_eq(fileStreamA.tell(), fileStreamB.tell());

    let availA = streamA.available();
    let availB = streamB.available();
    do_check_eq(availA, availB);
    if (availA == 0)
      return;

    let readSize = availA > 4096 ? 4096 : availA;

    do_check_true(streamA.readBytes(readSize) ==
                  streamB.readBytes(readSize));
  }
}

function test_max_read(fileStreamA, fileStreamB) {
  fileStreamA.QueryInterface(Ci.nsISeekableStream);
  fileStreamB.QueryInterface(Ci.nsISeekableStream);
  let streamA = new BinaryInputStream(fileStreamA);
  let streamB = new BinaryInputStream(fileStreamB);

  while(1) {
    do_check_eq(fileStreamA.tell(), fileStreamB.tell());

    let availA = streamA.available();
    let availB = streamB.available();
    do_check_eq(availA, availB);
    if (availA == 0)
      return;

    do_check_true(streamA.readBytes(availA) ==
                  streamB.readBytes(availB));
  }
}

const SET = Ci.nsISeekableStream.NS_SEEK_SET;
const CUR = Ci.nsISeekableStream.NS_SEEK_CUR;
const END = Ci.nsISeekableStream.NS_SEEK_END;
function test_seek(dummy, partialFileStream, size) {
  // We can't test the "real" filestream here as our existing file streams
  // are very broken and allows searching past the end of the file.

  partialFileStream.QueryInterface(Ci.nsISeekableStream);

  tests = [
    [SET, 0],
    [SET, 5],
    [SET, 1000],
    [SET, size-10],
    [SET, size-5],
    [SET, size-1],
    [SET, size],
    [SET, size+10],
    [SET, 0],
    [CUR, 5],
    [CUR, -5],
    [SET, 5000],
    [CUR, -100],
    [CUR, 200],
    [CUR, -5000],
    [CUR, 5000],
    [CUR, size * 2],
    [SET, 1],
    [CUR, -1],
    [CUR, -1],
    [CUR, -1],
    [CUR, -1],
    [CUR, -1],
    [SET, size-1],
    [CUR, 1],
    [CUR, 1],
    [CUR, 1],
    [CUR, 1],
    [CUR, 1],
    [END, 0],
    [END, -1],
    [END, -5],
    [END, -1000],
    [END, -size+10],
    [END, -size+5],
    [END, -size+1],
    [END, -size],
    [END, -size-10],
    [END, 10],
    [CUR, 10],
    [CUR, 10],
    [CUR, 100],
    [CUR, 1000],
    [END, -1000],
    [CUR, 100],
    [CUR, 900],
    [CUR, 100],
    [CUR, 100],
  ];

  let pos = 0;
  for each(test in tests) {
    let didThrow = false;
    try {
      partialFileStream.seek(test[0], test[1]);
    }
    catch (ex) {
      didThrow = true;
    }

    let newPos = test[0] == SET ? test[1] :
                 test[0] == CUR ? pos + test[1] :
                 size + test[1];
    if (newPos > size || newPos < 0) {
      do_check_true(didThrow);
    }
    else {
      do_check_false(didThrow);
      pos = newPos;
    }

    do_check_eq(partialFileStream.tell(), pos);
    do_check_eq(partialFileStream.available(), size - pos);
  }
}

function test_seek_then_read(fileStreamA, fileStreamB, size) {
  // For now we only test seeking inside the file since our existing file
  // streams behave very strange when seeking to past the end of the file.
  if (size < 20000) {
    return;
  }

  fileStreamA.QueryInterface(Ci.nsISeekableStream);
  fileStreamB.QueryInterface(Ci.nsISeekableStream);
  let streamA = new BinaryInputStream(fileStreamA);
  let streamB = new BinaryInputStream(fileStreamB);

  let read = {};

  tests = [
    [SET, 0],
    [read, 1000],
    [read, 1000],
    [SET, 5],
    [read, 1000],
    [read, 5000],
    [CUR, 100],
    [read, 1000],
    [read, 5000],
    [CUR, -100],
    [read, 1000],
    [CUR, -100],
    [read, 5000],
    [END, -10],
    [read, 10],
    [END, -100],
    [read, 101],
    [CUR, -100],
    [read, 10],
    [SET, 0],
    [read, 20000],
    [read, 1],
    [read, 100],
  ];

  for each(test in tests) {
    if (test[0] === read) {
  
      let didThrowA = false;
      let didThrowB = false;

      let bytesA, bytesB;
      try {
        bytesA = streamA.readBytes(test[1]);
      }
      catch (ex) {
        didThrowA = true;
      }
      try {
        bytesB = streamB.readBytes(test[1]);
      }
      catch (ex) {
        didThrowB = true;
      }
  
      do_check_eq(didThrowA, didThrowB);
      do_check_true(bytesA == bytesB);
    }
    else {
      fileStreamA.seek(test[0], test[1]);
      fileStreamB.seek(test[0], test[1]);
    }
    do_check_eq(fileStreamA.tell(), fileStreamB.tell());
    do_check_eq(fileStreamA.available(), fileStreamB.available());
  }
}

function test_text_portion(start, length) {
  let subFile = create_temp_file(test_file_data.substr(start, length));

  let streamTests = [
    test_readline,
    test_seek_then_readline,
  ];

  for each(test in streamTests) {
    let fileStream = new_file_input_stream(subFile)
                     .QueryInterface(Ci.nsILineInputStream);
    let partialStream = new_partial_file_input_stream(do_get_file(binary_test_file_name),
                                                      start, length)
                        .QueryInterface(Ci.nsILineInputStream);
    test(fileStream, partialStream, length);
    fileStream.close();
    partialStream.close();
  }
}

function test_readline(fileStreamA, fileStreamB)
{
  let moreA = true, moreB;
  while(moreA) {
    let lineA, lineB;
    [moreA, lineA] = read_line_stream(fileStreamA);
    [moreB, lineB] = read_line_stream(fileStreamB);
    do_check_eq(moreA, moreB);
    do_check_true(lineA.value == lineB.value);
  }
}

function test_seek_then_readline(fileStreamA, fileStreamB, size) {
  // For now we only test seeking inside the file since our existing file
  // streams behave very strange when seeking to past the end of the file.
  if (size < 100) {
    return;
  }

  fileStreamA.QueryInterface(Ci.nsISeekableStream);
  fileStreamB.QueryInterface(Ci.nsISeekableStream);

  let read = {};

  tests = [
    [SET, 0],
    [read, 5],
    [read, 5],
    [SET, 5],
    [read, 5],
    [read, 15],
    [CUR, 100],
    [read, 5],
    [read, 15],
    [CUR, -100],
    [read, 5],
    [CUR, -100],
    [read, 25],
    [END, -10],
    [read, 1],
    [END, -50],
    [read, 30],
    [read, 1],
    [read, 1],
    [CUR, -100],
    [read, 1],
    [SET, 0],
    [read, 10000],
    [read, 1],
    [read, 1],
    [SET, 0],
    [read, 1],
  ];

  for each(test in tests) {
    if (test[0] === read) {

      for (let i = 0; i < test[1]; ++i) {
        let didThrowA = false;
        let didThrowB = false;

        let lineA, lineB, moreA, moreB;
        try {
          [moreA, lineA] = read_line_stream(fileStreamA);
        }
        catch (ex) {
          didThrowA = true;
        }
        try {
          [moreB, lineB] = read_line_stream(fileStreamB);
        }
        catch (ex) {
          didThrowB = true;
        }

        do_check_eq(didThrowA, didThrowB);
        do_check_eq(moreA, moreB);
        do_check_true(lineA == lineB);
        do_check_eq(fileStreamA.tell(), fileStreamB.tell());
        do_check_eq(fileStreamA.available(), fileStreamB.available());
        if (!moreA)
          break;
      }
    }
    else {
      if (!(test[0] == CUR && (test[1] > fileStreamA.available() ||
                               test[1] < -fileStreamA.tell()))) {
        fileStreamA.seek(test[0], test[1]);
        fileStreamB.seek(test[0], test[1]);
        do_check_eq(fileStreamA.tell(), fileStreamB.tell());
        do_check_eq(fileStreamA.available(), fileStreamB.available());
      }
    }
  }
}

function read_line_stream(stream) {
  let line = {};
  let more = stream.readLine(line);
  return [more, line.value];
}

function new_file_input_stream(file) {
  var stream =
      Cc["@mozilla.org/network/file-input-stream;1"]
      .createInstance(Ci.nsIFileInputStream);
  stream.init(file, PR_RDONLY, 0, 0);
  return stream.QueryInterface(Ci.nsIInputStream);
}

function new_partial_file_input_stream(file, start, length, flags) {
  var stream =
      Cc["@mozilla.org/network/partial-file-input-stream;1"]
      .createInstance(Ci.nsIPartialFileInputStream);
  stream.init(file, start, length, PR_RDONLY, 0, flags || 0);
  return stream.QueryInterface(Ci.nsIInputStream);
}

function create_temp_file(data) {
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("ProfD", Ci.nsIFile);
  file.append("fileinputstream-test-file.tmp");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, 0x02 | 0x08 | 0x20, // write, create, truncate
               0666, 0);
  do_check_eq(ostream.write(data, data.length), data.length);
  ostream.close();

  return file;
}
