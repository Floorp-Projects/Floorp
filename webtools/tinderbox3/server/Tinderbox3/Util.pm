# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
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
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

package Tinderbox3::Util;

use strict;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(escape_html escape_js escape_url);

sub escape_html {
  my ($str) = @_;
  $str =~ s/>/&gt;/g;
  $str =~ s/</&lt;/g;
  $str =~ s/'/&apos;/g;
  $str =~ s/"/&quot;/g;
  die if $str =~ /\n/;
  return $str;
}

sub escape_js {
  my ($str) = @_;
  $str =~ s/(['"\\])/\\$1/g;
  $str =~ s/(\r?)\n/\\n/g;
  return $str;
}

sub escape_url {
  my ($str) = @_;
  $str =~ s/ /+/g;
  $str =~ s/([%&])/sprintf('%%%x', ord($1))/eg;
  return $str;
}


1
