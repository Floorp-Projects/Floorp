# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code are the Bugzilla Tests.
# 
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are 
# Copyright (C) 2001 Zach Lipton.  All
# Rights Reserved.
# 
# Contributor(s): Zach Lipton <zach@zachlipton.com>
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

package Support::Files;

@additional_files = ('syncshadowdb','processmail');
@exclude_files    = ('importxml.pl');

$file = '*';
@files = glob($file);

sub isTestingFile {
  my ($file) = @_;
  my $exclude;
  foreach $exclude (@exclude_files) {
        if ($file eq $exclude) { return undef; } # get rid of excluded files.
  }

  if ($file =~ /\.cgi$|\.pl$|\.pm$/) {
    return 1;
  }
  my $additional;
  foreach $additional (@additional_files) {
    if ($file eq $additional) { return 1; }
  }
  return undef;
}

foreach $currentfile (@files) {
        if (isTestingFile($currentfile)) {
                push(@testitems,$currentfile);
        }
}


1;
