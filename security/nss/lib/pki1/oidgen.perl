#!perl
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
$cvs_id = '@(#) $RCSfile: oidgen.perl,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:16:22 $ $Name:  $';

$count = -1;
while(<>) {
  s/^((?:[^"#]+|"[^"]*")*)(\s*#.*$)/$1/;
  next if (/^\s*$/);

  /^([\S]+)\s+([^"][\S]*|"[^"]*")/;
  $name = $1;
  $value = $2;
  # This is certainly not the best way to dequote the data.
  $value =~ s/"//g;

  if( $name =~ "OID" ) {
    $count++;
    $x[$count]{$name} = $value;
    $enc = encodeoid($value);
    $x[$count]{" encoding"} = escapeoid($enc);
    $x[$count]{" encoding length"} = length($enc);
  } else {
    if( $count < 0 ) {
      $g{$name} = $value;
    } else {
      $x[$count]{$name} = $value;
    }
  }
}

# dodump();
doprint();

sub dodump {
for( $i = 0; $i <= $count; $i++ ) {
  print "number $i:\n";
  %y = %{$x[$i]};
  while(($n,$v) = each(%y)) {
    print "\t$n ==> $v\n";
  }
}
}

sub doprint {
open(CFILE, ">oiddata.c") || die "Can't open oiddata.c: $!"; 
open(HFILE, ">oiddata.h") || die "Can't open oiddata.h: $!";

print CFILE <<EOD
/* THIS IS A GENERATED FILE */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifdef DEBUG
static const char CVS_ID[] = "$g{CVS_ID} ; $cvs_id";
#endif /* DEBUG */

#ifndef PKI1T_H
#include "pki1t.h"
#endif /* PKI1T_H */

const NSSOID nss_builtin_oids[] = {
EOD
    ;

for( $i = 0; $i <= $count; $i++ ) {
  %y = %{$x[$i]};
  print CFILE "  {\n";
  print CFILE "#ifdef DEBUG\n";
  print CFILE "    \"$y{TAG}\",\n";
  print CFILE "    \"$y{EXPL}\",\n";
  print CFILE "#endif /* DEBUG */\n";
  print CFILE "    { \"", $y{" encoding"};
  print CFILE "\", ", $y{" encoding length"}, " }\n";

  if( $i == $count ) {
    print CFILE "  }\n";
  } else {
    print CFILE "  },\n";
  }
}

print CFILE "};\n\n";

print CFILE "const PRUint32 nss_builtin_oid_count = ", ($count+1), ";\n\n";

for( $i = 0; $i <= $count; $i++ ) {
  %y = %{$x[$i]};
  if( defined($y{NAME}) ) {
    print CFILE "const NSSOID *$y{NAME} = (NSSOID *)&nss_builtin_oids[$i];\n";
  }
}

print CFILE "\n";

$attrcount = -1;
for( $i = 0; $i <= $count; $i++ ) {
  %y = %{$x[$i]};
  if( defined($y{ATTR}) ) {
    if( defined($y{NAME}) ) {
      $attrcount++;
      $attr[$attrcount]{ATTR} = $y{ATTR};
      $attr[$attrcount]{NAME} = $y{NAME};
    } else {
      warn "Attribute $y{ATTR} has no name, and will be omitted!";
    }
  }
}

print CFILE "const nssAttributeTypeAliasTable nss_attribute_type_aliases[] = {\n";

for( $i = 0; $i <= $attrcount; $i++ ) {
  %y = %{$attr[$i]};
  print CFILE "  {\n";
  print CFILE "    \"$y{ATTR}\",\n";
  print CFILE "    &$y{NAME}\n";

  if( $i == $attrcount ) {
    print CFILE "  }\n";
  } else {
    print CFILE "  },\n";
  }
}

print CFILE "};\n\n";

print CFILE "const PRUint32 nss_attribute_type_alias_count = ", ($attrcount+1), ";\n\n";

print HFILE <<EOD
/* THIS IS A GENERATED FILE */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef OIDDATA_H
#define OIDDATA_H

#ifdef DEBUG
static const char OIDDATA_CVS_ID[] = "$g{CVS_ID} ; $cvs_id";
#endif /* DEBUG */

#ifndef NSSPKI1T_H
#include "nsspki1t.h"
#endif /* NSSPKI1T_H */

extern const NSSOID nss_builtin_oids[];
extern const PRUint32 nss_builtin_oid_count;

/*extern const nssAttributeTypeAliasTable nss_attribute_type_aliases[];*/
/*extern const PRUint32 nss_attribute_type_alias_count;*/

EOD
    ;

for( $i = 0; $i <= $count; $i++ ) {
  %y = %{$x[$i]};
  if( defined($y{NAME}) ) {
    print HFILE "extern const NSSOID *$y{NAME};\n";
  }
}

print HFILE <<EOD

#endif /* OIDDATA_H */
EOD
    ;

close CFILE;
close HFILE;
}

sub encodenum {
    my $v = $_[0];
    my @d;
    my $i;
    my $rv = "";

    while( $v > 128 ) {
        push @d, ($v % 128);
        $v /= 128;
    };
    push @d, ($v%128);

    for( $i = @d-1; $i > 0; $i-- ) {
        $rv = $rv . chr(128 + $d[$i]);
    }

    $rv = $rv . chr($d[0]);

    return $rv;
}

sub encodeoid {
    my @o = split(/\./, $_[0]);
    my $rv = "";
    my $i;

    if( @o < 2 ) {
        # NSS's special "illegal" encoding
        return chr(128) . encodenum($o[0]);
    }

    $rv = encodenum($o[0] * 40 + $o[1]);
    shift @o; shift @o;

    foreach $i (@o) {
        $rv = $rv . encodenum($i);
    }

    return $rv;
}

sub escapeoid {
  my @v = unpack("C*", $_[0]);
  my $a;
  my $rv = "";

  foreach $a (@v) {
    $rv = $rv . sprintf("\\x%02x", $a);
  }

  return $rv;
}
