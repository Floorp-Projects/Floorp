# -*- Mode: perl; indent-tabs-mode: nil -*-

# TinderHeader.pm - the persistant storage for tinterbox
# administrative settings.  This package allows the adminstrator to
# set their values and retrieves the values for the status pages.

# This interface controls the scalar values
#	 TreeState, Build, IgnoreBuilds, MOTD, Images, 


# $Revision: 1.10 $ 
# $Date: 2003/08/17 01:44:07 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderHeader.pm,v $ 
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





package TinderHeader;



# Standard perl libraries

use File::Basename;



# Tinderbox Specific Libraries

use lib '#tinder_libdir#';

use Utils;



# These names are optional page header information the only method
# they contain is called: html_value.  If you comment out any of these
# uses a default method will be provided.  Usually this method is
# the null function.

if ( defined(@TinderConfig::HeaderImpl) ) {
  @IMPLS = @TinderConfig::HeaderImpl;
} else {
  @IMPLS = (
            'TinderHeader::Build',
            'TinderHeader::IgnoreBuilds',
            'TinderHeader::MOTD',

            # TinderDB::VC_Bonsai provides a
            # TinderHeader::TreeState implementation,
            # so comment out the TreeSTate if using
            # VC_Bonsai. Most VC implementations will
            # not have a State file in the version
            # control system.

            'TinderHeader::TreeState',

            # this is not implemented yet
            #'TinderHeader::Image,
           );
}


main::require_modules(@IMPLS);


$VERSION = '#tinder_version#';


# Should we turn on assertion checking?

$DEBUG = 1;


# each of the TinderHeader method appears on the left side of this
# hash and gets a default value.

%HEADER2DEFAULT_HTML = (%HEADER2DEFAULT_HTML ||
                        (
                         # the build module has one piece of info
                         # which goes in the header, our best guess 
                         # as to when the tree broke.
                         
                         'Build' => "",
                         'IgnoreBuilds' => "",
                         'MOTD' => "",
                         'TreeState' => "Open",
                         
                        )
                       );

# As each implmentation is 'used' it will load a a null object in its
# namespace into a hash indexed by the names of the implementations
# just as in the default hash.

# For example the TreeState module has this code in it:
#
# 	push %TinderHeader::NAMES2OBJS, ('TreeState' => new());


# Double check that there is a default for each implementation we are
# using, if not something is wrong.  We do not need the defaults we
# just want to check that things are OK.

foreach $namespace (keys %NAMES2OBJS) {
  (defined($TinderHeader::HEADER2DEFAULT_HTML{$namespace})) ||
    die("Can not register $namespace ".
        "in TinderHeader, no Default provided.\n");
}


%ALL_HEADERS=();

#-----------------------------------------------------------
# You should not need to configure anything below this line
#-----------------------------------------------------------



# get the html value from disk

sub gettree_header {
  my ($impl, $tree, ) = @_;
  my $out;

  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderHeader::gettree_header(): ".
          "Tree: $tree, not defined.");

    (defined($HEADER2DEFAULT_HTML{$impl})) ||
      die("TinderHeader::gettree_header(): ".
          "Header implementation: $impl, not defined.");
  }

  if ($NAMES2OBJS{$impl}) {
    my $obj = $NAMES2OBJS{$impl};
    $out = $obj->gettree_header($tree);
    (defined($out)) ||
      ($out = $HEADER2DEFAULT_HTML{$impl});
  } else {
    $out = $HEADER2DEFAULT_HTML{$impl};
  }

  return $out;
}



# save the html value to disk

sub savetree_header {
  my ($impl, $tree, $value) = @_;
  my $out;

  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderHeader::savetree_header(): ".
          "Tree: $tree, not defined.");

    (defined($HEADER2DEFAULT_HTML{$impl})) ||
      die("TinderHeader::savetree_header(): ".
          "Header implementation: $impl, not defined.");
  }

  if ($NAMES2OBJS{$impl}) {
    my $obj = $NAMES2OBJS{$impl};
    $out = $obj->savetree_header($tree, $value);
  }

  return $out;
}


# get all the headers for a particular tree, use the cache if they
# have been loaded already.

sub get_alltree_headers {
  my ($tree) = @_;

  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderHeader::gettree_header(): ".
          "Tree: $tree, not defined.");
  }
  
  if (!$ALL_HEADERS{$tree}) {
    my $header_ref;
    
    foreach $impl (keys %HEADER2DEFAULT_HTML) {
      $header_ref->{$impl} = gettree_header($impl, $tree);
    }
    
    $ALL_HEADERS{$tree} = $header_ref;
  }

  return $ALL_HEADERS{$tree};
}


