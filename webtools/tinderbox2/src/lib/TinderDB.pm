# -*- Mode: perl; indent-tabs-mode: nil -*-

# TinderDB.pm - the persistant storage for tinterbox.  This package
# allows the user to define which features (columns of the status
# table) are availible and provide a consistant interface to
# store/load the data and render the HTML to display the data.


# The class TinderDB is a wrapper which knows how to call the
# individual databases to present a coheriant interface for the
# implementations which are defined.  Any implementation can be left
# out if you do not wish to run with it in your shop.  This interface
# will do the right thing with your configuration.  Configurations are
# defined by adjusting the 'use' statements below.


# This interface controls the Databases:
#	Time Column, Version Control (VC) checkin lists, 
#       notice board display,  build display (colored squares)


# $Revision: 1.5 $ 
# $Date: 2000/11/09 19:31:05 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB.pm,v $ 
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





package TinderDB;


# Standard perl libraries

use File::Basename;



# Tinderbox Specific Libraries

use lib '#tinder_libdir#';

use Utils;


# Use the DB implementations you wish to use.

# These uses determine the columns of the build page and their
# include order is the order in which the columns are displayed.

# The main choice of implementations is how to gather information
# about checkins.  You can currently choose wether you are using
# bonsai or are using CVS raw.

if ( defined(@TinderConfig::DBImpl) ) {
  @IMPLS = @TinderConfig::DBImpl;
} else {
  @IMPLS = (
            'TinderDB::Time',
            'TinderDB::VC_CVS',
            'TinderDB::Notice',
            'TinderDB::BT_Generic',
            'TinderDB::Build',
           );
}

main::require_modules(@IMPLS);



# It would be nice if we had some kind of display of the bug tracking
# system as well.

$VERSION = '#tinder_version#';


# Should we turn on assertion checking?

$DEBUG = 1;


# What border should the status legends use?  new browers allow us to
# frame the parts of the legend without putting a border arround the
# individual cells.

if ( defined($TinderConfig::DB_LEGEND_BORDER) ) {
  $LEGEND_BORDER = $TinderConfig::DB_LEGEND_BORDER
} else {
  
  # if configuring in this file uncomment one of the assignment
  # lines below
  
  $TinderConfig::LEGEND_BORDER = 
    "";
  # "border rules=none";
}
  
# finest spacing on html page (in minutes), this resticts the
# minimum time between builds (to this value plus 5 minutes).

$TABLE_SPACING = $TinderConfig::DB_TABLE_SPACING || (5);

# number of times a database can be updated before its contents must
# be trimmed of old data.  This scan of the database is used to
# collect data about average build time so we do not want it
# performed too infrequently.

$MAX_UPDATES_SINCE_TRIM = $TinderConfig::DB_MAX_UPDATES_SINCE_TRIM || (50);

# Number of seconds to keep in Database, older data will be trimmed
# away.

$TRIM_SECONDS = $TinderConfig::DB_TRIM_SECONDS || (60 * 60 * 24 * 8);

# The DB implemenations are sourced in TinderConfig.pm just before
# this wrapper class is sourced.  It is expected that the
# implementations will need adjustment and other fiddling so we moved
# all the requires out of this file into the configuraiton file.


# Build a DB object to represent the columns of the build page.  Each
# db subclass has already added, at the time it was required, an empty
# object to HTML_COLUMNS.  This is how we track the impelmentations we
# are going to use and the correct order of the columns in the status
# table.

$DB = bless(\@TinderDB::HTML_COLUMNS);

# We may want the administration page to have the power to rearange
# the columns.  Don't forget that the way to get the DB column names
# is:

# foreach $obj (@{$TinderDB::DB}) { 
#	print ref($obj)."\n"; 
# }


#-----------------------------------------------------------
# You should not need to configure anything below this line
#-----------------------------------------------------------



# Our functions for database methods just iterate over the availible
# implementations.

# the next set of functions manipulate the persistant database.

sub loadtree_db {
  my ($tree, ) = @_;

  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::loadtree_db(): ".
          "Tree: $tree, not defined.");
  }

  foreach $db (@{$DB}) {
    $db->loadtree_db(@_);
  }

  return ;
}


sub apply_db_updates {
  my ($tree, ) = @_;
  
  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::apply_db_updates(): ".
          "Tree: $tree, not defined.");
  }

  my $out = 0;
  foreach $db (@{$DB}) {
    $out += $db->apply_db_updates(@_);
  }

  return $out;
}


sub trim_db_history {
# do not call this directly, its only here for testing.
# This is called by update
  my ($tree, ) = (@_);

  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::trim_db_history(): ".
          "Tree: $tree, not defined.");
  }

  my (@outrow) = ();
  foreach $db (@{$DB}) {
    $db->trim_db_history(@_);
  }

  return ;

}


