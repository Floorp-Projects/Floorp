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

#####################################################################
#
# This script is used import bugs from another installation of bugzilla.
# Moving a bug on another system will send mail to an alias provided by
# the administrator of the target installation (you). Set up an alias
# similar to the one given below so this mail will be automatically 
# run by this script and imported into your database.  Run 'newaliases'
# after adding this alias to your aliases file. Make sure your sendmail
# installation is configured to allow mail aliases to execute code. 
#
# bugzilla-import: "|/usr/bin/perl /opt/bugzilla/importxml.pl"
#
#####################################################################


# figure out which path this script lives in. Set the current path to
# this and add it to @INC so this will work when run as part of mail
# alias by the mailer daemon
# since "use lib" is run at compile time, we need to enclose the
# $::path declaration in a BEGIN block so that it is executed before
# the rest of the file is compiled.
BEGIN {
 $::path = $0;
 $::path =~ m#(.*)/[^/]+#;
 $::path = $1;
 $::path ||= '.';  # $0 is empty at compile time.  This line will
                   # have no effect on this script at runtime.
}

chdir $::path;
use lib ($::path);

use XML::Parser;
use Data::Dumper;
$Data::Dumper::Useqq = 1;

require "CGI.pl";
require "globals.pl";
$::lockcount = 0;

GetVersionTable();
ConnectToDatabase();


