#!/bin/perl
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
# The Original Code is Mozilla build system.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Ted Mielczarek <ted.mielczarek@gmail.com>
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

# Usage: apply-ed-patches.pl <source patch> <target directory> <path to ed>

use FileHandle;

sub do_patch {
  my ($ed, $target_file, $patch_file, $fh) = @_;
  # these keep winding up read only for me
  chmod 0666, $target_file;
  print $fh "w\n";
  $fh->close();
  print "$ed -s $target_file < $patch_file\n";
  system "$ed -s $target_file < $patch_file\n";
}

my $header_done = 0;
my ($target_file,$patch_file) = ('','');
my $source_patch = $ARGV[0];
my $srcdir = $ARGV[1];
my $ed = $ARGV[2];
$srcdir = "$srcdir/" unless $srcdir =~ m|/$|;
my $pfh = new FileHandle($source_patch, 'r');
while(<$pfh>) {
  # skip initial comment header
  next if !$header_done && /^#/;
  $header_done = 1;

  next if /^Only in/;
  if (/^diff -re (\S+)/) {
    my $new_file = $1;
    $new_file =~ s|^crt/src/||;
    $new_file = "$srcdir$new_file";
    my $new_patch_file = "$new_file.patch";

    if ($target_file ne '') {
      do_patch $ed, $target_file, $patch_file, $fh;
    }
    $target_file = $new_file;
    $patch_file = $new_patch_file;
    $fh = new FileHandle($patch_file, 'w');
    next;
  }

  print $fh $_ if $fh;
}

do_patch $ed, $target_file, $patch_file, $fh;
