#!perl -w
# 
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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
my $cvs_id = '@(#) $RCSfile: certdata.perl,v $ $Revision: 1.11 $ $Date: 2006/07/17 16:50:45 $';
use strict;

my %constants;
my $count = 0;
my $o;
my @objects = ();
my @objsize;
my $cvsid;

$constants{CKO_DATA} = "static const CK_OBJECT_CLASS cko_data = CKO_DATA;\n";
$constants{CK_TRUE} = "static const CK_BBOOL ck_true = CK_TRUE;\n";
$constants{CK_FALSE} = "static const CK_BBOOL ck_false = CK_FALSE;\n";

while(<>) {
  my @fields = ();
  my $size;

  s/^((?:[^"#]+|"[^"]*")*)(\s*#.*$)/$1/;
  next if (/^\s*$/);

  if( /(^CVS_ID\s+)(.*)/ ) {
#    print "The CVS ID is $2\n";
    $cvsid = $2 . "\"; $cvs_id\"";
    my $scratch = $cvsid;
    $size = 1 + $scratch =~ s/[^"\n]//g;
    @{$objects[0][0]} = ( "CKA_CLASS", "&cko_data", "sizeof(CK_OBJECT_CLASS)" );
    @{$objects[0][1]} = ( "CKA_TOKEN", "&ck_true", "sizeof(CK_BBOOL)" );
    @{$objects[0][2]} = ( "CKA_PRIVATE", "&ck_false", "sizeof(CK_BBOOL)" );
    @{$objects[0][3]} = ( "CKA_MODIFIABLE", "&ck_false", "sizeof(CK_BBOOL)" );
    @{$objects[0][4]} = ( "CKA_LABEL", "\"CVS ID\"", "7" );
    @{$objects[0][5]} = ( "CKA_APPLICATION", "\"NSS\"", "4" );
    @{$objects[0][6]} = ( "CKA_VALUE", $cvsid, "$size" );
    $objsize[0] = 7;
    next;
  }

  # This was taken from the perl faq #4.
  my $text = $_;
  push(@fields, $+) while $text =~ m{
      "([^\"\\]*(?:\\.[^\"\\]*)*)"\s?  # groups the phrase inside the quotes
    | ([^\s]+)\s?
    | \s
  }gx;
  push(@fields, undef) if substr($text,-1,1) eq '\s';

  if( $fields[0] =~ /BEGINDATA/ ) {
    next;
  }

  if( $fields[1] =~ /MULTILINE/ ) {
    $fields[2] = "";
    while(<>) {
      last if /END/;
      chomp;
      $fields[2] .= "\"$_\"\n";
    }
  }

  if( $fields[1] =~ /UTF8/ ) {
    if( $fields[2] =~ /^"/ ) {
      ;
    } else {
      $fields[2] = "\"" . $fields[2] . "\"";
    }

    my $scratch = $fields[2];
    $size = $scratch =~ s/[^"\n]//g; # should supposedly handle multilines, too..
    $size += 1; # null terminate
  }

  if( $fields[1] =~ /OCTAL/ ) {
    if( $fields[2] =~ /^"/ ) {
      ;
    } else {
      $fields[2] = "\"" . $fields[2] . "\"";
    }

    my $scratch = $fields[2];
    $size = $scratch =~ tr/\\//;
    # no null termination
  }

  if( $fields[1] =~ /^CK_/ ) {
    my $lcv = $fields[2];
    $lcv =~ tr/A-Z/a-z/;
    if( !defined($constants{$fields[2]}) ) {
      $constants{$fields[2]} = "static const $fields[1] $lcv = $fields[2];\n";
    }
    
    $size = "sizeof($fields[1])";
    $fields[2] = "&$lcv";
  }

  if( $fields[0] =~ /CKA_CLASS/ ) {
    $count++;
    $objsize[$count] = 0;
  }

  @{$objects[$count][$objsize[$count]++]} = ( "$fields[0]", $fields[2], "$size" );

 # print "$fields[0] | $fields[1] | $size | $fields[2]\n";
}

doprint();

sub dudump {
my $i;
for( $i = 0; $i <= $count; $i++ ) {
  print "\n";
  $o = $objects[$i];
  my @ob = @{$o};
  my $l;
  my $j;
  for( $j = 0; $j < @ob; $j++ ) {
    $l = $ob[$j];
    my @a = @{$l};
    print "$a[0] ! $a[1] ! $a[2]\n";
  }
}

}

sub doprint {
my $i;

open(CFILE, ">certdata.c") || die "Can't open certdata.c: $!";

print CFILE <<EOD
/* THIS IS A GENERATED FILE */
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
#ifdef DEBUG
static const char CVS_ID[] = $cvsid;
#endif /* DEBUG */

#ifndef BUILTINS_H
#include "builtins.h"
#endif /* BUILTINS_H */

EOD
    ;

foreach $b (sort values(%constants)) {
  print CFILE $b;
}

for( $i = 0; $i <= $count; $i++ ) {
  if( 0 == $i ) {
    print CFILE "#ifdef DEBUG\n";
  }

  print CFILE "static const CK_ATTRIBUTE_TYPE nss_builtins_types_$i [] = {\n";
  $o = $objects[$i];
 # print STDOUT "type $i object $o \n";
  my @ob = @{$o};
  my $j;
  for( $j = 0; $j < @ob; $j++ ) {
    my $l = $ob[$j];
    my @a = @{$l};
    print CFILE " $a[0]";
    if( $j+1 != @ob ) {
      print CFILE ", ";
    }
  }
  print CFILE "\n};\n";

  if( 0 == $i ) {
    print CFILE "#endif /* DEBUG */\n";
  }
}

for( $i = 0; $i <= $count; $i++ ) {
  if( 0 == $i ) {
    print CFILE "#ifdef DEBUG\n";
  }

  print CFILE "static const NSSItem nss_builtins_items_$i [] = {\n";
  $o = $objects[$i];
  my @ob = @{$o};
  my $j;
  for( $j = 0; $j < @ob; $j++ ) {
    my $l = $ob[$j];
    my @a = @{$l};
    print CFILE "  { (void *)$a[1], (PRUint32)$a[2] }";
    if( $j+1 != @ob ) {
      print CFILE ",\n";
    } else {
      print CFILE "\n";
    }
  }
  print CFILE "};\n";

  if( 0 == $i ) {
    print CFILE "#endif /* DEBUG */\n";
  }
}

print CFILE "\nPR_IMPLEMENT_DATA(builtinsInternalObject)\n";
print CFILE "nss_builtins_data[] = {\n";

for( $i = 0; $i <= $count; $i++ ) {

  if( 0 == $i ) {
    print CFILE "#ifdef DEBUG\n";
  }

  print CFILE "  { $objsize[$i], nss_builtins_types_$i, nss_builtins_items_$i, {NULL} }";

  if( $i == $count ) {
    print CFILE "\n";
  } else {
    print CFILE ",\n";
  }

  if( 0 == $i ) {
    print CFILE "#endif /* DEBUG */\n";
  }
}

print CFILE "};\n";

print CFILE "PR_IMPLEMENT_DATA(const PRUint32)\n";
print CFILE "#ifdef DEBUG\n";
print CFILE "  nss_builtins_nObjects = $count+1;\n";
print CFILE "#else\n";
print CFILE "  nss_builtins_nObjects = $count;\n";
print CFILE "#endif /* DEBUG */\n";
}
