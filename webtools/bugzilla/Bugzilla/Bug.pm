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
#                 Chris Yeh      <cyeh@bluemartini.com>

use strict;

use DBI;
use RelationSet;
use vars qw($unconfirmedstate $legal_keywords);

package Bug;
use CGI::Carp qw(fatalsToBrowser);
my %ok_field;

use Bugzilla::Config;
use Bugzilla::Util;

for my $key (qw (bug_id alias product version rep_platform op_sys bug_status 
                resolution priority bug_severity component assigned_to
                reporter bug_file_loc short_desc target_milestone 
                qa_contact status_whiteboard creation_ts 
                delta_ts votes whoid comment query error) ){
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

  # If the bug ID isn't numeric, it might be an alias, so try to convert it.
  $bug_id = &::BugAliasToID($bug_id) if $bug_id !~ /^[1-9][0-9]*$/;
  
  my $old_bug_id = $bug_id;
  if ((! defined $bug_id) || (!$bug_id) || (!detaint_natural($bug_id))) {
      # no bug number given or the alias didn't match a bug
      $self->{'bug_id'} = $old_bug_id;
      $self->{'error'} = "InvalidBugId";
      return $self;
  }

# default userid 0, or get DBID if you used an email address
  unless (defined $user_id) {
    $user_id = 0;
  }
  else {
     if ($user_id =~ /^\@/) {
        $user_id = &::DBname_to_id($user_id); 
     }
  }
     
  $self->{'whoid'} = $user_id;

  my $query = "
    select
      bugs.bug_id, alias, products.name, version, rep_platform, op_sys, bug_status,
      resolution, priority, bug_severity, components.name, assigned_to, reporter,
      bug_file_loc, short_desc, target_milestone, qa_contact,
      status_whiteboard, date_format(creation_ts,'%Y-%m-%d %H:%i'),
      delta_ts, sum(votes.count)
    from bugs left join votes using(bug_id),
      products, components
    where bugs.bug_id = $bug_id
      AND products.id = bugs.product_id
      AND components.id = bugs.component_id
    group by bugs.bug_id";

  &::SendSQL($query);
  my @row = ();

  if ((@row = &::FetchSQLData()) && &::CanSeeBug($bug_id, $self->{'whoid'})) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "alias", "product", "version", "rep_platform",
                       "op_sys", "bug_status", "resolution", "priority",
                       "bug_severity", "component", "assigned_to", "reporter",
                       "bug_file_loc", "short_desc", "target_milestone",
                       "qa_contact", "status_whiteboard", "creation_ts",
                       "delta_ts", "votes") {
        $fields{$field} = shift @row;
        if ($fields{$field}) {
            $self->{$field} = $fields{$field};
        }
        $count++;
    }
  } elsif (@row) {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotPermitted";
      return $self;
  } else {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotFound";
      return $self;
  }

  $self->{'assigned_to'} = &::DBID_to_name($self->{'assigned_to'});
  $self->{'reporter'} = &::DBID_to_name($self->{'reporter'});

  my $ccSet = new RelationSet;
  $ccSet->mergeFromDB("select who from cc where bug_id=$bug_id");
  my @cc = $ccSet->toArrayOfStrings();
  if (@cc) {
    $self->{'cc'} = \@cc;
  }

  if (Param("useqacontact") && (defined $self->{'qa_contact'}) ) {
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
      $self->{'keywords'} = join(', ', @list);
    }
  }

    &::SendSQL("select attach_id, creation_ts, isprivate, description 
             from attachments 
             where bug_id = $bug_id");
    my @attachments;
    while (&::MoreSQLData()) {
        my ($attachid, $date, $isprivate, $desc) = (&::FetchSQLData());
        my %attach;
        $attach{'attachid'} = $attachid;
        $attach{'isprivate'} = $isprivate;
        $attach{'date'} = $date;
        $attach{'desc'} = $desc;
        push @attachments, \%attach;
    }
    if (@attachments) {
        $self->{'attachments'} = \@attachments;
    }

    &::SendSQL("select bug_id, who, bug_when, isprivate, thetext 
           from longdescs 
           where bug_id = $bug_id");
    my @longdescs;
    while (&::MoreSQLData()) {
        my ($bug_id, $who, $bug_when, $isprivate, $thetext) = (&::FetchSQLData());
        my %longdesc;
        $longdesc{'who'} = $who;
        $longdesc{'bug_when'} = $bug_when;
        $longdesc{'isprivate'} = $isprivate;
        $longdesc{'thetext'} = $thetext;
        push @longdescs, \%longdesc;
    }
    if (@longdescs) {
        $self->{'longdescs'} = \@longdescs;
    }
  
  my @depends = EmitDependList("blocked", "dependson", $bug_id);
  if (@depends) {
      $self->{'dependson'} = \@depends;
  }  
  my @blocks = EmitDependList("dependson", "blocked", $bug_id);
  if (@blocks) {
    $self->{'blocks'} = \@blocks;
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

  foreach my $field ("bug_id", "alias", "bug_status", "product",
      "priority", "version", "rep_platform", "assigned_to", "delta_ts", 
      "component", "reporter", "target_milestone", "bug_severity", 
      "creation_ts", "qa_contact", "op_sys", "resolution", "bug_file_loc",
      "short_desc", "keywords", "status_whiteboard") {
    if ($self->{$field}) {
      $xml .= "  <$field>" . QuoteXMLChars($self->{$field}) . "</$field>\n";
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
            next if ($self->{'longdescs'}[$i]->{'isprivate'} 
                     && Param("insidergroup")
                     && !&::UserInGroup(Param("insidergroup")));
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
            next if ($self->{'attachments'}[$i]->{'isprivate'} 
                     && Param("insidergroup")
                     && !&::UserInGroup(Param("insidergroup")));
            $xml .= "  <attachment>\n"; 
            $xml .= "    <attachid>" . $self->{'attachments'}[$i]->{'attachid'}
                                    . "</attachid>\n"; 
            $xml .= "    <date>" . $self->{'attachments'}[$i]->{'date'} . "</date>\n"; 
            $xml .= "    <desc>" . QuoteXMLChars($self->{'attachments'}[$i]->{'desc'}) . "</desc>\n"; 
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
  $_[0] =~ s/&/&amp;/g;
  $_[0] =~ s/</&lt;/g;
  $_[0] =~ s/>/&gt;/g;
  $_[0] =~ s/'/&apos;/g;
  $_[0] =~ s/"/&quot;/g;
# $_[0] =~ s/([\x80-\xFF])/&XmlUtf8Encode(ord($1))/ge;
  return($_[0]);
}

sub XML_Header {
  my ($urlbase, $version, $maintainer, $exporter) = (@_);

  my $xml;
  $xml = "<?xml version=\"1.0\" standalone=\"yes\"?>\n";
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

sub CanChangeField {
   my $self = shift();
   my ($f, $oldvalue, $newvalue) = (@_);
   my $UserInEditGroupSet = -1;
   my $UserInCanConfirmGroupSet = -1;
   my $ownerid;
   my $reporterid;
   my $qacontactid;

    if ($f eq "assigned_to" || $f eq "reporter" || $f eq "qa_contact") {
        if ($oldvalue =~ /^\d+$/) {
            if ($oldvalue == 0) {
                $oldvalue = "";
            } else {
                $oldvalue = &::DBID_to_name($oldvalue);
            }
        }
    }
    if ($oldvalue eq $newvalue) {
        return 1;
    }
    if (trim($oldvalue) eq trim($newvalue)) {
        return 1;
    }
    if ($f =~ /^longdesc/) {
        return 1;
    }
    if ($UserInEditGroupSet < 0) {
        $UserInEditGroupSet = UserInGroup($self, "editbugs");
    }
    if ($UserInEditGroupSet) {
        return 1;
    }
    &::SendSQL("SELECT reporter, assigned_to, qa_contact FROM bugs " .
                "WHERE bug_id = $self->{'bug_id'}");
    ($reporterid, $ownerid, $qacontactid) = (&::FetchSQLData());

    # Let reporter change bug status, even if they can't edit bugs.
    # If reporter can't re-open their bug they will just file a duplicate.
    # While we're at it, let them close their own bugs as well.
    if ( ($f eq "bug_status") && ($self->{'whoid'} eq $reporterid) ) {
        return 1;
    }
    if ($f eq "bug_status" && $newvalue ne $::unconfirmedstate &&
        &::IsOpenedState($newvalue)) {

        # Hmm.  They are trying to set this bug to some opened state
        # that isn't the UNCONFIRMED state.  Are they in the right
        # group?  Or, has it ever been confirmed?  If not, then this
        # isn't legal.

        if ($UserInCanConfirmGroupSet < 0) {
            $UserInCanConfirmGroupSet = &::UserInGroup("canconfirm");
        }
        if ($UserInCanConfirmGroupSet) {
            return 1;
        }
        &::SendSQL("SELECT everconfirmed FROM bugs WHERE bug_id = $self->{'bug_id'}");
        my $everconfirmed = FetchOneColumn();
        if ($everconfirmed) {
            return 1;
        }
    } elsif ($reporterid eq $self->{'whoid'} || $ownerid eq $self->{'whoid'} ||
             $qacontactid eq $self->{'whoid'}) {
        return 1;
    }
    $self->{'error'} = "
Only the owner or submitter of the bug, or a sufficiently
empowered user, may make that change to the $f field."
}

sub Collision {
    my $self = shift();
    my $write = "WRITE";        # Might want to make a param to control
                                # whether we do LOW_PRIORITY ...
    &::SendSQL("LOCK TABLES bugs $write, bugs_activity $write, cc $write, " .
               "cc AS selectVisible_cc $write, " .
            "profiles $write, dependencies $write, votes $write, " .
            "keywords $write, longdescs $write, fielddefs $write, " .
            "keyworddefs READ, groups READ, attachments READ, products READ");
    &::SendSQL("SELECT delta_ts FROM bugs where bug_id=$self->{'bug_id'}");
    my $delta_ts = &::FetchOneColumn();
    &::SendSQL("unlock tables");
    if ($self->{'delta_ts'} ne $delta_ts) {
       return 1;
    }
    else {
       return 0;
    }
}

sub AppendComment  {
    my $self = shift();
    my ($comment) = (@_);
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace.
        return;
    }

    &::SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) " .
            "VALUES($self->{'bug_id'}, $self->{'whoid'}, now(), " . &::SqlQuote($comment) . ")");

    &::SendSQL("UPDATE bugs SET delta_ts = now() WHERE bug_id = $self->{'bug_id'}");
}


#from o'reilley's Programming Perl
sub display {
    my $self = shift;
    my @keys;
    if (@_ == 0) {                  # no further arguments
        @keys = sort keys(%$self);
    }  else {
        @keys = @_;                 # use the ones given
    }
    foreach my $key (@keys) {
        print "\t$key => $self->{$key}\n";
    }
}

sub CommitChanges {

#snapshot bug
#snapshot dependencies
#check can change fields
#check collision
#lock and change fields
#notify through mail

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