# save all the headers for a particular tree to disk in an easily
# parsable HTML form so that other programs can get at these values.

sub export_alltree_headers {
  my ($tree, $header_ref) = @_;

  my @out = ("<pre>\n");
  foreach $impl (keys %{ $header_ref }) {
   push @out, "$impl: $header_ref->{$impl}\n";
  }

  $outfile = FileStructure::get_filename($tree, 'alltree_headers');
  main::overwrite_file($outfile, @out);

  return 1;
}



1;

__END__

=head1 NAME

TinderHeader.pm - abstract interface to the tinderbox administrative settings.

=head1 DESCRIPTION



The separate implemenatation modules manipulated are:

=head1 Implementations


=over 4

=item B<TinderHeader::Build>

A space sparated list build names which should not be displayed on the
status page.  The value is set by the administrators.  This choice can
be overrridden by any user simply by running the tinder.cgi program
with noignore=1.  The data for these builds will continue to be
processed this value only effects the display.

=back


=over 4

=item B<TinderHeader::IgnoreBuilds>

A space sparated list build names which should not be displayed on the
status page.  This choice can be overrridden by running the tinder.cgi
program with noignore=1.  The data for these builds will continue to
be processed this value only effects the display.


=back


=over 4


=item B<TinderHeader::TreeState>

The current state of the tree as set by the adminstrators.  This is
used as an informational message only it does not directly effect the
operation of the version control system.


=back


=over 4

=item B<TinderHeader::MOTD>

The message of the day as set by the administrators.


=back


=over 4

Headers are Administrative functions and are performed rarely (less
then once an hour) and no historical data is preserved.  So there is
no locking or moving of files for these functions.  An effort is made
to keep the updates of these files atomic so that the rendering of any
HTML pages will not be effected by an update.  The data is stored in
"header" files, which are simply text files containing perl
expressions to be evaluated to gain the current data.  If
implementations are not provded then default values are used.  If any
header file has two contemporanious updates the result is undefined.


Note: that the VC DB module gets the current tree state from the
TreeState header and records that value in its database so that it can
shade the VC column correctly. 

Each module must implement the following functions.


=head1 METHODS


=over 4

=item B<savetree_header> 

Store the data for this tree specific header to the correct disk file.
This is usually called by admintree.cgi

=back


=over 4

=item B<gettree_header>

Return the current setting of the header in a form suitable for HTML
display.


=back

These are class methods which operate on the individual implementations.


=head1 CLASS METHODS


=over 4

=item B<savetree_header> 

Store the data for this tree specific header to the correct disk file.
This is usually called by admintree.cgi.  The user must specify which
tree and which implementation (string) is being accessed.

=back


=over 4

=item B<gettree_header>

Return the current setting of the header in a form suitable for HTML
display.  The user must specify which tree and which implementation
(string) is being accessed.

=back


=over 4

=item B<get_alltree_headers>

Returns a hash_ref containing all the headers for a particular tree
indexed by header name.


=back

=over 4

=item B<export_alltree_headers>

Save all the headers for a particular tree to disk in an easily
parsable HTML form so that other programs can get at these values.
The only argument to this function is the output of
get_alltree_headers.

=back

=head1 INTERNALS

=over 4

Many of the implementations inherit their database file manipulation
from the module BasicTxtHeader.pm which provides methods for loading
and atomically saving headers.  These header values are always in HTML
displayable format.

These module get their updates as a single database.  The client
software (admintree.cgi) updates the database file directly.  There
are no update files to assimilate into a single file only the last
version of the database is kept on disk.  They only implement the
function gettree_header which returns the current value in a form
suitable for html display.

The header information is found by calling the function
'gettree_header' and passing in the name of the implementation and
tree desired.  The names of the implementations values are not as
flexible as the columns of the build table.  This is because these
values have to go into a particular part of the HTML to be displayed
properly.  This requires the HTML generation software to know the
values it expects to get by name.

If some headers are not desired to run the 'use' statements for them
may be safely commented out and a default value will automatically be
provided.

Each function described here builds an $out string.  If there are bugs
in the resulting HTML you can put your perl breakpoint on the return
statement of any function and look at the completed string before it
is returned.

=head1 AUTHOR

Ken Estes (kestes@staff.mail.com)

=cut
