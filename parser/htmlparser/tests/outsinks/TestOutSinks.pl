#! /usr/bin/perl

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Akkana Peck.
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# This is a collection of test files to guard against regressions
# in the Mozilla output system.
# Documentation on the tests is available at:
# http://www.mozilla.org/editor/serializer-tests.html
#

# Make sure . is in the path, so we can load the other shared libraries
$ENV{LD_LIBRARY_PATH} .= ":.";

$errmsg = "";

# print "Testing simple html to html ...\n";
# $status = system("./TestOutput -i text/html -o text/html -f 0 -c OutTestData/simple.html OutTestData/simple.html");
# if ($status != 0) {
#   print "Simple html to html failed.\n";
#   $errmsg = "$errmsg simple.html";
# }

print "Testing simple copy case ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/simplecopy.out OutTestData/simple.html");
if ($status != 0) {
  print "Simple copy test failed.\n";
  $errmsg = "$errmsg simplecopy.out";
}

print "Testing simple formatted copy case ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 2 -w 0 -c OutTestData/simplecopy-formatted.out OutTestData/simple.html");
if ($status != 0) {
  print "Simple formatted copy test failed.\n";
  $errmsg = "$errmsg simplecopy-formatted.out";
}

print "Testing simple html to plaintext formatting ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 34 -w 70 -c OutTestData/simplefmt.out OutTestData/simple.html");
if ($status != 0) {
  print("Simple formatting test failed.\n");
  $errmsg = "$errmsg simplefmt.out ";
}

print "Testing non-wrapped plaintext in preformatted mode ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 16 -c OutTestData/plainnowrap.out OutTestData/plain.html");
if ($status != 0) {
  print "Non-wrapped plaintext test failed.\n";
  $errmsg = "$errmsg plainnowrap.out";
}

# print "Testing wrapped and formatted plaintext ...\n";
# $status = system("TestOutput -i text/html -o text/plain -f 32 -c OutTestData/plainwrap.out OutTestData/plain.html");
# if ($status != 0) {
#   print "Wrapped plaintext test failed.\n";
#   $errmsg = "$errmsg plainwrap.out";
# }

print "Testing mail quoting ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 2 -w 50 -c OutTestData/mailquote.out OutTestData/mailquote.html");
if ($status != 0) {
  print "Mail quoting test failed.\n";
  $errmsg = "$errmsg mailquote.out";
}

print "Testing misc. HTML output with format=flowed ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 2 -w 50 -c OutTestData/mischtml.out OutTestData/mischtml.html");
if ($status != 0) {
  print "Misc. HTML with format=flowed test failed.\n";
  $errmsg = "$errmsg mischtml.out";
}

print "Testing format=flowed output ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 66 -w 50 -c OutTestData/simplemail.out OutTestData/simplemail.html");
if ($status != 0) {
  print "Format=flowed test failed.\n";
  $errmsg = "$errmsg simplemail.out";
}

print "Testing HTML Table to Text ...\n";
$status = system("./TestOutput -i text/html -o text/plain -f 2 -c OutTestData/htmltable.out OutTestData/htmltable.html");
if ($status != 0) {
  print "HTML Table to Plain text failed.\n";
  $errmsg = "$errmsg htmltable.out";
}

if ($errmsg ne "") {
  print "\nTEST-UNEXPECTED-FAIL | $errmsg\n";
  exit 1
} else {
  print "\nTEST-PASS | TestOutSink\n";
}
