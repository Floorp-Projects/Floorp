#!/usr/bonsaitools/bin/perl -w
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

use DBI;
use RelationSet;
require "globals.pl";
require "CGI.pl";
package Bug;
use CGI::Carp qw(fatalsToBrowser);
my %ok_field;

for my $key (qw (bug_id product version rep_platform op_sys bug_status 
                resolution priority bug_severity component assigned_to
                reporter bug_file_loc short_desc target_milestone 
                qa_contact status_whiteboard creation_ts groupset 
                delta_ts error votes) ){
    $ok_field{$key}++;
    }

# create a new empty bug
#
sub new {
  my $type = shift();
  my %bug;

  # create a ref to an empty hash and bless it
  #
  my $self = {%bug};
  bless $self, $type;

  # construct from a hash containing a bug's info
  #
  if ($#_ == 1) {
    $self->initBug(@_);
  } else {
    confess("invalid number of arguments \($#_\)($_)");
  }

  # bless as a Bug
  #
  return $self;
}



# dump info about bug into hash unless user doesn't have permission
# user_id 0 is used when person is not logged in.
#
sub initBug  {
  my $self = shift();
  my ($bug_id, $user_id) = (@_);


  if ( (! defined $bug_id) || (!$bug_id) ) {
    # no bug number given
    return {};
  }

  if (! defined $user_id) {
    $user_id = 0;
  }

  &::ConnectToDatabase();
  &::GetVersionTable();

  # this verification should already have been done by caller
  # my $loginok = quietly_check_login();
  my $usergroupset = "0";
  if (defined $::usergroupset) {
    $usergroupset = $::usergroupset;
  }

  my $query = "
    select
      bugs.bug_id, product, version, rep_platform, op_sys, bug_status,
      resolution, priority, bug_severity, component, assigned_to, reporter,
      bug_file_loc, short_desc, target_milestone, qa_contact,
      status_whiteboard, date_format(creation_ts,'%Y-%m-%d %H:%i'),
      groupset, delta_ts, sum(votes.count)
    from bugs left join votes using(bug_id)
    where bugs.bug_id = $bug_id
    and bugs.groupset & $usergroupset = bugs.groupset
    group by bugs.bug_id";

  &::SendSQL($query);
  my @row;

  if (@row = &::FetchSQLData()) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "product", "version", "rep_platform",
                       "op_sys", "bug_status", "resolution", "priority",
                       "bug_severity", "component", "assigned_to", "reporter",
                       "bug_file_loc", "short_desc", "target_milestone",
                       "qa_contact", "status_whiteboard", "creation_ts",
                       "groupset", "delta_ts", "votes") {
	$fields{$field} = shift @row;
	if ($fields{$field}) {
	    $self->{$field} = $fields{$field};
	}
	$count++;
    }
  } else {
    &::SendSQL("select groupset from bugs where bug_id = $bug_id");
    if (@row = &::FetchSQLData()) {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotPermitted";
      return $self;
    } else {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotFound";
      return $self;
    }
  }

  if ($self->{'short_desc'}) {
    $self->{'short_desc'} = QuoteXMLChars( $self->{'short_desc'} );
  }

  if (defined $self->{'status_whiteboard'}) {
    $self->{'status_whiteboard'} = QuoteXMLChars($self->{'status_whiteboard'});
  }

  $self->{'assigned_to'} = &::DBID_to_real_or_loginname($self->{'assigned_to'});
  $self->{'reporter'} = &::DBID_to_real_or_loginname($self->{'reporter'});

  my $ccSet = new RelationSet;
  $ccSet->mergeFromDB("select who from cc where bug_id=$bug_id");
  my @cc = $ccSet->toArrayOfStrings();
  if (@cc) {
    $self->{'cc'} = \@cc;
  }

  if (&::Param("useqacontact") && (defined $self->{'qa_contact'}) ) {
    my $name = $self->{'qa_contact'} > 0 ? &::DBID_to_name($self->{'qa_contact'}) :"";
    if ($name) {
      $self->{'qa_contact'} = $name;
    }
  }

  if (@::legal_keywords) {
    &::SendSQL("SELECT keyworddefs.name 
              FROM keyworddefs, keywords
             WHERE keywords.bug_id = $bug_id 
               AND keyworddefs.id = keywords.keywordid
          ORDER BY keyworddefs.name");
    my @list;
    while (&::MoreSQLData()) {
        push(@list, &::FetchOneColumn());
    }
    if (@list) {
      $self->{'keywords'} = &::html_quote(join(', ', @list));
    }
  }

  &::SendSQL("select attach_id, creation_ts, description 
           from attachments 
           where bug_id = $bug_id");
  my @attachments;
  while (&::MoreSQLData()) {
    my ($attachid, $date, $desc) = (&::FetchSQLData());
    if ($date =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
        $date = "$3/$4/$2 $5:$6";
      my %attach;
      $attach{'attachid'} = $attachid;
      $attach{'date'} = $date;
      $attach{'desc'} = &::html_quote($desc);
      push @attachments, \%attach;
    }
  }
  if (@attachments) {
    $self->{'attachments'} = \@attachments;
  }

  &::SendSQL("select bug_id, who, bug_when, thetext 
           from longdescs 
           where bug_id = $bug_id");
  my @longdescs;
  while (&::MoreSQLData()) {
    my ($bug_id, $who, $bug_when, $thetext) = (&::FetchSQLData());
    my %longdesc;
    $longdesc{'who'} = $who;
    $longdesc{'bug_when'} = $bug_when;
    $longdesc{'thetext'} = &::html_quote($thetext);
    push @longdescs, \%longdesc;
  }
  if (@longdescs) {
    $self->{'longdescs'} = \@longdescs;
  }

  if (&::Param("usedependencies")) {
    my @depends = EmitDependList("blocked", "dependson", $bug_id);
    if ( @depends ) {
      $self->{'dependson'} = \@depends;
    }
    my @blocks = EmitDependList("dependson", "blocked", $bug_id);
    if ( @blocks ) {
      $self->{'blocks'} = \@blocks;
    }
  }
  return $self;
}



# given a bug hash, emit xml for it. with file header provided by caller
#
sub emitXML {
  ( $#_ == 0 ) || confess("invalid number of arguments");
  my $self = shift();
  my $xml;


  if (exists $self->{'error'}) {
    $xml .= "<bug error=\"$self->{'error'}\">\n";
    $xml .= "  <bug_id>$self->{'bug_id'}</bug_id>\n";
    $xml .= "</bug>\n";
    return $xml;
  }

  $xml .= "<bug>\n";

  foreach my $field ("bug_id", "urlbase", "bug_status", "product",
      "priority", "version", "rep_platform", "assigned_to", "delta_ts", 
      "component", "reporter", "target_milestone", "bug_severity", 
      "creation_ts", "qa_contact", "op_sys", "resolution", "bug_file_loc",
      "short_desc", "keywords", "status_whiteboard") {
    if ($self->{$field}) {
      $xml .= "  <$field>" . $self->{$field} . "</$field>\n";
    }
  }

  foreach my $field ("dependson", "blocks", "cc") {
    if (defined $self->{$field}) {
      for (my $i=0 ; $i < @{$self->{$field}} ; $i++) {
        $xml .= "  <$field>" . $self->{$field}[$i] . "</$field>\n";
      }
    }
  }

  if (defined $self->{'longdescs'}) {
    for (my $i=0 ; $i < @{$self->{'longdescs'}} ; $i++) {
      $xml .= "  <long_desc>\n"; 
      $xml .= "   <who>" . &::DBID_to_name($self->{'longdescs'}[$i]->{'who'}) 
                         . "</who>\n"; 
      $xml .= "   <bug_when>" . $self->{'longdescs'}[$i]->{'bug_when'} 
                              . "</bug_when>\n"; 
      $xml .= "   <thetext>" . QuoteXMLChars($self->{'longdescs'}[$i]->{'thetext'})
                             . "</thetext>\n"; 
      $xml .= "  </long_desc>\n"; 
    }
  }

  if (defined $self->{'attachments'}) {
    for (my $i=0 ; $i < @{$self->{'attachments'}} ; $i++) {
      $xml .= "  <attachment>\n"; 
      $xml .= "    <attachid>" . $self->{'attachments'}[$i]->{'attachid'}
                              . "</attachid>\n"; 
      $xml .= "    <date>" . $self->{'attachments'}[$i]->{'date'} . "</date>\n"; 
      $xml .= "    <desc>" . $self->{'attachments'}[$i]->{'desc'} . "</desc>\n"; 
    # $xml .= "    <type>" . $self->{'attachments'}[$i]->{'type'} . "</type>\n"; 
    # $xml .= "    <data>" . $self->{'attachments'}[$i]->{'data'} . "</data>\n"; 
      $xml .= "  </attachment>\n"; 
    }
  }

  $xml .= "</bug>\n";
  return $xml;
}



sub EmitDependList {
  my ($myfield, $targetfield, $bug_id) = (@_);
  my @list;
  &::SendSQL("select dependencies.$targetfield, bugs.bug_status
           from dependencies, bugs
           where dependencies.$myfield = $bug_id
             and bugs.bug_id = dependencies.$targetfield
           order by dependencies.$targetfield");
  while (&::MoreSQLData()) {
    my ($i, $stat) = (&::FetchSQLData());
    push @list, $i;
  }
  return @list;
}



sub QuoteXMLChars {
  $_[0] =~ s/</&lt;/g;
  $_[0] =~ s/>/&gt;/g;
  $_[0] =~ s/'/&apos;/g;
  $_[0] =~ s/"/&quot;/g;
  $_[0] =~ s/&/&amp;/g;
# $_[0] =~ s/([\x80-\xFF])/&XmlUtf8Encode(ord($1))/ge;
  return($_[0]);
}


sub XML_Header {
  my ($urlbase, $version, $maintainer, $exporter) = (@_);

  my $xml;
  $xml = "<?xml version=\"1.0\" standalone=\"no\"?>\n";
  $xml .= "<!DOCTYPE bugzilla SYSTEM \"$urlbase";
  if (! ($urlbase =~ /.+\/$/)) {
    $xml .= "/";
  }
  $xml .= "bugzilla.dtd\">\n";
  $xml .= "<bugzilla";
  if (defined $exporter) {
    $xml .= " exporter=\"$exporter\"";
  }
  $xml .= " version=\"$version\"";
  $xml .= " urlbase=\"$urlbase\"";
  $xml .= " maintainer=\"$maintainer\">\n";
  return ($xml);
}


sub XML_Footer {
  return ("</bugzilla>\n");
}

sub AUTOLOAD {
  use vars qw($AUTOLOAD);
  my $self = shift;
  my $type = ref($self) || $self;
  my $attr = $AUTOLOAD;

  $attr =~ s/.*:://;
  return unless $attr=~ /[^A-Z]/;
  if (@_) {
    $self->{$attr} = shift;
    return;
  }
  confess ("invalid bug attribute $attr") unless $ok_field{$attr};
  if (defined $self->{$attr}) {
    return $self->{$attr};
  } else {
    return '';
  }
}

1;