sub sillyness {
    my $zz;
    $zz = $Data::Dumper::Useqq;
    $zz = %::components;
    $zz = %::versions;
    $zz = %::keywordsbyname;
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

sub MailMessage {
  my $subject = shift @_;
  my $message = shift @_;
  my @recipients = @_;

  my $to = join (", ", @recipients);
  my $header = "To: $to\n";
  my $from = Param("moved-from-address");
  $from =~ s/@/\@/g;
  $header.= "From: Bugzilla <$from>\n";
  $header.= "Subject: $subject\n\n";

  open(SENDMAIL,
    "|/usr/lib/sendmail -ODeliveryMode=background -t") ||
      die "Can't open sendmail";
  print SENDMAIL $header . $message . "\n";
  close SENDMAIL;

  Log($subject . " sent to: $to");
}


sub Log {
    my ($str) = (@_);
    Lock();
    open(FID, ">>data/maillog") || die "Can't write to data/maillog";
    print FID time2str("%D %H:%M", time()) . ": $str\n";
    close FID;
    Unlock();
}

sub Lock {
    if ($::lockcount <= 0) {
        $::lockcount = 0;
        if (!open(LOCKFID, ">>data/maillock")) {
            mkdir "data", 0777;
            chmod 0777, "data";
            open(LOCKFID, ">>data/maillock") || die "Can't open lockfile.";
        }
        my $val = flock(LOCKFID,2);
        if (!$val) { # '2' is magic 'exclusive lock' const.
            print "Content-type: text/html\n\n";
            print "Lock failed: $val\n";
        }
        chmod 0666, "data/maillock";
    }
    $::lockcount++;
}

sub Unlock {
    $::lockcount--;
    if ($::lockcount <= 0) {
        flock(LOCKFID,8);       # '8' is magic 'unlock' const.
        close LOCKFID;
    }
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
  my $subject = "Bug import error: no maintainer";
  my $message = "Cannot import these bugs because no maintainer for "; 
  $message .=   "the exporting db is given.\n";
  $message .=   "\n\nPlease re-open the original bug.\n";
  $message .= "\n\n$xml";
  my @to = (Param("maintainer"));
  MailMessage ($subject, $message, @to);
  exit;
}

my $exporter;
if (defined $tree->[1][0]->{'exporter'}) {
  $exporter = $tree->[1][0]->{'exporter'};
} else {
  my $subject = "Bug import error: no exporter";
  my $message = "Cannot import these bugs because no exporter is given.\n";
  $message .=   "\n\nPlease re-open the original bug.\n";
  $message .= "\n\n$xml";
  my @to = (Param("maintainer"), $maintainer);
  MailMessage ($subject, $message, @to);
  exit;
}


unless ( Param("move-enabled") ) {
  my $subject = "Error: bug importing is disabled here";
  my $message = "Cannot import these bugs because importing is disabled\n";
  $message .= "at this site. For more info, contact ";
  $message .=  Param("maintainer") . ".\n";
  my @to = (Param("maintainer"), $maintainer, $exporter);
  MailMessage ($subject, $message, @to);
  exit;
}

my $exporterid = DBname_to_id($exporter);
if ( ! $exporterid ) {
  my $subject = "Bug import error: invalid exporter";
  my $message = "The user <$tree->[1][0]->{'exporter'}> who tried to move\n";
  $message .= "bugs here does not have an account in this database.\n";
  $message .= "\n\nPlease re-open the original bug.\n";
  $message .= "\n\n$xml";
  my @to = (Param("maintainer"), $maintainer, $exporter);
  MailMessage ($subject, $message, @to);
  exit;
}

my $urlbase;
if (defined $tree->[1][0]->{'urlbase'}) {
  $urlbase= $tree->[1][0]->{'urlbase'}; 
} else {
  my $subject = "Bug import error: invalid exporting database";
  my $message = "Cannot import these bugs because the name of the exporting db was not given.\n";
  $message .= "\n\nPlease re-open the original bug.\n";
  $message .= "\n\n$xml";
  my @to = (Param("maintainer"), $maintainer, $exporter);
  MailMessage ($subject, $message, @to);
  exit;
}
  

my $bugqty = ($#{@{$tree}->[1]} +1 -3) / 4;
my $log = "Imported $bugqty bug(s) from $urlbase,\n  sent by $exporter.\n\n";
for (my $k=1 ; $k <= $bugqty ; $k++) {
  my $cur = $k*4;

  if (defined $tree->[1][$cur][0]->{'error'}) {
    $log .= "\nError in bug $tree->[1][$cur][4][2]\@$urlbase:";
    $log .= " $tree->[1][$cur][0]->{'error'}\n";
    if ($tree->[1][$cur][0]->{'error'} =~ /NotFound/) {
      $log .= "$exporter tried to move bug $tree->[1][$cur][4][2] here";
      $log .= " but $urlbase reports that this bug does not exist.\n"; 
    } elsif ( $tree->[1][$cur][0]->{'error'} =~ /NotPermitted/) {
      $log .= "$exporter tried to move bug $tree->[1][$cur][4][2] here";
      $log .= " but $urlbase reports that $exporter does not have access";
      $log .= " to that bug.\n";
    }
    next;
  }

  my %multiple_fields;
  foreach my $field (qw (dependson cc long_desc blocks)) {
    $multiple_fields{$field} = "x"; 
  }
  my %all_fields;
  foreach my $field (qw (dependson product bug_status priority cc version 
      bug_id rep_platform short_desc assigned_to bug_file_loc resolution
      delta_ts component reporter urlbase target_milestone bug_severity 
      creation_ts qa_contact keywords status_whiteboard op_sys blocks)) {
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
    $long_description .=  UnQuoteXMLChars($sorted_descs[$z]->{'thetext'});
    $long_description .=  "\n";
  }

  my $comments;

  $comments .= "\n\n------- Bug moved to this database by $exporter "; 
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

  my @query = ();
  my @values = ();
  foreach my $field ( qw(creation_ts delta_ts status_whiteboard) ) {
      if ( (defined $bug_fields{$field}) && ($bug_fields{$field}) ){
        push (@query, "$field");
        push (@values, SqlQuote($bug_fields{$field}));
      }
  }

  if ( (defined $bug_fields{'bug_file_loc'}) && ($bug_fields{'bug_file_loc'}) ){
      push (@query, "bug_file_loc");
      push (@values, SqlQuote(UnQuoteXMLChars($bug_fields{'bug_file_loc'})));
      }

  if ( (defined $bug_fields{'short_desc'}) && ($bug_fields{'short_desc'}) ){
      push (@query, "short_desc");
      push (@values, SqlQuote(UnQuoteXMLChars($bug_fields{'short_desc'})) );
      }


  my $prod;
  my $comp;
  my $default_prod = Param("moved-default-product");
  my $default_comp = Param("moved-default-component");
  if ( (defined ($bug_fields{'product'})) &&
       (defined ($bug_fields{'component'})) ) {
     $prod = $bug_fields{'product'};
     $comp = $bug_fields{'component'};
  } else {
     $prod = $default_prod;
     $comp = $default_comp;
  }

  my @product;
  my @component;
  if ((@product = grep /^$prod$/i, @::legal_product) &&
      (@component = grep /^$comp$/i, @{$::components{$product[0]}}) ) {
    push (@query, "product");
    push (@values, SqlQuote($product[0]) );
    push (@query, "component");
    push (@values, SqlQuote($component[0]) );
  } elsif ((@product = grep /^$default_prod$/i, @::legal_product) &&
      (@component = grep /^$default_comp$/i, @{$::components{$product[0]}}) ) {
    push (@query, "product");
    push (@values, SqlQuote($product[0]) );
    push (@query, "component");
    push (@values, SqlQuote($component[0]) );
  } else {
    my $subject = "Bug import error: invalid default product or component";
    my $message = "Cannot import these bugs because an invalid default ";
    $message .= "product and/or component was defined for the target db.\n";
    $message .= Param("maintainer") . " needs to fix the definitions of ";
    $message .= "moved-default-product and moved-default-component.\n";
    $message .= "\n\nPlease re-open the original bug.\n";
    $message .= "\n\n$xml";
    my @to = (Param("maintainer"), $maintainer, $exporter);
    MailMessage ($subject, $message, @to);
    exit;
  }

  if (defined  ($::versions{$product[0]} ) &&
     (my @version = grep /^$bug_fields{'version'}$/i, 
                         @{$::versions{$product[0]}}) ){
    push (@values, SqlQuote($version[0]) );
    push (@query, "version");
  } else {
    push (@query, "version");
    push (@values, SqlQuote(@{$::versions{$product[0]}}->[0]));
    $err .= "Unknown version $bug_fields{'version'} in product $product[0]. ";
    $err .= "Setting version to \"@{$::versions{$product[0]}}->[0]\".\n";
  }

  if (defined ($bug_fields{'priority'}) &&
       (my @priority = grep /^$bug_fields{'priority'}$/i, @::legal_priority) ){
    push (@values, SqlQuote($priority[0]) );
    push (@query, "priority");
  } else {
    push (@values, SqlQuote("P3"));
    push (@query, "priority");
    $err .= "Unknown priority ";
    $err .= (defined $bug_fields{'priority'})?$bug_fields{'priority'}:"unknown";
    $err .= ". Setting to default priority \"P3\".\n";
  }

  if (defined ($bug_fields{'rep_platform'}) &&
       (my @platform = grep /^$bug_fields{'rep_platform'}$/i, @::legal_platform) ){
    push (@values, SqlQuote($platform[0]) );
    push (@query, "rep_platform");
  } else {
    push (@values, SqlQuote("Other") );
    push (@query, "rep_platform");
    $err .= "Unknown platform ";
    $err .= (defined $bug_fields{'rep_platform'})?
                     $bug_fields{'rep_platform'}:"unknown";
    $err .= ". Setting to default platform \"Other\".\n";
  }

  if (defined ($bug_fields{'op_sys'}) &&
     (my @opsys = grep /^$bug_fields{'op_sys'}$/i, @::legal_opsys) ){
    push (@values, SqlQuote($opsys[0]) );
    push (@query, "op_sys");
  } else {
    push (@values, SqlQuote("other"));
    push (@query, "op_sys");
    $err .= "Unknown operating system ";
    $err .= (defined $bug_fields{'op_sys'})?$bug_fields{'op_sys'}:"unknown";
    $err .= ". Setting to default OS \"other\".\n";
  }

  if (Param("usetargetmilestone")) {
    if (defined  ($::target_milestone{$product[0]} ) &&
       (my @tm = grep /^$bug_fields{'target_milestone'}$/i, 
                       @{$::target_milestone{$product[0]}}) ){
      push (@values, SqlQuote($tm[0]) );
      push (@query, "target_milestone");
    } else {
      SendSQL("SELECT defaultmilestone FROM products " .
              "WHERE product = " . SqlQuote($product[0]));
      my $tm = FetchOneColumn();
      push (@values, SqlQuote($tm));
      push (@query, "target_milestone");
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
    push (@values, SqlQuote($severity[0]) );
    push (@query, "bug_severity");
  } else {
    push (@values, SqlQuote("normal"));
    push (@query, "bug_severity");
    $err .= "Unknown severity ";
    $err .= (defined $bug_fields{'bug_severity'})?
                     $bug_fields{'bug_severity'}:"unknown";
    $err .= ". Setting to default severity \"normal\".\n";
  }

  my $reporterid = DBname_to_id($bug_fields{'reporter'});
  if ( ($bug_fields{'reporter'}) && ( $reporterid ) ) {
    push (@values, SqlQuote($reporterid));
    push (@query, "reporter");
  } else {
    push (@values, SqlQuote($exporterid));
    push (@query, "reporter");
    $err .= "The original reporter of this bug does not have\n";
    $err .= "   an account here. Reassigning to the person who moved\n";
    $err .= "   it here, $exporter.\n";
    if ( $bug_fields{'reporter'} ) {
      $err .= "   Previous reporter was $bug_fields{'reporter'}.\n";
    } else {
      $err .= "   Previous reporter is unknown.\n";
    }
  }

  my $changed_owner = 0;
  if ( ($bug_fields{'assigned_to'}) && 
       ( DBname_to_id($bug_fields{'assigned_to'})) ) {
    push (@values, SqlQuote(DBname_to_id($bug_fields{'assigned_to'})));
    push (@query, "assigned_to");
  } else {
    push (@values, SqlQuote($exporterid) );
    push (@query, "assigned_to");
    $changed_owner = 1;
    $err .= "The original owner of this bug does not have\n";
    $err .= "   an account here. Reassigning to the person who moved\n";
    $err .= "   it here, $exporter.\n";
    if ( $bug_fields{'assigned_to'} ) {
      $err .= "   Previous owner was $bug_fields{'assigned_to'}.\n";
    } else {
      $err .= "   Previous owner is unknown.\n";
    }
  }

  my @resolution;
  if (defined ($bug_fields{'resolution'}) &&
       (@resolution= grep /^$bug_fields{'resolution'}$/i, @::legal_resolution) ){
    push (@values, SqlQuote($resolution[0]) );
    push (@query, "resolution");
  } elsif ( (defined $bug_fields{'resolution'}) && (!$resolution[0]) ){
    $err .= "Unknown resolution \"$bug_fields{'resolution'}\".\n";
  }

  # if the bug's owner changed, mark the bug NEW, unless a valid 
  # resolution is set, which indicates that the bug should be closed.
  #
  if ( ($changed_owner) && (!$resolution[0]) ) {
    push (@values, SqlQuote("NEW"));
    push (@query, "bug_status");
    $err .= "Bug assigned to new owner, setting status to \"NEW\".\n";
    $err .= "   Previous status was \"";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= "\".\n";
  } elsif ( (defined ($bug_fields{'resolution'})) && (!$resolution[0]) ){
    #if the resolution was illegal then set status to NEW
    push (@values, SqlQuote("NEW"));
    push (@query, "bug_status");
    $err .= "Resolution was invalid. Setting status to \"NEW\".\n";
    $err .= "   Previous status was \"";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= "\".\n";
  } elsif (defined ($bug_fields{'bug_status'}) &&
       (my @status = grep /^$bug_fields{'bug_status'}$/i, @::legal_bug_status) ){
    #if a bug status was set then use it, if its legal
    push (@values, SqlQuote($status[0]));
    push (@query, "bug_status");
  } else {
    # if all else fails, make the bug new
    push (@values, SqlQuote("NEW"));
    push (@query, "bug_status");
    $err .= "Unknown status ";
    $err .= (defined $bug_fields{'bug_status'})?
                     $bug_fields{'bug_status'}:"unknown";
    $err .= ". Setting to default status \"NEW\".\n";
  }

  if (Param("useqacontact")) {
    my $qa_contact;
    if ( (defined $bug_fields{'qa_contact'}) &&
         ($qa_contact  = DBname_to_id($bug_fields{'qa_contact'})) ){
      push (@values, SqlQuote($qa_contact));
      push (@query, "qa_contact");
    } else {
      SendSQL("select initialqacontact from components where program=" .
              SqlQuote($product[0]) .
              " and value=" . SqlQuote($component[0]) );
      $qa_contact = FetchOneColumn();
      push (@values, SqlQuote(DBname_to_id($qa_contact)) );
      push (@query, "qa_contact");
      $err .= "Setting qa contact to the default for this product.\n";
      $err .= "   This bug either had no qa contact or an invalid one.\n";
    }
  }


  my $query  = "INSERT INTO bugs (\n" 
               . join (",\n", @query)
               . "\n) VALUES (\n"
               . join (",\n", @values)
               . "\n)\n";
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

  if (defined ($bug_fields{'keywords'})) {
    my %keywordseen;
    foreach my $keyword (split(/[\s,]+/, $bug_fields{'keywords'})) {
      if ($keyword eq '') {
        next;
      }
      my $i = $::keywordsbyname{$keyword};
      if (!$i) {
        $err .= "Skipping unknown keyword: $keyword.\n";
        next;
      }
      if (!$keywordseen{$i}) {
        SendSQL("INSERT INTO keywords (bug_id, keywordid) VALUES ($id, $i)");
        $keywordseen{$i} = 1;
      }
    }
  }

  $long_description .= "\n" . $comments;
  if ($err) {
    $long_description .= "\n$err\n";
  }

  SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
    "($id, $exporterid, now(), " . SqlQuote($long_description) . ")");

  $log .= "Bug $urlbase/show_bug.cgi?id=$bug_fields{'bug_id'} ";
  $log .= "imported as bug $id.\n";
  $log .= Param("urlbase") . "/show_bug.cgi?id=$id\n\n";
  if ($err) {
    $log .= "The following problems were encountered creating bug $id.\n";
    $log .= "You may have to set certain fields in the new bug by hand.\n\n";
    $log .= $err;
    $log .= "\n\n\n";
  }

  system("./processmail", $id, $exporter);
}

my $subject = "$bugqty bug(s) successfully moved from $urlbase to " 
               . Param("urlbase") ;
my @to = ($exporter);
MailMessage ($subject, $log, @to);
