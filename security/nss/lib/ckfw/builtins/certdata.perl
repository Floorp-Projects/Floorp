#!perl -w
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
my $cvs_id = '@(#) $RCSfile: certdata.perl,v $ $Revision: 1.16 $ $Date: 2012/11/30 02:40:52 $';
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

    my $scratch = eval($fields[2]);

    $size = length($scratch) + 1; # null terminate
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

print <<EOD
/* THIS IS A GENERATED FILE */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef DEBUG
static const char CVS_ID[] = $cvsid;
#endif /* DEBUG */

#ifndef BUILTINS_H
#include "builtins.h"
#endif /* BUILTINS_H */

EOD
    ;

foreach $b (sort values(%constants)) {
  print $b;
}

for( $i = 0; $i <= $count; $i++ ) {
  if( 0 == $i ) {
    print "#ifdef DEBUG\n";
  }

  print "static const CK_ATTRIBUTE_TYPE nss_builtins_types_$i [] = {\n";
  $o = $objects[$i];
  my @ob = @{$o};
  my $j;
  for( $j = 0; $j < @ob; $j++ ) {
    my $l = $ob[$j];
    my @a = @{$l};
    print " $a[0]";
    if( $j+1 != @ob ) {
      print ", ";
    }
  }
  print "\n};\n";

  if( 0 == $i ) {
    print "#endif /* DEBUG */\n";
  }
}

for( $i = 0; $i <= $count; $i++ ) {
  if( 0 == $i ) {
    print "#ifdef DEBUG\n";
  }

  print "static const NSSItem nss_builtins_items_$i [] = {\n";
  $o = $objects[$i];
  my @ob = @{$o};
  my $j;
  for( $j = 0; $j < @ob; $j++ ) {
    my $l = $ob[$j];
    my @a = @{$l};
    print "  { (void *)$a[1], (PRUint32)$a[2] }";
    if( $j+1 != @ob ) {
      print ",\n";
    } else {
      print "\n";
    }
  }
  print "};\n";

  if( 0 == $i ) {
    print "#endif /* DEBUG */\n";
  }
}

print "\nbuiltinsInternalObject\n";
print "nss_builtins_data[] = {\n";

for( $i = 0; $i <= $count; $i++ ) {

  if( 0 == $i ) {
    print "#ifdef DEBUG\n";
  }

  print "  { $objsize[$i], nss_builtins_types_$i, nss_builtins_items_$i, {NULL} }";

  if( $i == $count ) {
    print "\n";
  } else {
    print ",\n";
  }

  if( 0 == $i ) {
    print "#endif /* DEBUG */\n";
  }
}

print "};\n";

print "const PRUint32\n";
print "#ifdef DEBUG\n";
print "  nss_builtins_nObjects = $count+1;\n";
print "#else\n";
print "  nss_builtins_nObjects = $count;\n";
print "#endif /* DEBUG */\n";
}
