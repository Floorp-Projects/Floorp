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
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global variables and routines used throughout bugzilla.

$::TreeID = "default";

use strict;
use DBI;
use File::Path;

use Date::Format;               # For time2str().
use Date::Parse;                # For str2time().

use Mail::Mailer;
use Mail::Internet;
use Mail::Header;

$ENV{'MAILADDRESS'} = Param('maintainer');

# use Carp;                       # for confess

# Contains the version string for the current running Bonsai
$::param{'version'} = '1.3';



##
##  Routines to deal with Customizable Parameter Lists
##
sub PerformSubsts {
     my ($str, $substs) = (@_);

     $str =~
         s/%([_a-z]*)%/(defined($substs->{$1}) ? $substs->{$1} : Param($1))/eg;

     return $str;
}


sub ReturnSubsts {
     my ($str, $subst) = @_;

     $str = PerformSubsts($str, undef) if $subst;
     return $str;
}

sub Param {
     my ($value, $subst) = (@_);

     if (defined $::param{$value}) {
          return ReturnSubsts($::param{$value}, $subst);
     }

     # Um, maybe we haven't sourced in the params at all yet.
     if (stat("data/params")) {
          # Write down and restore the version # here.  That way, we
          # get around anyone who maliciously tries to tweak the
          # version number by editing the params file.  Not to mention
          # that in 2.0, there was a bug that wrote the version number
          # out to the params file...
          my $v = $::param{'version'};
          require "data/params";
          $::param{'version'} = $v;
     }
     if (defined $::param{$value}) {
          return ReturnSubsts($::param{$value}, $subst);
     }

     # Well, that didn't help.  Maybe it's a new param, and the user
     # hasn't defined anything for it.  Try and load a default value
     # for it.
     require "defparams.pl";
     WriteParams();
     if (defined $::param{$value}) {
          return ReturnSubsts($::param{$value}, $subst);
     }

     # We're pimped.
     die "Can't find param named $value";
}



##
##  Routines to handle the mechanics of connecting to the database
##
sub ConnectToDatabase {
    my ($dsn);

    if (!defined $::db) {
        $dsn = Param('dbiparam');

#        DBI->trace(1, "/tmp/dbi.out");

	$::db = DBI->connect($dsn, Param('mysqluser'), Param('mysqlpassword'))
            || die "Can't connect to database server.";
    }
}

sub DisconnectFromDatabase {
    if (defined $::db) {
        $::db->disconnect();
        undef $::db;
    }
}

sub SendSQL {
    my ($str) = (@_);
    my ($rows);

    $::currentquery = $::db->prepare($str)
	|| die "'$str': ". $::db->errstr;
    $rows = $::currentquery->execute
        || die "'$str': Can't execute the query: " . $::currentquery->errstr;
}

sub MoreSQLData {
    if (defined @::fetchahead) {
	return 1;
    }
    if (@::fetchahead = $::currentquery->fetchrow()) {
	return 1;
    }
    return 0;
}

sub FetchSQLData {
    if (defined @::fetchahead) {
	my @result = @::fetchahead;
	undef @::fetchahead;
	return @result;
    }
    return $::currentquery->fetchrow();
}


sub FetchOneColumn {
    my @row = FetchSQLData();
    return $row[0];
}


# This routine is largely copied from Mysql.pm.