sub savetree_db {
# do not call this directly, its only here for testing.
# This is called by update
  my ($tree) = @_;
  
  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::savetree_db(): ".
          "Tree: $tree, not defined.");
  }

  my (@outrow) = ();
  foreach $db (@{$DB}) {
     $db->savetree_db(@_);
  }

  return ;
}


# the next set of function make columns of the build page

sub status_table_legend {

  my (@outrow) = ();
  foreach $db (@{$DB}) {
    push @outrow, $db->status_table_legend(@_);
  }

  return (@outrow);
}


sub status_table_header {
  my ($tree, ) = @_;
  
  if ($DEBUG) {
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::status_table_header(): ".
          "Tree: $tree, not defined.");
  }

  my (@outrow) = ();
  foreach $db (@{$DB}) {
    push @outrow, $db->status_table_header(@_);
  }

  return (@outrow);
}


sub status_table_start {

  my ($row_times, $tree, ) = @_;

  if ($DEBUG) {
    ( ($row_times) && (scalar(@{$row_times}) > 5 ) ) ||
      die("TinderDB::status_table_start(): ".
          "row_times, not defined.");
  
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::status_table_start(): ".
          "Tree: $tree, not defined.");
  }

  my (@outrow) = ();
  foreach $db (@{$DB}) {
    $db->status_table_start(@_);
  }

  return ;
}



