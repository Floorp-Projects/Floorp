#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# timestamp.tst - test the timestamp code in processmail.


# $Revision: 1.2 $ 
# $Date: 2003/08/17 00:57:35 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/timestamp.tst,v $ 
# $Name:  $ 


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


# Standard perl libraries
use File::Basename;
use Sys::Hostname;
use File::stat;
use Getopt::Long;

# Tinderbox libraries
use lib '#tinder_libdir#';

use TinderConfig;
use TreeData;
use HTMLPopUp;
use VCDisplay;
use Error_Parse;
use Utils;
use Persistence;
use TinderDB::Build;



sub write_testlog {

  my ($tree, $testlog, $starttime, $status,) = @_;

  $timenow = $starttime + (15*60);

$test_headers=<<EOF;
Content-Length: -1346
Received: by staff.mail.com (mbox kestes)
 (with Cubic Circle's cucipop (v1.22 1998/04/11) Wed Aug 11 11:11:37 1999)
X-From_: fetchmail-friends-request\@ccil.org  Wed Aug 11 11:07:18 1999
Return-Path: <fetchmail-friends-request\@ccil.org>
Received: from locke.ccil.org (slist\@locke.ccil.org [192.190.237.102])
	by staff.mail.com (8.9.1/8.9.1) with ESMTP id LAA29008
	for <kestes\@inamecorp.com>; Wed, 11 Aug 1999 11:07:18 -0400 (EDT)
Received: (from slist\@localhost)
	by locke.ccil.org (8.8.5/8.8.5) id LAA07287;
	Wed, 11 Aug 1999 11:31:31 -0400 (EDT)
X-Authentication-Warning: locke.ccil.org: slist set sender to fetchmail-friends-request\@ccil.org using -f
Message-Id: <199908110807.QAA01429\@possum.os2.ami.com.au>
Precedence: list
From: John Summerfield <summer\@OS2.ami.com.au>
To: fetchmail-friends\@ccil.org
Subject: fetchmail doesn't quit
Date: Wed, 11 Aug 1999 16:07:16 +0800

tinderbox: tree: $tree
tinderbox: administrator: testadmin\@mozilla.org
tinderbox: starttime: $starttime
tinderbox: timenow: $timenow
tinderbox: status: $status
tinderbox: buildname: worms-SunOS-sparc-5.6-Depend-apprunner
tinderbox: errorparser: unix
tinderbox: END

EOF

  ;

  open(FILE, ">$testlog") ||
    die("Could not open: $testlog. $!\n");
  
  print FILE $test_headers;
  
  close(FILE) ||
    die("Could not close: $testlog. $!\n");
  
  
  return 1;
}



# --------- Main Function ----------------

{
  $tree = 'Project_A';
  $testlog = "/tmp/testlog";
  $test_dir = dirname($0);
  $processmail = "$test_dir/../bin/processmail";
  my $stamp_file = FileStructure::get_filename($tree, 'update_time_stamp');
  
  # do not check for errors, file may not exists.
  unlink($stamp_file);
  

  # we adjust the starttime, but keep the runtime fixed.
  
  $time = time();
  $tests = [
            ['', 'success', "Variable: 'tinderbox: starttime' not set."],
            ['abcde', 'success', "Variable: 'tinderbox: starttime:', is not of the form"],
            [$time - (60*70), 'success', ''],
            [$time, 'success', ''],
            [$time, 'success', 'time is going backwards'],
            [$time + 1, 'success', ''],
            [$time + 2, 'foobaz', "Variable: 'tinderbox: status:' must be one of"],
            [$time + 3, '', "Variable: 'tinderbox: status:' not set."],
            [$time + (60*60*60), 'success', 'your clock is set incorrectly'],
           ];
  
  foreach $test_ref (@{$tests}) {
    $testno++;
    my ($start_time, $status, $expected_results) =  @{$test_ref};
    write_testlog($tree, $testlog, $start_time, $status);
    $results = `$processmail $testlog 2>&1`;
    if ($expected_results) {
      ($results =~ m/$expected_results/) ||
        die ("Test no: $testno Failed\n".
             " expected: $expected_results\n".
             " got: $results\n\n");
    } else {
      ($results eq '') ||
        die ("Test no: $testno Failed\n".
             " expected: $expected_results\n".
             " got: $results\n\n");
    }
  }
  
  exit 0;
}