sub SqlQuote {
    my ($str) = (@_);
#     if (!defined $str) {
#         confess("Undefined passed to SqlQuote");
#     }
    $str =~ s/([\\\'])/\\$1/g;
    $str =~ s/\0/\\0/g;
    return "'$str'";
}



# Fills in a hashtable with info about the columns for the given table in the
# database.  The hashtable has the following entries:
#   -list-  the list of column names
#   <name>,type  the type for the given name

sub LearnAboutColumns {
    my ($table) = (@_);
    my %a;
    SendSQL("show columns from $table");
    my @list = ();
    my @row;
    while (@row = FetchSQLData()) {
        my ($name,$type) = (@row);
        $a{"$name,type"} = $type;
        push @list, $name;
    }
    $a{"-list-"} = \@list;
    return \%a;
}



# If the above returned a enum type, take that type and parse it into the
# list of values.  Assumes that enums don't ever contain an apostrophe!

sub SplitEnumType {
    my ($str) = (@_);
    my @result = ();
    if ($str =~ /^enum\((.*)\)$/) {
        my $guts = $1 . ",";
        while ($guts =~ /^\'([^\']*)\',(.*)$/) {
            push @result, $1;
            $guts = $2;
	}
    }
    return @result;
}


##
##  Routines to generate perl code that will reinitialize variables
##  correctly when eval'ed
##


# Generate a string which, when later interpreted by the Perl compiler, will
# be the same as the given string.
sub PerlQuote {
    my ($str) = (@_);

    $str = SqlQuote($str);

    return $str;
}

sub GenerateArrayCode {
    my ($ref) = (@_);
    my @list;
    foreach my $i (@$ref) {
        push @list, PerlQuote($i);
    }
    return join(',', @list);
}


# Given the name of a global variable, generate Perl code that, if later
# executed, would restore the variable to its current value.

sub GenerateCode {
    my ($name) = (@_);
    my $result = $name . " = ";
    if ($name =~ /^\$/) {
        my $value = eval($name);
        if (ref($value) eq "ARRAY") {
            $result .= "[" . GenerateArrayCode($value) . "]";
        } else {
            $result .= PerlQuote(eval($name));
        }
    } elsif ($name =~ /^@/) {
        my @value = eval($name);
        $result .= "(" . GenerateArrayCode(\@value) . ")";
    } elsif ($name =~ '%') {
        $result = "";
        foreach my $k (sort { uc($a) cmp uc($b)} eval("keys $name")) {
            $result .= GenerateCode("\$" . substr($name, 1) .
                                    "{'" . $k . "'}");
        }
        return $result;
    } else {
        die "Can't do $name -- unacceptable variable type.";
    }
    $result .= ";\n";
    return $result;
}


##
##  Locking and Logging routines
##
$::lockcount = 0;

sub Lock {
    if ($::lockcount <= 0) {
        $::lockcount = 0;
        if (!open(LOCKFID, ">>data/lock")) {
            mkdir "data", 0777;
            chmod 0777, "data";
            open(LOCKFID, ">>data/lock") || die "Can't open lockfile.";
        }
        my $val = flock(LOCKFID,2);
        if (!$val) { # '2' is magic 'exclusive lock' const.
            print "Lock failed: $val\n";
        }
        chmod 0666, "data/lock";
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

sub LockOpen {
     my ($fhandle, $file, $errstr) = @_;

     Lock();
     unless (open($fhandle, $file)) {
          my $errmsg = $!;
          Unlock();
          die "$errstr: $errmsg";
     }
}

sub Log {
    my ($str) = (@_);
    local (*LOGFID);

    LockOpen(\*LOGFID, ">>data/log", "Can't write to data/log");
    print LOGFID time2str("%Y-%m-%d %H:%M", time()) . ": $str\n";
    close LOGFID;
    chmod 0666, "data/log";
    Unlock();
}


my %lastidcache;
undef %lastidcache;

sub GetId {
     my ($table, $field, $value) = @_;
     my ($index, $qvalue, $id);

     $index = "$table|$field|$value";
     return ($lastidcache{$index})
          if (exists $lastidcache{$index});

     $qvalue = SqlQuote($value);
     SendSQL("select id from $table where $field = $qvalue");
     ($id) = FetchSQLData();

     unless ($id) {
         SendSQL("insert into $table ($field) values ($qvalue)");
         SendSQL("select LAST_INSERT_ID()");
         ($id) = FetchSQLData();
     }

     return ($lastidcache{$index} = $id);
}


sub GetId_NoCache {
     my ($table, $field, $value) = @_;
     my ($qvalue, $id);

     $qvalue = SqlQuote($value);
     SendSQL("select id from $table where $field = $qvalue");
     ($id) = FetchSQLData();

     unless ($id) {
         SendSQL("insert into $table ($field) values ($qvalue)");
         SendSQL("select LAST_INSERT_ID()");
         ($id) = FetchSQLData();
     }

     return $id;
}


sub MakeValueHash {
     my ($value) = @_;
     my ($len, $hash);

     $len = length($value);
     return $len;

#
# Using a check sum was slightly slower than just using the string
# length...
#

#
#     $hash = ($len & 0x2ff) << 22;
#     $hash += unpack("%16C*", $value);
#     return $hash;
}

sub GetHashedId {
     my ($table, $field, $value) = @_;
     my ($qvalue, $id, $hash);

     $hash = MakeValueHash($value);
     $qvalue = SqlQuote($value);
     SendSQL("select id from $table where hash = $hash and $field = $qvalue");
     ($id) = FetchSQLData();

     unless ($id) {
         SendSQL("insert into $table (hash, $field) values ($hash, $qvalue)");
         SendSQL("select LAST_INSERT_ID()");
         ($id) = FetchSQLData();
     }

     return $id;

}


my $lastdescription;
my $lastdescriptionid = 0;
undef $lastdescription;

sub AddToDatabase {
     my ($lines, $desc) = @_;
     my ($descid, $basequery, $query, $line, $quoted);
     my ($chtype, $date, $name, $repository, $dir);
     my ($file, $version, $sticky, $branch, $addlines, $removelines);

     # Truncate/cleanup description
     $desc = substr($desc, 0, 60000)
          if (length($desc) > 60000);
     $desc =~ s/\s+$//;

     # Find a description id
     $descid = $lastdescriptionid;
     if (!defined($lastdescription) || ($desc ne $lastdescription)) {
          undef $descid;
     }

     # Build the query...
     $basequery = "replace into
                      checkins(
                          type, ci_when, whoid, repositoryid, dirid,
                          fileid, revision, stickytag, branchid, addedlines,
                          removedlines, descid)
                      values (";

     foreach $line (split(/\n/, $lines)) {
          next if ($line =~ /^\s*$/);

          ($chtype, $date, $name, $repository, $dir, $file, $version,
           $sticky, $branch, $addlines, $removelines) =
                split(/\s*\|\s*/, $line);

          Log("line <$line>");

          # Clean up a few variables...
          $branch =~ s/^T//;
          $dir =~ s!/$!!;
          $dir =~ s!^\./!!;
          $addlines = 0 if (!defined($addlines) || $addlines =~ /^\s*$/);
          $removelines = 0 if (!defined($removelines) || 
                               $removelines =~ /^\s*$/);
          $removelines = abs($removelines);
          $date = time2str("%Y/%m/%d %H:%M", $date)
               if ($date =~ /^[+-]?\d+$/);

          # Find the description id, if it isn't already set
          if (!defined($descid)) {
               $descid = GetHashedId('descs', 'description', $desc);
               $quoted = SqlQuote($desc);
               $lastdescriptionid = $descid;
               $lastdescription = $desc;
          }

          # Build the final query
          $query = $basequery;
          if ($chtype eq "C") {
               $query .= "'Change'";
          } elsif ($chtype eq "A") {
               $query .= "'Append'";
          } elsif ($chtype eq "R") {
               $query .= "'Remove'";
          } else {
               $query .= "NULL";
          }

          $query .= ", '$date'";
          $query .= ", " . GetId("people", "who", $name);
          $query .= ", " . GetId("repositories", "repository", $repository);
          $query .= ", " . GetId("dirs", "dir", $dir);
          $query .= ", " . GetId("files", "file", $file);
          $query .= ", " . SqlQuote($version);
          $query .= ", " . SqlQuote($sticky);
          $query .= ", " . GetId("branches", "branch", $branch);
          $query .= ", $addlines";
          $query .= ", $removelines";
          $query .= ", $descid)";

          SendSQL($query);
     }
}

sub assert {
    print "assert(expr)" if @_ != 1;
    if (!$_[0]) {
	die "Assertion failed";
    }
}

sub lsearch {
    my ($list,$item) = (@_);
    my $count = 0;
    foreach my $i (@$list) {
        if ($i eq $item) {
            return $count;
        }
        $count++;
    }
    return -1;
}

sub DataDir {
     my $dir;

     if ($::TreeID eq "default") {
          $dir = "data";
     } else {
          $dir = "data/$::TreeID";
     }

     # Make sure it exists...
     unless (-d $dir) {
         rmtree([$dir], 1, 1);
         mkpath([$dir], 0, 0777);
         die "Couldn't create '$dir'\n"
             unless (-d $dir);
     }

     return $dir;
}


sub LoadTreeConfig {
    return
         if (@::TreeList > 0);

    require 'data/treeconfig.pl';
}


sub LoadDirList {
     my ($modules, $dirsfile);

     undef @::LegalDirs;
     LoadTreeConfig() unless (@::TreeList > 0);

     $modules = $::TreeInfo{$::TreeID}{'repository'} . "/CVSROOT/modules";
     $dirsfile = DataDir() . "/legaldirs";

     if (-f $modules) {
          if ((!(-f $dirsfile)) ||
              ((-M $dirsfile) > (-M $modules))) {
               system("./createlegaldirs.pl", $::TreeID);
          }
     }

     LockOpen(\*DIRS, "$dirsfile", "Couldn't open $dirsfile");
     while (<DIRS>) {
          chop;
          push @::LegalDirs, $_;
     }
     close(DIRS);
     Unlock();
}



sub PickNewBatchID {
     my ($batchfile);

     $::BatchID++;
     $batchfile = DataDir() . "/batchid.pl";

     LockOpen(\*BATCH, "> $batchfile", "Couldn't write $batchfile");
     print BATCH GenerateCode('$::BatchID');
     close(BATCH);
     Unlock();
}


sub LoadBatchID {
     my ($batchfile);

     Lock();
     $batchfile = DataDir() . "/batchid.pl";
     unless (-f $batchfile) {
          LockOpen(\*BATCH, "> $batchfile", "Couldn't write $batchfile");
          print BATCH "\$::BatchID = 1;\n";
          close(BATCH);
          Unlock();
     }
     require "$batchfile";
     Unlock();
}


sub LoadCheckins {
     my ($filename, $base_ts);

     return if (@::CheckInList);

     Lock();
     LoadBatchID() unless ($::BatchID);

     $filename = DataDir() . sprintf("/batch-%d.pl", $::BatchID);
     require "$filename" if (-f $filename);
     Unlock();

     $::TreeOpen = 1 unless (defined($::TreeOpen));

     $base_ts = 28800;          # Which is "1/1/70".
     $::LastGoodTimeStamp = $base_ts unless ($::LastGoodTimeStamp);
     $::CloseTimeStamp    = $base_ts unless ($::CloseTimeStamp);
}



sub WriteCheckins {
     my ($filename, $filedest, $i, $checkin);
     my (%person, $name);

     if (Param('readonly')) {
          print "
<P><B><font color=red>
    Can't write checkins file; not viewing current info.
</font></b>\n\n";
          return;
     }

     $filename = DataDir() . "/tmp-$$";
     unless (open(TEMP, "> $filename")) {
          print "
<P><B><font color=red>
    Can't write checkins file.  Temp file '$filename' unwriteable: $!
</font></b>\n\n";
          return;
     }
     chmod(0666, $filename);

     undef(%person);

     foreach $i ('TreeOpen', 'LastGoodTimeStamp', 'CloseTimeStamp') {
          print TEMP GenerateCode("\$::$i");
     }
     print TEMP GenerateCode('@::CheckInList');
     foreach $checkin (@::CheckInList) {
          my $info = eval("\\\%$checkin");

          print TEMP GenerateCode("\%$checkin");
          $person{$$info{'person'}} = 1;
     }
     print TEMP "1;\n";
     close(TEMP);

     Lock();
     $filedest = DataDir() . "/batch-$::BatchID.pl";
     unlink($filedest);
     rename($filename, $filedest);

     open(TEMP, "> $filename");
     chmod(0666, $filename);
     foreach $name (sort(keys(%person))) {
          print TEMP EmailFromUsername($name) . "\n";
     }
     print TEMP EmailFromUsername(Param("bonsai-hookinterest", 1)) . "\n";
     close(TEMP);
     rename($filename, DataDir() . "/hooklist");
     Unlock();
}


# Trim whitespace from front and back.

sub trim {
    ($_) = (@_);
    s/^\s+//g;
    s/\s+$//g;
    return $_;
}

sub ConstructMailTo {
     my ($name, $subject) = @_;

     return "<a href=\"mailto:$name?subject=$subject\">Send mail to $name</a>";
}

sub MyFmtClock {
     my ($time) = @_;

     $time = 1 * 365 * 24 * 60 * 60 + 1 if ($time <= 0);
     return time2str("%Y-%m-%d %T", $time);
}

sub SqlFmtClock {
     my ($time) = @_;

     $time = 1 * 365 * 24 * 60 * 60 + 1 if ($time <= 0);
     return time2str("%Y-%m-%d %T", $time);
}

sub LoadMOTD {
     my $motd_file = DataDir() . "/motd.pl";

     Lock();
     $::MOTD = '';
     eval `cat $motd_file`
          if (-f $motd_file);
     Unlock();
}

sub WriteMOTD {
     my $motd_file = DataDir() . "/motd.pl";

     LockOpen(\*MOTD, "> $motd_file", "Couldn't create $motd_file");
     chmod(0666, $motd_file);
     print MOTD GenerateCode('$::MOTD');
     close(MOTD);
     Unlock();
}


sub LoadWhiteboard {
     my $wb_file = DataDir() . "/whiteboard";

     $::WhiteBoard = '';
     Lock();
     $::WhiteBoard = `cat $wb_file` if (-f $wb_file);
     Unlock();
     $::OrigWhiteBoard = $::WhiteBoard;
}


sub WriteWhiteboard {
     my $wb_file = DataDir() . "/whiteboard";

     if ($::OrigWhiteBoard ne $::WhiteBoard) {
          LockOpen(\*WB, "> $wb_file", "Couldn't create `$wb_file'");
          print WB "$::WhiteBoard\n";
          close(WB);
          chmod(0666, $wb_file);
          Unlock();
     }
}

sub Pluralize {
     my ($str, $num) = @_;

     return ($str) if ($num == 1);
     return ($str . "s");
}


# Given a person's username, get back their full email address.
sub EmailFromUsername {
     my ($name) = @_;
     my ($first, $last);

     $first = '';
     $last = '';
     $name = trim($name);
     if ($name =~ /(.*)<(.*)>(.*)/) {
          $first = "$1<";
          $name = $2;
          $last = ">$3";
     }
     $name =~ s/%/@/;
     $name .= '@' . Param("userdomain", 1)
          unless ($name =~ /\@/);
     $name = $first . trim($name) . $last;
     return $name;
}


# Convert the argument to an array
sub _to_array {
     my ($thing) = @_;

     return @$thing if (ref($thing));
     return ($thing);
}

# Get all of the headers for a mail message, returned as a comma seperated
# string, unless looking for a subject
sub _GetMailHeaders {
     my ($mail_hdrs, $hdr) = @_;
     my (@values, $value, $v);

     $value = '';
     @values = $mail_hdrs->get($hdr);
     foreach $v (@values) {
          chop($v);
          $value .= ',' if ($value && ($hdr ne 'Subject'));
          $value .= ' ' if ($value);
          $value .= $v;
     }

     return $value;
}

# Given a complete mail message as an argument, return the message
# headers in an associative array.
sub ParseMailHeaders {
     my ($mail_txt) = @_;
     my (@mail, %headers);

     %headers = ();
     @mail = map { "$_\n" } split /\n/, $mail_txt;
     my $mail_msg = Mail::Internet->new(\@mail, Modify => 1);
     my $mail_hdrs = $mail_msg->head();

     $headers{'From'}    = _GetMailHeaders($mail_hdrs, 'From');
     $headers{'To'}      = _GetMailHeaders($mail_hdrs, 'To');
     $headers{'CC'}      = _GetMailHeaders($mail_hdrs, 'CC');
     $headers{'Subject'} = _GetMailHeaders($mail_hdrs, 'Subject');

     return %headers;
}


# Given a mail message, return just the mail body as a string
sub FindMailBody {
     my ($mail_txt) = @_;
     my (@mail);

     @mail = map { "$_\n" } split /\n/, $mail_txt;
     my $mail_msg = Mail::Internet->new(\@mail, Modify => 1);
     return join("", _to_array($mail_msg->body()));
}

# Clean up a mail header and return as an array, unless it is the
# subject.  This will split each address and put it in an individual
# array element.  Each address element is cleaned up by making sure
# that it is a valid address by calling EmailFromUsername() to add
# missing domain names.
sub _CleanMailHeader {
     my ($tag, @hdr) = @_;
     my ($val, @rslt, $addr);

     if ($tag eq 'Subject') {
          $val = trim(join(' ', @hdr));
          $val =~ s/\s+/ /g;
          return $val;
     }

     @rslt = ();
     foreach $val (@hdr) {
          next unless $val;
          foreach $addr (split(/,\s+/, $val)) {
               trim($addr);
               $addr = EmailFromUsername($addr);
               push @rslt, $addr;
          }
     }
     return @rslt;
}

# Clean up a header list and make it suitable for use with the
# Mail::Mailer->open() call
sub CleanMailHeaders {
     my (%headers) = @_;

     $headers{'From'}    = [ _CleanMailHeader('From',    $headers{'From'})   ];
     $headers{'CC'}      = [ _CleanMailHeader('CC',      $headers{'CC'})     ];
     $headers{'To'}      = [ _CleanMailHeader('To',      $headers{'To'})     ];
     $headers{'Subject'} =   _CleanMailHeader('Subject', $headers{'Subject'});

     return %headers;
}




# Generate a string to put at the head of a subject of an e-mail.
sub SubjectTag {
     if ($::TreeID eq "default") {
          return "[Bonsai]";
     }
     return "[Bonsai-$::TreeID]";
}

sub MailDiffs {
     my ($name, $oldstr, $newstr) = @_;
     my $old_file = "data/old-$name.$$";
     my $new_file = "data/new-$name.$$";
     my $diff_text = '';

     return
          if ($oldstr eq $newstr);

     open(OLD, "> $old_file") ||
          die "Couldn't create `$old_file': $!";
     print OLD "$oldstr\n";
     close(OLD);

     open(NEW, "> $new_file") ||
          die "Couldn't create `$new_file': $!";
     print NEW "$newstr\n";
     close(NEW);

     $diff_text = `diff -c -b $old_file $new_file`;

     if (length($diff_text) > 3) {
          my $mail_relay = Param("mailrelay");
          my $mailer = Mail::Mailer->new("smtp", Server => $mail_relay);
          my %headers = (
                         From     => Param("bonsai-daemon", 1),
                         To       => Param("bonsai-messageinterest", 1),
                         Subject  => SubjectTag() . " Changes made to $name" 
                        );

          %headers = CleanMailHeaders(%headers);
          $mailer->open(\%headers)
               or warn "Can't send mail: $!\n";
          print $mailer "$diff_text\n";
          $mailer->close();
    }
    unlink($old_file, $new_file);
}


sub PrettyDelta {
     my ($delta) = @_;
     my ($result, $oneday, $onehour, $oneminute);

     $result = '';
     $oneday = 24 * ($onehour = 60 * ($oneminute = 60));

     return " - nochange"
          unless ($delta);

     if ($delta >= $oneday) {
          my $numdays = $delta / $oneday;

          $result .= " $numdays " . Pluralize("day", $numdays);
          $delta = $delta % $oneday;
     }

     if ($delta >= $onehour) {
          my $numhours = $delta / $onehour;

          $result .= " $numhours " . Pluralize("hour", $numhours);
          $delta = $delta % $onehour;
     }

     if ($delta >= $oneminute) {
          my $numminutes = $delta / $oneminute;

          $result .= " $numminutes " . Pluralize("minute", $numminutes);
          $delta = $delta % $oneminute;
     }

     if ($delta > 0) {
          $result .= " $delta " . Pluralize("second", $delta);
     }

     return $result;
}


##
##  Routines to check and verify passwords
##

# Confirm that the given password is right.  If not, generate HTML and exit.

sub CheckGlobalPassword {
     my ($password, $encoded) = @_;

     my $correct = trim(`cat data/passwd`);
     $encoded = crypt($password, "aa")
          unless ($encoded);

     unless ($correct eq $encoded) {
          print "<TITLE>Bzzzzt!</TITLE>

<H1>Invalid password.</h1>
Please click the <b>Back</b> button and try again.";

        Log("Invalid admin password entered.");
        exit 0;
    }
}

sub CheckPassword {
     my ($password) = @_;

     my $encoded = crypt($password, "aa");
     my $pw_file = DataDir() . "/treepasswd";
     my $correct = "xxx $encoded";

     if (-f $pw_file) {
          $correct = trim(`cat $pw_file`);
     }

     CheckGlobalPassword($password, $encoded)
          unless ($correct eq $encoded);
}

sub ParseTimeAndCheck {
     my ($timestr) = @_;
     my $result;

     $result = str2time($timestr);
     unless ($result) {
          print "
<TITLE>Time trap</TITLE>
<H1>Can't parse the time</H1>
You entered a time of '<tt>$timestr</tt>', and I can't understand it.  Please
hit <B>Back</B> and try again.\n";
          exit(0);
     }
     return $result;
}


##
##  Miscelaneous routines from utils.pl
##
sub pickDefaultRepository {
     LoadTreeConfig();
     return $::TreeInfo{$::TreeList[0]}{'repository'};
}


sub getRepositoryList {
     my @result = ();
     my ($i, $r, $j);

     LoadTreeConfig();

TREELOOP:
     foreach $i (@::TreeList) {
          $r = $::TreeInfo{$i}{'repository'};
          foreach $j (@result) {
               if ($j eq $r) {
                    next TREELOOP;
               }
          }
          push @result, $r;
     }
     return @result;
}


sub validateRepository {
    my ($root) = @_;
    my @list = getRepositoryList();
    my $r;

    foreach $r (@list) {
        if ($r eq $root) {
            return;
        }
    }

    my $escaped_root = html_quote($root);
    print "Invalid repository `$escaped_root' selected.\n";
    print ConstructMailTo(Param('maintainer'), "Invalid Repository '$root'");
    print " if you think this should have worked.\n";
    exit;
}

sub formatSqlTime {
    my ($date) = @_;
    my $time = sprintf("%u", $date);

    $time = time2str("%Y/%m/%d %T", $date)
               if ($date =~ /^[+-]?\d+$/);

    return $time;
}


# Returns true if the given directory or filename is one of the hidden ones
# that we don't want to show users.

sub IsHidden {
    my ($name) = (@_);

    $name =~ s:///*:/:g;        # Remove any multiple slashes.
    if (!defined @::HideList) {
        if (open(HIDE, "<data/hidelist")) {
            while (<HIDE>) {
                chop;
                $_ = trim($_);
                s/\#.*//;
                next if (/^$/);

                push(@::HideList, $_);
            }
            close HIDE;
        } else {
            @::HideList = ();
        }
    }
    foreach my $item (@::HideList) {
        if ($name =~ m/$item/) {
            return 1;
        }
    }
    return 0;
}

sub CheckHidden {
    my ($name) = (@_);

    if (IsHidden($name)) {
        $| = 1;
        print "";
        die "Security violation; not allowed to access '$name'.";
    }
}


# Mark up text to highlight things for display as links in an
# HTML document.
sub MarkUpText {
     my ($text) = @_;
     my $bugsrpl = Param('bugsystemexpr');
     my $bugsmatch = Param('bugsmatch');
     my %substs = ();

     $substs{'bug_id'} = '$1';
     $bugsrpl = PerformSubsts($bugsrpl, \%substs);

     $text =~ s{((ftp|http)://\S*[^\s.])}{<a href=\"$1\">$1</a>}g;
     $text =~ s/(&lt;(.*@.*)&gt;)/<a href=\"mailto:$2\">$1<\/a>/g;

     $bugsmatch = 2
          unless ($bugsmatch =~ /^\+?\d+$/);
     $bugsmatch =~ s/\D*(\d+).*/$1/;
     eval ('$text =~ s((\d{' . $bugsmatch . ',}))(' . $bugsrpl . ')g;');

     return $text;
}

# Quotify a string, suitable for output as an html document
sub html_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    return $var;
}

sub GenerateProfileHTML {
     my ($name) = @_;
     my $ldapserver = Param('ldapserver');
     my $ldapport = Param('ldapport');
     my $result = '';
     my %value = ();
     my $i;

     my %fields = (
                   cn                 => "Name",
                   mail               => "E-mail",
                   telephonenumber    => "Phone",
                   pager              => "Pager",
                   nscpcurcontactinfo => "Contact Info"
                  );

     my $namelist = join(" ", keys(%fields));

     $result = "<TABLE><tr><Td align=right><b>Name:</b><td>$name</table>";
     return $result
          unless ($ldapserver);

     unless (open(LDAP, "./data/ldapsearch " .
                  "-b \"dc=netscape,dc=com\" " .
                  "-h $ldapserver -p $ldapport " .
                  "-s sub \"(mail=$name\@netscape.com)\" " .
                  "$namelist |")) {
          $result = "<B>Error -- Couldn't contact the directory server.</B>" .
               "<PRE>$!</PRE><br>\n" . $result;
          return $result;
     }

     while (<LDAP>) {
          if (/^([a-z]*): (.*)$/) {
               $value{$1} .= ' ' if $value{$1};
               $value{$1} .= $2
          }
     }
     close(LDAP);

     $result = "<TABLE>\n";
     foreach $i (keys(%fields)) {
          $result .= "  <TR><TD align=right><B>$fields{$i}:</B></TD>\n";
          $result .= "  <TD>$value{$i}</TD></TR>\n";
     }
     $result .= "</TABLE>\n";

     return $result;
}


sub GenerateUserLookUp {
     my ($uname, $account, $email) = @_;
     my $phonebookurl = Param('phonebookurl');
     my %substs = ();

     $account = $uname unless $account;
     $email = $account unless $email;

     %substs = (
                user_name    => html_quote($uname),
                email_name   => url_quote(EmailFromUsername($email)),
                account_name => url_quote($account)
               );
     $phonebookurl = PerformSubsts($phonebookurl, \%substs);
     
#     if ($phonebookurl =~ /^(.*href=\"[^:]*:)([^\"]*)(\".*)/) {
#          $phonebookurl = $1 . url_quote($2) . $3;
#     } 
     return $phonebookurl;
}

#
# Change this routine to 'fix' a bonsai path name and generate valid LXR path
# names.  Called with a bonsai pathname which should be converted.  Should
# return a valid url for LXR viewing of files.
#
sub Fix_LxrLink {
     my ($lxr_path) = @_;
     my $lxr_base = Param('lxr_base');
     my $munge = Param('lxr_mungeregexp');
     if ($munge ne "") {
         eval("\$lxr_path =~ $munge");
     }
     return "$lxr_base$lxr_path";
}

#
# Change this routine to 'fix' an LXR pathname and generate a valid
# bonsai path name.  Called with a full pathname which should
# reference a file in the CVS repository.
#
# Used to translate name that LXR generates and calls the bonsai
# scripts with.
#
sub Fix_BonsaiLink {
     my ($bonsai_path) = @_;

     return $bonsai_path if (-f $bonsai_path);

     $bonsai_path =~ s!/cscroot/csc/!/cscroot/!;
     $bonsai_path =~ s!/device/!/!;
     $bonsai_path =~ s!^csc/!!;

     return $bonsai_path;
}

# Quotify a string, suitable for invoking a shell process
sub shell_escape {
    my ($file) = @_;
    $file =~ s/([ \"\'\`\~\^\?\$\&\|\!<>\(\)\[\]\;\:])/\\$1/g;
    return $file;
}

1;