sub status_table_row {

  my ($row_times, $row_index, $tree, ) = @_;

  if ($DEBUG) {
    ( ($row_times) && (scalar(@{$row_times}) > 5 ) ) ||
      die("TinderDB::status_table_row(): ".
          "row_times, not defined.");
    
    ( defined($row_index) ) ||
      die("TinderDB::status_table_row(): ".
          "row_index, not defined.");
    
    ( ($row_index >= 0) && ($row_index <= $#{ $row_times }) ) ||
      die("TinderDB::status_table_row(): ".
          "index is not valid.");
    
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::status_table_row(): ".
          "Tree: $tree, not defined.");
  }

  my (@outrow) = ();
  foreach $db (@{$DB}) {
    push @outrow, $db->status_table_row(@_);
  }

  return (@outrow);
}


sub status_table_body {
  my ($row_times, $tree, ) = @_;

  if ($DEBUG) {
    ( ($row_times) && (scalar(@{$row_times}) > 5 ) ) ||
      die("TinderDB::status_table_body(): ".
          "row_times, not defined.");
    
    (TreeData::tree_exists($tree)) ||
      die("TinderDB::status_table_body(): ".
          "Tree: $tree, not defined.");
  }

  my @out;

  # We must call html_start before we call html_row.  

  TinderDB::status_table_start($row_times, $tree);
  
  foreach $i (0 .. $#{$row_times}) {
    my (@row) = TinderDB::status_table_row($row_times, $i, $tree);
    push @out, (
                "<!-- Row $i -->\n",
                "<tr align=center>\n",
                "@row\n",
                "</tr>\n\n"
               );
  }

  return @out;
}


1;

__END__

=head1 NAME

TinderDB.pm - abstract interface to the tinderbox persistant datastores.

=head1 DESCRIPTION

This is an abstract interface into all the columns of the Tinderbox
HTML page.  Each set of columns (Build, Version-Control, Notes event
the times in the first column) knows how to generate its own html and
the full table is generated by concatinating their individual output.
The set of columns are managed separately and may be rearanged,
execluded from a site or run with varing implementations.  Each set of
columns must manage its own historical data in a database and create
the html rows needed for display.  This module provides a single
abstract interface which can access all the databases and makes it
possible to easily configure tinderbox to run without some of the
databases.  This would be needed if you wish to run tinderbox but have
no Version-Control system compatible with the current interface or
wished to run tinderbox ONLY for continual monitoring of the
Version-Control system and not run any builds. 


The separate implemenatation modules manipulated are:

=head1 Implementations


=over 4

=item B<TinderDB::Time>

The column which displays the times and may optionally have URLS to 
the version control web query interface.


=back


=over 4

=item B<TinderDB::VC>

The column which displays who checked in at what time in the build
cyle.  This column must have an implementation which is specific to
the version contol (VC) software in use.

=back


=over 4

=item B<TinderDB::Notice>

The column displays all notices sent by users to the web interface.


=back

=over 4

=item B<TinderDB::BT>

The set of columns displays which bug tickets are moving forward and
which tickets are moving backwards through their lifecycle.  The
defintion of forward and backward is easily configurable and is based
on the state that the bug has moved into. Bugs which are 'closed' or
'verified' are moving forward, bugs which are 'reopened' or 'failed'
are moving backwards.  This column should remain fairly generic for
all bug tracking systems but may require different mail parsers of the
bug tracking system uses an exotic mail notification format.

=back


=over 4

=item B<TinderDB::Build>

This set of columns shows the state of each build type for the given
tree and contains links to the logfiles.  Builds may include any
quality control metric which can be labled pass/fail.  Typical quality
metrics are: linters, regression tests, coverage tests, performance
test, and multiplatform compilation of the source code.

=back


=head1 INTERNALS

The databases are never locked by the client software (notice board,
mail processing software, version control software if some form of
push is used).  So if many different pieces of data arrive
simultaniously for the tinderbox server they must all be stored in
separate files.  The regular updates (pushed data for example the
build log updates or the notice board updates) are passed to the
tinderbox server as text files which contain perl code to be
evaluated.  Each data update is stored in a file with a known name and
a unique extension.  The tinderbox server is run periodically in
daemon_mode and will assimilate all outstanding updates then update
the static html files which discribe the state of the build.  When
tinderbox runs it looks for files with the known prefix then it reads
each one in turn, loads it into a common database then deletes the
file.  To ensure that tinderbox never encounters a partially written
file each file is written to the disk using a a name with a different
prefix then the server looks for (beginning with 'Tmp') then the name
is changed to be the name tinderbox looks for.  Since name changes, on
Unix systems, in the same directory, are atomic, tinderbox will never
be confused by incomplete updates.


The build module has several lists which the tinder server access via
known functions.  These are used to generate summary data in various
formats.  (@LATEST_STATUS; @BUILD_NAMES; etc;)


Note: that the VC DB module gets the current tree state from the
TinderHeader::TreeState and records that value in its database so that
it can shade the VC column correctly.  This does not require any
locking or data storage on the part of the TreeState module.


The tinderbox server can be run by the webserver in cgi_mode (non
daemon_mode) this does not allow any databases to be updated and does
not update any static html files but will allow users to generate the
build data pages using different configuration parameters then is
standard.

The tinderbox server does not use any information about the internal
DB format.  Each module can use any convienent means to store and
retrive the required data.  Different implementations of the same
module may even use different methods of storage.  The tinderbox
server will ask each module which is implemented to load their
database with fresh data then it will call each module and ask to
create the html for each row in turn.  No information stored in any
database depends on the table grid size.  The grid is specified at the
time the html is generated.  The html for each set of columns is
generated by comparing the contents of the database against a vector
of times in time() format (which represents the time label for each
row) and a row index (which represents which row we are building the
html for).


Many of the database implementations inherit their database file
manipulation from the module BasicTxtDB.pm which provides safe methods
for loading and saving databases.



Each function discribed here builds an $out string.  If there are bugs
in the resulting HTML you can put your perl breakpoint on the return
statement of any function and look at the completed string before it
is returned.


Each module must implement the following functions.  The TinderDB.pm
module just cycles through all implementations and performs the
function call on each one.

=head1 METHODS


=over 4

=item B<loadtree_db> 

Load the database for the current tree

=back


=over 4

=item B<apply_db_updates>

Gather any new data which is applicable to this database and update
the database for this tree.  Returns the number of updates
processed. This function will call the internal function savetree_db()
to save the updated database to disk.  It returns the number of
updates which have occured.

The Build module takes this opportunity to set the known global
variables: 

	\@Tinder::DB::Build::LATEST_STATUS, 
	\@Tinder::DB::Build::BUILD_NAMES, 

and to compute its gettree_header() method.

This function will allways call savetree_db()
and will periodically call trim_db_history()

=back

=over 4

=item B<savetree_db>

Save the current database to disk.  This should not be called
directly, rather it is called everytime that apply_db_updates() is
called.


=back

=over 4

=item B<trim_db_history>

Purge any history from this trees database which is older then the
time given.  This should not be called directly, rather it is called
everytime the number of updates by apply_db_updates() since the last
purge is grater then $MAX_UPDATES_SINCE_TRIM.


=back


=over 4

=item B<status_table_legend>

return a table representing the legend for the set of columns this
implementation puts into the build table.


=back


=over 4

=item B<status_table_header>

return a header line appropriate for theset of columns this
implementation puts into the build table.


=back


=over 4

=item B<status_table_start>

This function is called before the rows of the table are generated.
Each method may clear its internal variables and perform any setup
necessary.  The function accepts the vector of row times so that
calculations using this data prepared.  The row times vector must not
change during the page creation. This should not be called directly,
it is called by status_table_body().

=back


=over 4

=item B<status_table_row>

return a html representation of the given row.  It accepts the vector
of row times and the current row number.  Each row will be created in
order from row 0 to row \$\#row_times.  By allowing the
implementations to know the order of row traversal and the times that
each row symbolizes, implementations may use large rowspans and
provide blank output when their results are not needed. This should
not be called directly, it is called by status_table_body().



=back


=over 4

=item B<status_table_row>

return a html representation of the body of the status table. 
this method is just a wrapper for status_table_start() and
status_table_row().



=back



=head1 AUTHOR

Ken Estes (kestes@staff.mail.com)


=cut
