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
# Contributor(s): Dawn Endico <endico@mozilla.org>


# This script reads in xml bug data from standard input and inserts 
# a new bug into bugzilla. Everything before the beginning <?xml line
# is removed so you can pipe in email messages.

use diagnostics;
use strict;
use XML::Parser;
use Data::Dumper;
$Data::Dumper::Useqq = 1;

require "CGI.pl";
require "globals.pl";

GetVersionTable();
ConnectToDatabase();

sub sillyness {
    my $zz;
    $zz = %::components;
    $zz = %::versions;
    $zz = @::legal_bug_status;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_product;
    $zz = @::legal_severity;
    $zz = @::legal_resolution;
    $zz = %::target_milestone;
}

sub UnQuoteXMLChars {
    $_[0] =~ s/&amp;/&/g;
    $_[0] =~ s/&lt;/</g;
    $_[0] =~ s/&gt;/>/g;
    $_[0] =~ s/&apos;/'/g;
    $_[0] =~ s/&quot;/"/g;
#    $_[0] =~ s/([\x80-\xFF])/&XmlUtf8Encode(ord($1))/ge;
    return($_[0]);
}

my $xml;
while (<>) {
 $xml .= $_;
}
# remove everything in file before xml header (i.e. remove the mail header)
$xml =~ s/^.+(<\?xml version.+)$/$1/s;

my $parser = new XML::Parser(Style => 'Tree');
my $tree = $parser->parse($xml);

my $maintainer;
if (defined $tree->[1][0]->{'maintainer'}) {
  $maintainer= $tree->[1][0]->{'maintainer'}; 
} else {
  print "Cannot import these bugs because no maintainer for the exporting db is given.\n";
  print "Aborting.\n";
  exit;
}

my $exporter;
if (defined $tree->[1][0]->{'exporter'}) {
  $exporter = $tree->[1][0]->{'exporter'};
} else {
  print "Cannot import these bugs because no exporter is given.\n";
  print "Send notification to $maintainer.\n";
  print "Aborting.\n";
  exit;
}

my $urlbase;
if (defined $tree->[1][0]->{'urlbase'}) {
  $urlbase= $tree->[1][0]->{'urlbase'}; 
} else {
  print "Cannot import these bugs because the name of the exporting db was not given.\n";
  print "Send notification to $maintainer.\n";
  print "Aborting.\n";
  exit;
}
  

my $exporterid = DBname_to_id($exporter);
if ( ! $exporterid ) {
  print "The user <$tree->[1][0]->{'exporter'}> who tried to move bugs here ";
  print "does not have an account in this database. Aborting.\n";
  print "Send notification to $maintainer.\n";
  exit;
}


