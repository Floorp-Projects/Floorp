#! /usr/bin/perl

# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): Akkana Peck.

#
# This is a collection of test files to guard against regressions
# in the Mozilla output system.
#

# Make sure . is in the path, so we can load the other shared libraries
$ENV{LD_LIBRARY_PATH} .= ":.";

$errmsg = "";

print "Testing simple html to html ...\n";
$status = system("TestOutput -i text/html -o text/html -f 0 -c OutTestData/simple.html OutTestData/simple.html");
if ($status != 0) {
  print "Simple html to html failed.\n";
  $errmsg = "$errmsg simple.html";
}

print "Testing simple copy case ...\n";
$status = system("TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/simplecopy.out OutTestData/simple.html");
if ($status != 0) {
  print "Simple copy test failed.\n";
  $errmsg = "$errmsg simplecopy.out";
}

print "Testing simple html to plaintext formatting ...\n";
$status = system("TestOutput -i text/html -o text/plain -f 34 -w 70 -c OutTestData/simplefmt.out OutTestData/simple.html");
if ($status != 0) {
  print("Simple formatting test failed.\n");
  $errmsg = "$errmsg simplefmt.out ";
}

print "Testing non-wrapped plaintext in preformatted mode ...\n";
$status = system("TestOutput -i text/html -o text/plain -f 16 -c OutTestData/plainnowrap.out OutTestData/plain.html");
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
$status = system("TestOutput -i text/html -o text/plain -f 2 -w 50 -c OutTestData/mailquote.out OutTestData/mailquote.html");
if ($status != 0) {
  print "Mail quoting test failed.\n";
  $errmsg = "$errmsg mailquote.out";
}

print "Testing format=flowed output ...\n";
$status = system("TestOutput -i text/html -o text/plain -f 66 -w 50 -c OutTestData/simplemail.out OutTestData/simplemail.html");
if ($status != 0) {
  print "Format=flowed test failed.\n";
  $errmsg = "$errmsg simplemail.out";
}

print "Testing conversion of XIF entities ...\n";
$status = system("TestOutput -i text/xif -o text/plain -c OutTestData/entityxif.out OutTestData/entityxif.xif");
if ($status != 0) {
  print "XIF entity conversion test failed.\n";
  $errmsg = "$errmsg entityxif.out";
}

print "Testing XIF to HTML ...\n";
$status = system("TestOutput -i text/xif -o text/html -c OutTestData/xifstuff.out OutTestData/xifstuff.xif");
if ($status != 0) {
  print "XIF to HTML conversion test failed.\n";
  $errmsg = "$errmsg xifstuff.out";
}

print "Testing HTML Table to Text ...\n";
$status = system("TestOutput -i text/html -o text/plain -f 2 -c OutTestData/htmltable.out OutTestData/htmltable.html");
if ($status != 0) {
  print "HTML Table to Plain text failed.\n";
  $errmsg = "$errmsg htmltable.out";
}

print "Testing XIF to plain with doctype (bug 28447) ...\n";
$status = system("TestOutput -i text/xif -o text/plain -f 2 -c OutTestData/xifdtplain.out OutTestData/doctype.xif");
if ($status != 0) {
  print "XIF to plain with doctype failed.\n";
  $errmsg = "$errmsg xifdtplain.out";
}

print "Testing XIF to html with doctype ...\n";
$status = system("TestOutput -i text/xif -o text/html -f 0 -c OutTestData/xifdthtml.out OutTestData/doctype.xif");
if ($status != 0) {
  print "XIF to html with doctype failed.\n";
  $errmsg = "$errmsg xifdthtml.out";
}

if ($errmsg ne "") {
  print "\nERROR: DOM CONVERSION TEST FAILED: $errmsg\n";
  exit 1
} else {
  print "DOM CONVERSION TESTS SUCCEEDED\n";
}
