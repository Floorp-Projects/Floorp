#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# Util.tst - simple regression tests for the Util.pm module


# $Revision: 1.2 $ 
# $Date: 2000/09/22 14:56:14 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/util.tst,v $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 


# Standard perl libraries

# Tinderbox libraries
use lib '#tinder_libdir#';

use Utils;




sub extract_digits_tst {

  $a = "kestes";
  ($a eq extract_digits($a)) && die();
  
  $a = "\n";
  ($a eq extract_digits($a)) && die();
  
  $a = "\0";
  ($a eq extract_digits($a)) && die();
  
  $a = "\t";
  ($a eq extract_digits($a)) && die();
  
  $a = "%";
  ($a eq extract_digits($a)) && die();
  



  $a = "123";
  ($a eq extract_digits($a)) || die();
  
  $a = "0";
  ($a eq extract_digits($a)) || die();

  $a = 123;
  ($a eq extract_digits($a)) || die();
  
  $a = 0;
  ($a eq extract_digits($a)) || die();



return 1;
}


sub extract_user_tst {

  # typical user
  $a = 'kestes';
  ($a eq extract_user($a)) || die();
  
  # typical mail address
  $a = 'kestes@staff.mail.com';
  ($a eq extract_user($a)) || die();
  
  # typical netscape CVS address
  $a = 'kestes%staff.mail.com';
  ($a eq extract_user($a)) || die();
  
  # mail address with 'decoration'
  $a = ' "leaf (Daniel Nunes)" <leaf@mozilla.org> ';
    ('leaf@mozilla.org' eq extract_user($a)) || die();
  
  # bad addresses
  $a = "\n";
  (length(extract_user($a))) && die();
  
  $a = ";";
  (length(extract_user($a))) && die();
  
  $a = "\0";
  (length(extract_user($a))) && die();
  
  return 1;
}


sub extract_html_chars_tst {


  $a = "<SCRIPT SRC='http://bad-site/badfile'></SCRIPT>";
  (length(extract_html_chars($a))) && die();
  
  $a = "<script>";
  (length(extract_html_chars($a))) && die();
  
  $a = "</script>";
  (length(extract_html_chars($a))) && die();
  
  $a = "<ScRiPT></ScRiPT>";
  (length(extract_html_chars($a))) && die();
  
  $a = "<SCRIPT></SCRIPT>";
  (length(extract_html_chars($a))) && die();

  $a = "<SCRIPT></SCRIPT>";
  (length(extract_html_chars($a))) && die();

  $a = "<OBJECT></OBJECT>";
  (length(extract_html_chars($a))) && die();

  $a = "<APPLET></APPLET>";
  (length(extract_html_chars($a))) && die();

  $a = "<EMBED></EMBED>";
  (length(extract_html_chars($a))) && die();

  $a = "\0";
  (length(extract_html_chars($a))) && die();

  # pass typical HTML
  $a = "<H2> title <H2><pre> typical \n \t HTML </pre>";
  ($a eq extract_html_chars($a)) || die();
  
  return 1;
}



sub is_time_valid_tst {

  is_time_valid(0) && die();
  is_time_valid(7) && die();
  is_time_valid(-7) && die();
  is_time_valid(10000000000) && die();

  is_time_valid('7') && die();
  is_time_valid('-7') && die();

  is_time_valid('a') && die();
  is_time_valid('Z') && die();

  is_time_valid("\t") && die();
  is_time_valid("\n")  && die();
  is_time_valid("\0")  && die();
  is_time_valid("%")  && die();
  is_time_valid("\$")  && die();
  is_time_valid(";")  && die();


  # this is a valid time
  #  DB<2> x $a=localtime(934395489)
  #        0  'Wed Aug 11 14:18:09 1999'

  is_time_valid('934395489') || die();
  is_time_valid(934395489) || die();

 
 return 1;
}



sub uniq_tst {

  ( 0 == scalar( uniq()) ) || die();
  ( 1 == scalar( uniq(1)) ) || die();
  ( 1 == scalar( uniq('1',1)) ) || die();
  ( 1 == scalar( uniq(1,1,)) ) || die();
  ( 3 == scalar( uniq(1,2,3)) ) || die();

  return 1;
}


sub max_tst {

 ( 1 == scalar( max(1,1,1,1,1,1,1)) ) || die();
 ( 7 == ( max(1,2,3,4,5,6,7)) ) || die();
 ( 7 == ( max(7)) ) || die();

 return 1;
}

sub min_tst {

 ( 1 == scalar( min(1,1,1,1,1,1,1)) ) || die();
 ( 1 == ( min(1,2,3,4,5,6,7)) ) || die();
 ( 7 == ( min(7)) ) || die();

 return 1;
}


sub median_tst {

 ( 1 == scalar( median(1,1,1,1,1,1,1)) ) || die();
 ( 4 == ( median(1,2,3, 4, 5,6,7)) ) || die();
 ( 1 == ( median(1,1,1,1,1,1,1000000)) ) || die();
 ( 1 == ( median(1,1,1,1,1,1,-1000000)) ) || die();

 return 1;
}


# --------------------main-------------------------

{
  set_static_vars();	

  extract_digits_tst();
  extract_user_tst();
  extract_html_chars_tst();
  is_time_valid_tst();

  uniq_tst();
  max_tst();
  min_tst();
  median_tst();

  exit 0;
}