my $bugqty = ($#{@{$tree}->[1]} +1 -3) / 4;
print "Importing $bugqty bugs from $urlbase,\n  sent by $exporter.\n";
for (my $k=1 ; $k <= $bugqty ; $k++) {
  my $cur = $k*4;

  if (defined $tree->[1][$cur][0]->{'error'}) {
    print "\nError in bug $tree->[1][$cur][4][2]\@$urlbase:";
    print " $tree->[1][$cur][0]->{'error'}\n";
    if ($tree->[1][$cur][0]->{'error'} =~ /NotFound/) {
      print "$exporter tried to move bug $tree->[1][$cur][4][2] here";
      print " but $urlbase reports that this bug does not exist.\n"; 
    } elsif ( $tree->[1][$cur][0]->{'error'} =~ /NotPermitted/) {
      print "$exporter tried to move bug $tree->[1][$cur][4][2] here";
      print " but $urlbase reports that $exporter does not have access";
      print " to that bug.\n";
    }
    next;
  }

  my %multiple_fields;
  foreach my $field (qw (dependson cc long_desc blocks)) {
    $multiple_fields{$field} = "x"; 
  }
  my %all_fields;
  foreach my $field (qw (dependson product bug_status priority cc version 
      bug_id rep_platform short_desc assigned_to resolution
      delta_ts component reporter urlbase target_milestone bug_severity 
      creation_ts qa_contact keyword status_whiteboard op_sys blocks)) {
    $all_fields{$field} = "x"; 
  }
 
 
  my %bug_fields;
  my $err = "";
  for (my $i=3 ; $i < $#{@{$tree}->[1][$cur]} ; $i=$i+4) {
    if (defined $multiple_fields{$tree->[1][$cur][$i]}) {
      if (defined $bug_fields{$tree->[1][$cur][$i]}) {
        $bug_fields{$tree->[1][$cur][$i]} .= " " .  $tree->[1][$cur][$i+1][2];
      } else {
        $bug_fields{$tree->[1][$cur][$i]} = $tree->[1][$cur][$i+1][2];
      }
    } elsif (defined $all_fields{$tree->[1][$cur][$i]}) {
      $bug_fields{$tree->[1][$cur][$i]} = $tree->[1][$cur][$i+1][2];
    } else {
      $err .= "---\n";
      $err .= "Unknown bug field \"$tree->[1][$cur][$i]\"";
      $err .= " encountered while moving bug\n";
      $err .= "<$tree->[1][$cur][$i]>";
      if (defined $tree->[1][$cur][$i+1][3]) {
        $err .= "\n";
        for (my $j=3 ; $j < $#{@{$tree}->[1][$cur][$i+1]} ; $j=$j+4) {
          $err .= "  <". $tree->[1][$cur][$i+1][$j] . ">";
          $err .= " $tree->[1][$cur][$i+1][$j+1][2] ";
          $err .= "</". $tree->[1][$cur][$i+1][$j] . ">\n";
        }
      } else {
        $err .= " $tree->[1][$cur][$i+1][2] ";
      }
      $err .= "</$tree->[1][$cur][$i]>\n";
    }
  }

  my @long_descs;
  for (my $i=3 ; $i < $#{@{$tree}->[1][$cur]} ; $i=$i+4) {
    if ($tree->[1][$cur][$i] =~ /long_desc/) {
      my %long_desc;
      $long_desc{'who'} = $tree->[1][$cur][$i+1][4][2];
      $long_desc{'bug_when'} = $tree->[1][$cur][$i+1][8][2];
      $long_desc{'thetext'} = $tree->[1][$cur][$i+1][12][2];
      push @long_descs, \%long_desc;
    }
  }

  # instead of giving each comment its own item in the longdescs
  # table like it should have, lets cat them all into one big
  # comment otherwise we would have to lie often about who
  # authored the comment since commenters in one bugzilla probably
  # don't have accounts in the other one.
  sub by_date {my @a; my @b; $a->{'bug_when'} cmp $b->{'bug_when'}; }
  my @sorted_descs = sort by_date @long_descs;
  my $long_description = "";
  for (my $z=0 ; $z <= $#sorted_descs ; $z++) {
    unless ( $z==0 ) {
      $long_description .= "\n\n\n------- Additional Comments From ";
      $long_description .= "$sorted_descs[$z]->{'who'} "; 
      $long_description .= "$sorted_descs[$z]->{'bug_when'}"; 
      $long_description .= " ----\n\n";
    }
    $long_description .=  "$sorted_descs[$z]->{'thetext'}\n";
  }


  my $comments;
  my $query = "INSERT INTO bugs (\n";
  my $values = "VALUES (\n";

  $comments .= "\n\n------- Bug Moved by $exporter "; 
  $comments .= time2str("%Y-%m-%d %H:%M", time);
  $comments .= " -------\n\n";
  $comments .= "This bug previously known as bug $bug_fields{'bug_id'} at ";
  $comments .= $urlbase . "\n";
  $comments .= $urlbase . "show_bug.cgi?";
  $comments .= "id=" . $bug_fields{'bug_id'} . "\n";
  $comments .= "Originally filed under the $bug_fields{'product'} ";
  $comments .= "product and $bug_fields{'component'} component.\n";
  if (defined $bug_fields{'dependson'}) {
    $comments .= "Bug depends on bug(s) $bug_fields{'dependson'}.\n";
  }
  if (defined $bug_fields{'blocks'}) {
  $comments .= "Bug blocks bug(s) $bug_fields{'blocks'}.\n";
  }


  foreach my $field ( qw(creation_ts delta_ts keywords status_whiteboard) ) {
      if ( (defined $bug_fields{$field}) && ($bug_fields{$field}) ){
        $query .= "$field,\n";
        $values .= SqlQuote($bug_fields{$field}) . ",\n";
      }
  }

  if ( (defined $bug_fields{'short_desc'}) && ($bug_fields{'short_desc'}) ){
        $query .= "short_desc,\n";
      $values .= SqlQuote(UnQuoteXMLChars($bug_fields{'short_desc'})) . ",\n";
      }

  my @product;
  if (defined ($bug_fields{'product'}) &&
       (@product = grep /^$bug_fields{'product'}$/i, @::legal_product) ){
    $query .= "product,\n";
    $values .= SqlQuote($product[0]) . ",\n";
  } else {
    $query .= "product,\n";
    $values .= "\'Browser\',\n";
    $product[0] = "Browser";
    $err .= "Unknown product $bug_fields{'product'}. ";
    $err .= "Moving to default product \"Browser\".\n";
  }

  if (defined  ($::versions{$product[0]} ) &&
     (my @version = grep /^$bug_fields{'version'}$/i, 
                         @{$::versions{$product[0]}}) ){
    $values .= SqlQuote($version[0]) . ",\n";
    $query .= "version,\n";
  } else {
    $query .= "version,\n";
    $values .= "\'@{$::versions{$product[0]}}->[0]\',\n";
    $err .= "Unknown version $bug_fields{'version'} in product $product[0]. ";
    $err .= "Setting version to \"@{$::versions{$product[0]}}->[0]\".\n";
  }

  if (defined ($bug_fields{'priority'}) &&
       (my @priority = grep /^$bug_fields{'priority'}$/i, @::legal_priority) ){
    $values .= SqlQuote($priority[0]) . ",\n";
    $query .= "priority,\n";
  } else {
    $values .= "\'P3\',\n";
    $query .= "priority,\n";
    $err .= "Unknown priority ";
    $err .= (defined $bug_fields{'priority'})?$bug_fields{'priority'}:"unknown";
    $err .= ". Setting to default priority \"P3\".\n";
  }

  if (defined ($bug_fields{'rep_platform'}) &&
       (my @platform = grep /^$bug_fields{'rep_platform'}$/i, @::legal_platform) ){
    $values .= SqlQuote($platform[0]) . ",\n";
    $query .= "rep_platform,\n";
  } else {
    $values .= "\'Other\',\n";
    $query .= "rep_platform,\n";
    $err .= "Unknown platform ";
    $err .= (defined $bug_fields{'rep_platform'})?
                     $bug_fields{'rep_platform'}:"unknown";
    $err .= ". Setting to default platform \"Other\".\n";
  }

  if (defined ($bug_fields{'op_sys'}) &&
     (my @opsys = grep /^$bug_fields{'op_sys'}$/i, @::legal_opsys) ){
    $values .= SqlQuote($opsys[0]) . ",\n";
    $query .= "op_sys,\n";
  } else {
    $values .= "\'other\',\n";
    $query .= "op_sys,\n";
    $err .= "Unknown operating system ";
    $err .= (defined $bug_fields{'op_sys'})?$bug_fields{'op_sys'}:"unknown";
    $err .= ". Setting to default OS \"other\".\n";
  }

  my @component;
  if (defined  ($::components{$product[0]} ) &&
     (@component = grep /^$bug_fields{'component'}$/i, 
                       @{$::components{$product[0]}}) ){
    $values .= SqlQuote($component[0]) . ",\n";
    $query .= "component,\n";
  } else {
    $component[0] = $::components{$product[0]}->[0];
    $values .= SqlQuote($component[0]) . ",\n";
    $query .= "component,\n";
    $err .= "Unknown component \"";
    $err .= (defined $bug_fields{'component'})?$bug_fields{'component'}:"unknown";
    $err .= "\" in product \"$product[0]\".\n";
    $err .= "   Setting to this product\'s first component, ";
    $err .= "\'$::components{$product[0]}->[0]\'.\n";
  }

  if (Param("usetargetmilestone")) {
    if (defined  ($::target_milestone{$product[0]} ) &&
       (my @tm = grep /^$bug_fields{'target_milestone'}$/i, 
                       @{$::target_milestone{$product[0]}}) ){
      $values .= SqlQuote($tm[0]) . ",\n";
      $query .= "target_milestone,\n";
    } else {
      SendSQL("SELECT defaultmilestone FROM products " .
              "WHERE product = " . SqlQuote($product[0]));
      my $tm = FetchOneColumn();
      $values .= "\'$tm\',\n";
      $query .= "target_milestone,\n";
      $err .= "Unknown milestone \"";
      $err .= (defined $bug_fields{'target_milestone'})?
              $bug_fields{'target_milestone'}:"unknown";
      $err .= "\" in product \"$product[0]\".\n";
      $err .= "   Setting to default milestone for this product, ";
      $err .= "\'" . $tm . "\'\n";
    }
  }

  if (defined ($bug_fields{'bug_severity'}) &&
       (my @severity= grep /^$bug_fields{'bug_severity'}$/i, 
                           @::legal_severity) ){
    $values .= SqlQuote($severity[0]) . ",\n";
    $query .= "bug_severity,\n";
  } else {
    $values .= "\'normal',\n";
    $query .= "bug_severity,\n";
    $err .= "Unknown severity ";
    $err .= (defined $bug_fields{'bug_severity'})?
                     $bug_fields{'bug_severity'}:"unknown";
    $err .= ". Setting to default severity \"normal\".\n";
  }

  my $changed_owner = 0;
  if ( ($bug_fields{'assigned_to'}) && 
       ( DBname_to_id($bug_fields{'assigned_to'})) ) {
    $values .= "'" . DBname_to_id($bug_fields{'assigned_to'}) . "',\n";
    $query .= "assigned_to,\n";
  } else {
    $values .= "'" . $exporterid . "',\n";
    $query .= "assigned_to,\n";
    $changed_owner = 1;
    $err .= "The original owner of this bug does not have\n";
    $err .= "   an account here. Reassigning to the person who moved\n";
    $err .= "   it here, $bug_fields{'exporter'}\n";
    if ( $bug_fields{'assigned_to'} ) {
      $err .= "   Previous owner was $bug_fields{'assigned_to'}.\n";
    } else {
      $err .= "   Previous owner is unknown.\n";
    }
  }

  my @resolution;
  if (defined ($bug_fields{'resolution'}) &&
       (@resolution= grep /^$bug_fields{'resolution'}$/i, @::legal_resolution) ){
    $values .= SqlQuote($resolution[0]) . ",\n";
    $query .= "resolution,\n";
  } elsif ( (defined $bug_fields{'resolution'}) && (!$resolution[0]) ){
    $err .= "Unknown resolution \"$bug_fields{'resolution'}\".\n";
  }

  # if the bug's owner changed, mark the bug NEW, unless a valid 
  # resolution is set, which indicates that the bug should be closed.
  #
  if ( ($changed_owner) && (!$resolution[0]) ) {
    $values .= "\'NEW\',\n";
    $query .= "bug_status,\n";
    $err .= "Bug assigned to new owner, setting status to \"NEW\".\n";
    $err .= "   Previous status was \"";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= "\".\n";
  } elsif ( (defined ($bug_fields{'resolution'})) && (!$resolution[0]) ){
    #if the resolution was illegal then set status to NEW
    $values .= "\'NEW\',\n";
    $query .= "bug_status,\n";
    $err .= "Resolution was invalid. Setting status to \"NEW\".\n";
    $err .= "   Previous status was \"";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= "\".\n";
  } elsif (defined ($bug_fields{'bug_status'}) &&
       (my @status = grep /^$bug_fields{'bug_status'}$/i, @::legal_bug_status) ){
    #if a bug status was set then use it, if its legal
    $values .= SqlQuote($status[0]) . ",\n";
    $query .= "bug_status,\n";
  } else {
    # if all else fails, make the bug new
    $values .= "\'NEW\',\n";
    $query .= "bug_status,\n";
    $err .= "Unknown status ";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= ". Setting to default status \"NEW\".\n";
  }

  if (Param("useqacontact")) {
    my $qa_contact;
    if ( (defined $bug_fields{'qa_contact'}) &&
         ($qa_contact  = DBname_to_id($bug_fields{'qa_contact'})) ){
      $values .= "'$qa_contact'";
      $query .= "qa_contact\n";
    } else {
      SendSQL("select initialqacontact from components where program=" .
              SqlQuote($product[0]) .
              " and value=" . SqlQuote($component[0]) );
      $qa_contact = FetchOneColumn();
      $values .= SqlQuote(DBname_to_id($qa_contact)) . "\n";
      $query .= "qa_contact\n";
      $err .= "Setting qa contact to the default for this product.\n";
      $err .= "   This bug either had no qa contact or an invalid one.\n";
    }
  }

  $query .= ") $values )\n";
  SendSQL($query);
  SendSQL("select LAST_INSERT_ID()");
  my $id = FetchOneColumn();

  if (defined $bug_fields{'cc'}) {
    foreach my $person (split(/[ ,]/, $bug_fields{'cc'})) {
      my $uid;
      if ( ($person ne "") && ($uid = DBname_to_id($person)) ) {
        SendSQL("insert into cc (bug_id, who) values ($id, " . SqlQuote($uid) .")");
      }
    }
  }


  $long_description .= "\n" . $comments;
  if ($err) {
    $long_description .= "\n$err\n";
  }

  SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
    "($id, $exporterid, now(), " . SqlQuote($long_description) . ")");

  print "Bug $bug_fields{'bug_id'}\@$urlbase ";
  print "imported as bug $id.\n";
}
