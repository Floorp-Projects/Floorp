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
# The Original Code is the Despot Account Administration System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Zach Lipton <zach@zachlipton.org>

use strict;
use diagnostics;


# a few configurables
#
my $despotOwnerMailTo = "mailto:endico\@mozilla.org";
my $despotOwner = "endico";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $F::state;
    $zz = $F::description;
    $zz = $F::id;
    $zz = $F::newpassword2;
    $zz = $F::files;
    $zz = $F::partition;
    $zz = $F::doclinks;
    $zz = $F::file;
    $zz = $F::newsgroups;
}


use CGI::Carp qw(fatalsToBrowser);

use CGI qw(:standard :html3);

require 'utils.pl';

$::POSTTYPE = "Post";

$| = 1;

$::despot = param("_despot");
if (!defined $::despot) {
    $::despot = 0;
}

# Whether or not the HTTP response headers and beginning of the HTML page has
# already been returned to the client.  Necessary because some user actions
# (f.e. AddUser) result in PrintHeader() getting called twice.
$::header_done = 0;

if (!param()) {
    PrintHeader();
    print h1("Despot -- access control for mozilla.org.");
    if (!exists $ENV{"HTTPS"} || $ENV{"HTTPS"} ne "ON") {
        my $fixedurl = "https://$ENV{'SERVER_NAME'}$ENV{'SCRIPT_NAME'}";
        print b("<font color=red>If possible, please use the " .
                a({href=>$fixedurl}, "secure version of this form") .
                ".</font>");
    }
    print h2("Login, please.");
    print img({-align=>"right",-width=>72,-height=>84,-src=>"handcuff.gif"});
    print p("To manage mozilla users, or to change your mozilla.org " .
            "password, you must first log in.");
    PrintLoginForm();
    exit;
}

import_names("F");              # Makes all form values available as F::.

# $F::debug = 1;

use Mysql;

$::db = Mysql->Connect("localhost", "mozusers", $F::loginname, "")
    || die "Can't connect to database server";


$::skiptrailer = 0;

if (defined $F::command) {
    my $cmd = $F::command;
    Authenticate() unless grep($cmd eq $_, qw(FindPartition));
    param("command", "");
    PrintHeader() if $cmd eq "MainMenu";
    eval "$cmd()";
    if ($@ ne "") {
        die "Error executing $cmd -- $@";
    }
} else {
    Authenticate();
    PrintHeader();
    MainMenu();
}


if (!$::skiptrailer) {
    print hr();
    print MyForm("MainMenu") . submit("Main menu") . end_form();
}


my $query = Query("select needed from syncneeded where needed=1");
my @row = $query->fetchrow();
if ($row[0]) {
    print hr();
    print p("Updating external machines...");
    print "<PRE>";
    if (!open(DOSYNC, "./syncit.pl -user $F::loginname|")) {
        print p("Can't do sync (error $?).  Please send mail to " .
                a({href=>$despotOwnerMailTo}, $despotOwner) . ".");
    } else {
        while (<DOSYNC>) {
            if ($::despot) {
                s/\&/\&amp;/g;
                s/</\&lt;/g;
                s/>/\&gt;/g;
                print $_;
            }
        }
        close(DOSYNC);
    }
    print "</PRE>";
    print p("... update completed.");
}



sub Authenticate {
    my $query = Query("select passwd,despot,neednewpassword,id,disabled from users where email = '$F::loginname'");
    my @row = $query->fetchrow();
    if (!@row || !checkpassword($F::loginpassword, $row[0])) {
        PrintHeader();
        print h1("Authentication Failed");
        print p("I can't figure out who you are.  Either your email address/password " . 
                "were entered incorrectly, or you didn't enter them.  In any case, I " . 
                "need to know who you are before I let you do what you wanted to do, " . 
                "so please enter your email address and password now.");
        if ($F::loginname && $F::loginname !~ /@/) {
            print p("Note! You must type in your full e-mail address, including the '\@'.");
        }
        PrintLoginForm();
        exit;
    }

    my $passwd;
    my $disabled;
    ($passwd,$::candespot,$::neednewpassword,$::loginid,$disabled) = @row;

    if ($disabled eq "Yes") {
        Punt("Your account has been disabled.");
    }

    if ($::candespot eq "Yes") {
        $::candespot = 1;
    } else {
        $::candespot = 0;
        $::despot = 0;
    }

    if ($::neednewpassword eq "Yes") {
        $::neednewpassword = 1;
    } else {
        $::neednewpassword = 0;
    }
    
}

sub PrintHeader {
    return if $::header_done;

    print header();

    my $bg = "white";
    my $extra = "";
    if ($::despot) {
        $bg = "lightgreen";
        $extra = " (Wonder-twin powers -- activate!)";
    }

    print start_html(-Title=>"Despot -- configure mozilla users$extra",
                     -BGCOLOR=>$bg);

    $::header_done = 1;
}


sub PrintLoginForm {
    print start_form(-method=>$::POSTTYPE);
    print table(Tr(th({-align=>"right"}, "Email address:"),
                   td(textfield(-name=>"loginname",
                                -size=>20))),
                Tr(th({-align=>"right"}, "Password:"),
                   td(password_field(-name=>"loginpassword",
                                     -size=>20))));
    print submit(-name=>"Log in");

    # Add all submitted values to the form (except email address and password,
    # which we want them to retype) as hidden fields so the user can immediately 
    # do whatever they wanted to do upon logging in.
    foreach my $field (param()) {
        next if grep($field eq $_, qw(loginname loginpassword));
        print hidden(-name=>$field, -default=>param($field));
    }

    print hr();
    print p("If you think you should be able to use this system, but you haven't been issued a login, please send mail to " .
            a({href=>$despotOwnerMailTo}, $despotOwner) . ".");
#     print p("If you do not yet have a mozilla.org account, or you have one " .
#             "but have forgotten your password, please fill in your e-mail " .
#             "address above, and click <nobr>here: " .
#             submit({name=>"mailMePassword"}, "Email me a password") . "</nobr>");

    print end_form();
}

sub MainMenu() {
    print h1("Despot -- main menu");
    print table({-width=>"100%"},
                Tr(td(h3("What would you like to do?")) .
                   td({-align=>"right"}, a({-href=>"help.html"},
                                           "Introduction to Despot"))));

    my @list = ();
    push(@list, li(MyForm("ViewAccount") .
                   submit("View info about your account") .end_form()));
    push(@list, li(MyForm("ChangePassword") .
                   submit("Change your password") . end_form()));
    push(@list, li(MyForm("ListPartitions") .
                   submit("List all the partitions") .end_form()));
#     my $query = Query("select partitionid,partitions.name,repositories.name,repositories.id from members,partitions,repositories where userid=$::loginid and class != 'Member' and partitions.id=members.partitionid and repositories.id=partitions.repositoryid");
#     my @row;
#     while (@row = $query->fetchrow()) {
#         my ($partid,$partname,$repname,$repid) = (@row);
#         my $title = "Edit partition '$partname' ($repname)";
#         param("partitionid", $partid);
#         push(@list,
#              li(MyForm("EditPartition") . submit($title) .
#                 hidden(-name=>"partitionid") . end_form()));
#     }

    my $query = Query("select id,name from repositories order by name");
    my @vals = ();
    my %labels;
    while (my @row = $query->fetchrow()) {
        my $v = $row[0];
        push(@vals, $v);
        $labels{$v} = $row[1];
    }
    my $radio = popup_menu(-name=>"repid",
                           "-values"=>\@vals,
                           -default=>$vals[0],
                           -labels=>\%labels);
    push(@list, li(MyForm("FindPartition") .
                   submit("Find partition containing file") .
                   "named<nobr> " .
                   textfield(-name=>"file", -size=>60) .
                   "</nobr> in <nobr>repository " . $radio .
                   end_form()));

    if ($::candespot) {
        param("_despot", !$::despot);
        my $str = $::despot ? "Disable" : "Enable";
        push(@list,
             li(MyForm("MainMenu") . submit("$str Despot Powers") .
                end_form()));
        param("_despot", $::despot);
        if ($::despot) {
            my @l2 = ();
            my $opts = popup_menu(-name=>"matchtype",
                                  "-values"=>["regexp", "not regexp"],
                                  -default=>"regexp",
                                  -labels=>{"regexp"=>"Matching regexp",
                                            "not regexp"=>"Not matching regexp"});
            push(@l2,
                 li("<nobr>" .
                    MyForm("ListUsers") . submit("List all users") .
                    $opts .
                    textfield(-name=>"match", -size=>50) .
                    "</nobr>" .
                    end_form()));
            push(@l2,
                 li(MyForm("AddUser") . submit("Add or edit user") .
                    "with email<nobr> " .
                    textfield(-name=>"email", -size=>25) . "</nobr>" .
                    end_form()));
            push(@l2,
                 li(MyForm("AddPartition") . submit("Create or edit partition")
                    . " named<nobr> " .
                    textfield(-name=>"partition", -size=>24) . "</nobr>" .
                    "</nobr> in <nobr>repository " . $radio .
                    end_form()));
            push(@list, "Despot commands:" . ul(\@l2));
        }
    }
        

    print ul(\@list);

    if ($::neednewpassword) {
        print "<font color=red>Your account has been frozen.  To make it usable, you must change your password.</font>";
    }

    $::skiptrailer = 1;
}


sub AddUser() {
    EnsureDespot();
    if ($F::email eq "") {
        Punt("You must enter an email address.");
    }
    my $email = $F::email;
    my $query;
    my @row;
    if ($email !~ /^[^@, ]*@[^@, ]*\.[^@, ]*$/) {
        Punt("Email addresses must contain exactly one '\@', and at least one '.' after the \@, and may not contain any commas or spaces.");
    }

    my $q = SqlQuote($email);
    $query = Query("select count(*) from users where email='$q'");
    @row = $query->fetchrow();

    if ($row[0] < 1) {
        my $p = "";
        my $realname = "";
        my $plain = pickrandompassword();
        $p = cryptit($plain);
        my $feedback = "'" . tt($plain) . "'";
        my $mailwords = "of '$plain'";
        $p = $::db->quote($p);
        $realname = $::db->quote($realname);
        Query("insert into users (email,passwd,neednewpassword,realname) values ('$q',$p,'Yes',$realname)");
        PrintHeader();
        print p("New account created.  Password initialized to $feedback; " .
                "please " .
                a({href=>"mailto:$email?subject=Change your mozilla.org password&body=Your new mozilla.org account has been created.  It initially has a%0apassword $mailwords.  Please go to http://despot.mozilla.org/despot.cgi%0aand change your password as soon as possible.  You won't actually be%0aable to use it for anything until you do."},
                  "send mail") .
                " and have the user change the password!");
        print hr();
    }
    EditUser();
}


sub EditUser() {
    EnsureDespot();
    PrintHeader();
    print h1("Edit a user");
    my $q = SqlQuote($F::email);
    my $query = Query("select * from users where email='$q'");
    my @row;
    @row = $query->fetchrow();
    my $query2 = Query("show columns from users");
    my @list;
    my @desc;
    for (my $i=0 ; $i<@row ; $i++) {
        @desc = $query2->fetchrow();
        my $line = th({-align=>"right"}, "$desc[0]:");
        if ($desc[0] ne ($query->name)[$i]) {
            die "show columns in different order than select???";
        }
        $_ = $desc[1];
        if (/^varchar/ || /int\([0-9]*\)$/) {
            if ($desc[0] eq "voucher") {
                $row[$i] = IdToEmail($row[$i]);
            }
            $line .= td(textfield(-name=>($query->name)[$i],
                                  -default=>$row[$i],
                                  -size=>50,
                                  -override=>1));
        } elsif (/^enum/) {
            s/^enum\(//;
            s/\)$//;
            my @values = split(/,/);
            my $opts = "";
            for (my $j=0 ; $j<@values ; $j++) {
                $values[$j] =~ s/^\'//;
                $values[$j] =~ s/\'$//;
            }
            $line .= td(radio_group(-name=>$desc[0], "-values"=>\@values,
                                    -default=>$row[$i]));
        }
        $line .= hidden("orig_$desc[0]", $row[$i]);

            

        push(@list,Tr($line));
    }
    print MyForm("ChangeUser");
    print table(@list);
    print submit("Save changes");
    print end_form();
    print MyForm("GeneratePassword") . hidden(-name=>"email");
    print submit("Generate a new random password for this user");
    print end_form();
# DeleteUser is currently a bad idea, as it leaves dangling pointers.  
# See bugzilla.mozilla.org bug 17589
#
#    print MyForm("DeleteUser") . hidden(-name=>"email");
#    print submit("Delete user");
#    print end_form();
}

# DeleteUser is currently a bad idea, as it leaves dangling pointers.  
# See bugzilla.mozilla.org bug 17589
# 
#sub DeleteUser() {
#    my $q = SqlQuote($F::email);
#    Query("delete from users where email = '$q'");
#    Query("insert into syncneeded (needed) values (1)");
#    PrintHeader();
#    print h1("OK, $F::email is gone.");
#    print hr();
#    MainMenu();
#}

sub ChangeUser() {
    foreach my $field ("email") {
        my $value = param($field);
        if ($value ne param("orig_$field")) {
            my $query = Query("select count(*) from users where $field = '" .
                              SqlQuote($value) . "'");
            my @row;
            @row = $query->fetchrow();
            if ($row[0] > 0) {
                Punt("Can't change $field to '$value'; already used.");
            }
        }
    }


    my $query = Query("show columns from users");
    my @list;
    my @row;
    while (@row = $query->fetchrow()) {
        my $old = param("orig_$row[0]");
        my $new = param($row[0]);
        if ($old ne $new) {
            if ($row[0] eq "voucher") {
                $old = EmailToId($old);
                $new = EmailToId($new);
            }
            Query("insert into changes (email,field,oldvalue,newvalue,who) values (" .
                  $::db->quote($F::orig_email) . ",'$row[0]'," .
                  $::db->quote($old) . "," .
                  $::db->quote($new) . ",'" .
                  $F::loginname . "')");
        }
        push(@list, "$row[0] = '" . SqlQuote($new) . "'");
    }
    my $qstr = "update users set " . join(",", @list) . " where email='" .
        SqlQuote($F::orig_email) . "'";
    Query($qstr);
    PrintHeader();
    print h1("OK, record for $F::email has been updated.");
    print hr();
    MainMenu();
    Query("insert into syncneeded (needed) values (1)");
}

sub GeneratePassword {
    my $email = $F::email;
    my $plain = pickrandompassword();
    my $p = cryptit($plain);
    Query("update users set passwd = " . $::db->quote($p) . ", neednewpassword='Yes' where email=" .
          $::db->quote($email));
    PrintHeader();
    print h1("OK, new password generated.");
    print "$email now has a new password of '" . tt($plain) . "'.  ";
    print "Please " .
        a({href=>"mailto:$email?subject=Change your mozilla.org password&body=Your mozilla.org account now has a password of '$plain'.  Please go to%0ahttp://despot.mozilla.org/despot.cgi and change your password as soon as%0apossible.  You won't actually be able to use your mozilla.org account%0afor anything until you do."},
          "send mail") .
              " to have the user change the password!";
    Query("insert into syncneeded (needed) values (1)");
}
    



sub ListPartitions () {
    PrintHeader();
    print p("If you're wondering what a 'partition' is, " .
            a({-href=>"help.html#partition"}, "read this") . ".");
    ListSomething("partitions", "id", "ListPartitions", "EditPartition", "name",
                  "name,repositoryid,state,description", "",
                  {"branchid"=>["branches.name",
                                "branches.id=partitions.branchid"],
                   "repositoryid"=>["repositories.name",
                                    "repositories.id=partitions.repositoryid"]
                                    },
                  "");
}


sub ListUsers() {
    EnsureDespot();

    PrintHeader();
    # ListSomething("users", "email", "ListUsers", "EditUser", "email", "email,realname,gila_group,cvs_group", "users as u2", {"voucher"=>"u2.email,u2.id=users.voucher"});
    my $wherepart = "";
    if ($F::match ne "") {
        $wherepart = "email $F::matchtype " . $::db->quote($F::match);
    }
    ListSomething("users", "email", "ListUsers", "EditUser", "email", "email,realname,gila_group,cvs_group", "users as u2", {"voucher"=>["if(users.voucher=0,'NONE',u2.email)", "u2.id=if(users.voucher=0,users.id,users.voucher)", "u2"]}, $wherepart);
}


sub ListSomething {
    my ($tablename,$idcolumn,$listprocname,$editprocname,$defaultsortorder,
        $defaultcolumns,$extratables,$columndefs,$extrawhere) = (@_);


    my %columnremap;
    my %columnwhere;
    my %columntable;

    while (my ($key,$value) = each %$columndefs) {
        ($columnremap{$key}, $columnwhere{$key}, $columntable{$key}) = (@$value);
        if (!defined $columntable{$key}) {
            $columntable{$key} = $columnremap{$key};
            $columntable{$key} =~ s/\..*$//;
        }
    }
        

    print h1("List of $tablename");
    my $sortorder = $defaultsortorder;
    if (defined $F::sortorder) {
        $sortorder = $F::sortorder;
    }

    my $query = Query("show columns from $tablename");
    my @allcols = ();
    my @cols = ();
    while (@row = $query->fetchrow()) {
        push(@allcols, $row[0]);
    }

    my $hiddencols = "";
    if (defined @F::showcolumns) {
        @cols = @F::showcolumns;
        foreach my $c (@cols) {
            $hiddencols .= hidden(-name=>"showcolumns",
                                 -default=>$c,
                                 -override=>1);
        }
    }

    if (0 == @cols) {
        @cols = split(/,/, $defaultcolumns);
    }

    my $rtext = th();
    my @list = ();
    my @emaillist = ();
    my $hiddenbits = "";
    if (defined $F::matchtype) {
        $hiddenbits .= hidden(-name=>"matchtype");
    }
    if (defined $F::match) {
        $hiddenbits .= hidden(-name=>"match");
    }

    foreach my $c (@cols) {
        if ($c eq $sortorder) {
            $rtext .= th({-valign=>"top"}, "sorted by<br>$c");
        } else {
            $rtext .= th({-valign=>"top"},
                         MyForm($listprocname) .
                         submit($c) .
                         $hiddenbits .
                         $hiddencols .
                         hidden(-name=>"sortorder",
                                -default=>$c,
                                -override=>1) .
                         end_form());
        }
        if (!defined $columnremap{$c}) {
            $columnremap{$c} = "$tablename.$c";
            $columntable{$c} = $tablename;
        }
    }

    push(@list, $rtext);
    my @mungedcols;
    my %usedtables;
    my $wherepart = "";
    if (defined $extrawhere && $extrawhere ne "") {
        $wherepart = " where " . $extrawhere;
    }
    foreach my $c (@cols) {
        my $t = $columnremap{$c};
        push(@mungedcols,$t);
        $usedtables{$columntable{$c}} = 1;
        if (defined $columnwhere{$c}) {
            if ($wherepart eq "") {
                $wherepart = " where ";
            } else {
                $wherepart .= " and ";
            }
            $wherepart .= $columnwhere{$c};
        }
    }
    if ($extratables ne "") {
        foreach my $i (split(/,/, $extratables)) {
            $usedtables{$i} = 1;
            if ($i =~ m/as (.*)$/) {
                if (exists $usedtables{$1}) {
                    delete $usedtables{$1};
                } else {
                    delete $usedtables{$i};
                }
            }
        }
    }
    my $orderby = $columnremap{$sortorder};
    if ($orderby =~ /\(/) {
        # The sort order is too complicated for stupid SQL.  Pick something
        # simpler.
        $orderby = "$tablename.$sortorder";
    }
    $query = Query("select $tablename.$idcolumn," . join(",", @mungedcols) . " from " . join(",", keys(%usedtables)) . $wherepart . " order by $orderby");
    while (@row = $query->fetchrow()) {
        my $i;
        for ($i=1 ; $i<@row ; $i++) {
            if (!defined $row[$i]) {
                $row[$i] = "";
            }
        }
        my $thisid = shift(@row);
        push(@emaillist, $thisid);
        push(@list, td(MyForm($editprocname) . hidden($idcolumn, $thisid) .
                       submit("Edit") . end_form()) .
             td(\@row));
    }
    print table(Tr(\@list));
    if ($idcolumn eq "email") {
        my $mailtolist = "mailto:" . join(',', @emaillist);
        print p(a({href=>$mailtolist},"Send mail to all users"));
    }
    print hr();
    print MyForm($listprocname);
    print submit("Redisplay with the following columns:") . br();
    print checkbox_group(-name=>"showcolumns",
                         "-values"=>\@allcols,
                         -default=>\@cols,
                         -linebreak=>'true');
    print hidden(-name=>"sortorder", -default=>$sortorder, -override=>1);
    print $hiddenbits;
    print end_form();
    

}



sub ViewAccount {
    my $query = Query("select * from users where email='$F::loginname'");
    my @row;
    @row = $query->fetchrow();
    my $query2 = Query("show columns from users");
    my @list;
    my @desc;
    my $userid;
    for (my $i=0 ; $i<@row ; $i++) {
        @desc = $query2->fetchrow();
        my $line = th({-align=>"right"}, "$desc[0]:");
        if ($desc[0] ne ($query->name)[$i]) {
            die "show columns in different order than select???";
        }
        if ($desc[0] eq "voucher") {
            $row[$i] = IdToEmail($row[$i]);
        }
        $line .= td($row[$i]);
        push(@list,Tr($line));
    }
    PrintHeader();
    print table(@list);
    $query = Query("select partitions.id,repositories.name,partitions.name,class from members,repositories,partitions where members.userid=$::loginid and partitions.id=members.partitionid and repositories.id=partitions.repositoryid order by class");
    while (@row = $query->fetchrow()) {
        my ($partid, $repname,$partname,$class) = (@row);
        param("partitionid", $partid);
        print MyForm("EditPartition") . hidden("partitionid") .
            submit("$class of $partname ($repname)") . end_form();
    }

    if ($::despot) {
        print MyForm("EditUser") . hidden("email", $F::loginname) .
            submit("Edit your account") .
                "(Since you're a despot, you can do this)" .                
                    end_form();
    }
    

}
    


sub FindPartition {
    my $repid = $F::repid;
    my $file = $F::file;
    my $query = Query("select files.pattern,partitions.name,partitions.id from files,partitions where partitions.id=files.partitionid and partitions.repositoryid=$repid");
    my @matches = ();
    while (@row = $query->fetchrow()) {
        my ($pattern,$name,$id) = (@row);
        if (FileMatches($file, $pattern) || FileMatches($pattern, $file)) {
            push(@matches, { 'name' => $name, 'id' => $id, 'pattern' => $pattern });
        }
    }

    if (scalar(@matches) > 0) {
        # The "view" parameter means the request came from the "look up owners of file"
        # form on the owners page, so we should redirect the user to the anchor on that
        # page for the first matching module.
        if (param("view")) {
            print "Location: http://www.mozilla.org/owners.html#$matches[0]->{name}\n\n";
            exit;
        }
        PrintHeader();
        print h1("Searching for partitions matching $file ...");
        foreach my $match (@matches) {
            print MyForm("EditPartition") . 
                  hidden("partitionid", $match->{id}) . 
                  submit($match->{name}) . 
                  "(matches pattern $match->{pattern})" .
                  end_form();
        }
    }
    else {
        PrintHeader();
        print h1("Searching for partitions matching $file ...");
        print "No partitions found.";
        if ($file !~ /^mozilla/) {
            print p("Generally, your filename should start with mozilla/ or mozilla-org/.");
        }
    }
}
                    




sub AddPartition {
    EnsureDespot();
    my $partition = $F::partition;
    if ($partition eq "") {
        Punt("You must enter a partition name.");
    }
    my $repid = $F::repid;
    my $query;
    my @row;

    $query = Query("select id from partitions where repositoryid=$repid and name = '$partition'");

    if (!(@row = $query->fetchrow())) {
        Query("insert into partitions (name,repositoryid,state,branchid) values ('$partition',$repid,'Open',1)");
        PrintHeader();
        print p("New partition created.") . hr();
        @row = Query("select LAST_INSERT_ID()")->fetchrow();
    }

    $F::partitionid = $row[0];
    EditPartition();
}

sub EditPartition() {
    my $partitionid;
    if (defined $F::partitionid) {
        $partitionid = $F::partitionid;
    } else {
        $partitionid = $F::id;
    }
    my $canchange = CanChangePartition($partitionid);
    my $query = Query("select partitions.name,partitions.description,state,repositories.name,repositories.id,branches.name,newsgroups,doclinks from partitions,repositories,branches where partitions.id = $partitionid and repositories.id = repositoryid and branches.id = branchid");
    @row = $query->fetchrow();

    my ($partname,$partdesc,$state,$repname,$repid,$branchname,$newsgroups,
        $doclinks) = (@row);
    PrintHeader();
    print h1(($canchange ? "Edit" : "View") . " partition -- $partname");
    if (!$canchange) {
        print p(b("You can't change anything here!") .
                " You can look, but you won't be permitted to touch.");
    }

    if ($partname eq "default") {
        print "<font color=red>This is the magic default partition.  It gets consulted on any file that does not match any other partition.</font>";
    }

    my @list;

    push(@list, Tr(th("Repository:") . td($repname)));

    push(@list, Tr(th("Description:") . td(textarea(-name=>"description",
                                                    -default=>$partdesc,
                                                    -rows=>4,
                                                    -columns=>50))));

    push(@list, Tr(th("Newsgroups:"),
                   td(textfield(-name=>'newsgroups',
                                -default=>$newsgroups,
                                -size=>30,
                                -override=>1))));

    push(@list, Tr(th("Doc links:"),
                   td(textarea(-name=>'doclinks',
                               -default=>$doclinks,
                               -rows=>10,
                               -columns=>50,
                               -override=>1))));
                                                  

    push(@list,
         Tr(th(a({-href=>"help.html#state"},"State:")) .
            td(radio_group(-name=>'state',
                           "-values"=>['Open', 'Restricted', 'Closed'],
                           -default=>$state,
                           -linebreak=>'false'))));

    push(@list,
         Tr(th(a({-href=>"help.html#branch"}, "Branch:")),
            td(textfield(-name=>'branchname',
                         -default=>$branchname,
                         -size=>30,
                         -override=>1))));

    $query = Query("select pattern from files where partitionid=$partitionid order by pattern");
    push(@list, CreateListRow("files", $query));
    foreach my $class ("Owner", "Peer", "Member") {
        $query = Query("select users.email,users.disabled from members,users where members.partitionid = $partitionid and members.class = '$class' and users.id = members.userid order by users.email");
        push(@list, CreateListRow($class, $query));
    }
                                                   
         
    param("partitionid", $partitionid);
    param("repid", $repid);
    param("repname", $repname);
    my $h = hidden(-name=>"partitionid") .
        hidden(-name=>"repid") .
            hidden(-name=>"repname");

    print MyForm("ChangePartition");
    print table(@list);
    if ($canchange) {
        print submit("Save changes");
        print $h;
        print end_form();
        print MyForm("DeletePartition");
        print submit("Delete partition");
        print $h;
    }
    print end_form();
}

sub ChangePartition {
    my $query;
    my @row;

    EnsureCanChangePartition($F::partitionid);

    Query("lock tables partitions write,branches write,files write,members write,users read");

    # Sanity checking first...

    my @files = split(/\n/, $F::files);

    my $q = SqlQuote($F::branchname);
    $query = Query("select files.pattern,partitions.name from " .
                   "files,partitions,branches where files.partitionid != " . 
                   "$F::partitionid and partitions.id=files.partitionid " .
                   "and partitions.repositoryid=$F::repid and branches.id=" .
                   "partitions.branchid and branches.name='$q'");
    while (@row = $query->fetchrow()) {
        my $f1 = $row[0];
        foreach my $f2 (@files) {
            $f2 = trim($f2);
            if ($f2 eq "") {
                next;
            }
            if (FileMatches($f1, $f2) || FileMatches($f2, $f1)) {
                Punt("File rule $f2 overlaps with existing rule in partition $row[1]");
            }
        }
    }

    my %idhash;
    foreach my $class ("Owner", "Peer", "Member") {
        my @names = split(/\n/, param($class));
        foreach my $n (@names) {
            $n =~ s/\(.*\)//g;
            $n = trim($n);
            if ($n eq "") {
                next;
            }
            $query = Query("select id,${F::repname}_group from users where email = " . $::db->quote($n));
            if (!(@row = $query->fetchrow())) {
                Punt("$n is not an email address in the database.");
            }
            my ($userid,$group) = (@row);
            if (exists $idhash{$n}) {
                Punt("$n seems to be mentioned more than once.  A given user can only have one class.");
            }
            $idhash{$n} = $userid;
            if ($group eq "None") {
                Punt("$n is not allowed to use the $F::repname repository at all, so it doesn't make sense to put this user into this partition.");
            }
        }
    }

                



    # And now actually update things.

    $query = Query("select id from branches where name = " .
                   $::db->quote($F::branchname));
    if (!(@row = $query->fetchrow())) {
        Query("insert into branches (name) values (" .
              $::db->quote($F::branchname) . ")");
        $query = Query("select LAST_INSERT_ID()");
        @row = $query->fetchrow();
    }
    my $branchid = $row[0];
    Query("update partitions set description=" .
          $::db->quote($F::description) .
          ", branchid=$branchid, state='$F::state', newsgroups=" .
          $::db->quote($F::newsgroups) .
          ", doclinks=" .
          $::db->quote($F::doclinks) .
          " where id=$F::partitionid");

    Query("delete from files where partitionid=$F::partitionid");
    foreach my $f2 (@files) {
        $f2 = trim($f2);
        if ($f2 eq "") {
            next;
        }
        Query("insert into files (partitionid,pattern) values ($F::partitionid," . $::db->quote($f2) . ")");
    }

    Query("delete from members where partitionid=$F::partitionid");
    foreach my $class ("Owner", "Peer", "Member") {
        my @names = split(/\n/, param($class));
        foreach my $n (@names) {
            $n =~ s/\(.*\)//g;
            $n = trim($n);
            if ($n eq "") {
                next;
            }
            Query("insert into members(userid,partitionid,class) values ($idhash{$n},$F::partitionid,'$class')");
        }
    }

    PrintHeader();
    print h1("OK, the partition has been updated.");
    print hr();
    Query("unlock tables");
    MainMenu();

    Query("insert into syncneeded (needed) values (1)");
}


sub DeletePartition() {
    EnsureCanChangePartition($F::partitionid);

    Query("delete from partitions where id = '$F::partitionid'");
    Query("delete from files where partitionid = '$F::partitionid'");
    Query("delete from members where partitionid = '$F::partitionid'");
    Query("insert into syncneeded (needed) values (1)");
    PrintHeader();
    print h1("OK, the partition is gone.");
    print hr();
    MainMenu();
}

sub FileMatches {
    my ($pattern, $name) = (@_);
    my $regexp = $pattern;
    if ($pattern =~ /\%$/) {
        $regexp =~ s:\%$:[^/]*:;
    } elsif ($pattern =~ /\*$/) {
        $regexp =~ s:\*$:.*:;
    }
    if ($name =~ /$pattern/) {
        return 1;
    }
    return 0;
}


sub CreateListRow {
    my ($title, $query) = (@_);
    my $result = "";
    my @row;
    while (my @row = $query->fetchrow()) {
        my $v = $row[0];
        if (defined $row[1] && $row[1] eq "Yes") {
            $v .= " (disabled)";
        }
        $result .= $v . "\n";
    }
    return Tr(th(a({-href=>"help.html#$title"}, "$title:")) .
              td(textarea(-name=>$title,
                          -default=>$result,
                          -rows=>10,
                          -columns=>50)));
}



sub ChangePassword {
    PrintHeader();
    print h1("Change your mozilla.org password.");
    $F::loginpassword = "";
    print start_form($::POSTTYPE);
    print hidden("loginname", $F::loginname);
    print hidden(-name=>"command",
                 -default=>"SetNewPassword",
                 -override=>1);
    print table(Tr(th({-align=>"right"}, "Old password:"),
                   td(password_field(-name=>"loginpassword", -size=>20,
                                     -override=>1))),
                Tr(th({-align=>"right"}, "New password:"),
                   td(password_field(-name=>"newpassword1", -size=>20))),
                Tr(th({-align=>"right"}, "Retype new password:"),
                   td(password_field(-name=>"newpassword2", -size=>20))));
    print submit("Change password");
    print end_form();
}


sub SetNewPassword {
    if ($F::newpassword1 ne $F::newpassword2) {
        Punt("New passwords didn't match.");
    }
    if ($F::newpassword1 eq $F::loginpassword) {
        Punt("Your new password isn't any different than your old one.");
    }
    my $z1;
    my $salt = "";
    my $pass = cryptit($F::newpassword1, $salt);

    my $qpass = $::db->quote($pass);
    Query("update users set passwd = $qpass, neednewpassword = 'No' where email='$F::loginname'");
    PrintHeader();
    print h1("Password has been updated.");
    Query("insert into syncneeded (needed) values (1)");
    if ($::despot) {
        param("loginpassword", $F::newpassword1);
        print hr();
        MainMenu();
    }
}





sub MyForm {
    my ($command) = @_;
    my $result = start_form($::POSTTYPE) . hidden("loginname", $F::loginname) .
        hidden("_despot") .
        hidden("loginpassword", $F::loginpassword) .
            hidden(-name=>"command", -value=>$command, -override=>1);
    return $result;
}

sub Punt {
    my ($header) = @_;
    PrintHeader();
    print h1($header);
    print p("Please hit " . b("back") . " and try again.");
    exit;
}


sub EnsureDespot {
    if (!$::despot) {
        Punt("You're not a despot.  You can't do this.");
    }
}


sub EnsureCanChangePartition {
    my ($id) = (@_);
    if (!CanChangePartition($id)) {
        Punt("You must be an Owner or Peer of the partition to do this.");
    }
}


sub CanChangePartition {
    my ($id) = (@_);
    if ($::despot) {
        return 1;
    }
    my $query = Query("select class from members where userid=$::loginid and partitionid=$id");
    my @row;
    if (@row = $query->fetchrow()) {
        if ($row[0] ne "Member") {
            return 1;
        }
    }
    return 0;
}


sub pickrandompassword {
    my $sc = "abcdefghijklmnopqrstuvwxyz";
    my $result = "";
    for (my $i=0 ; $i<8 ; $i++) {
        $result .= substr($sc, int (rand () * 100000) % (length ($sc) + 1), 1);
    }
    return $result;
}

sub IdToEmail {
    my ($id) = (@_);
    if ($id eq "0") {
        return "(none)";
    } else {
        my $query3 =
            Query("select email,disabled from users where id = $id");
        my @row3;
        if (@row3 = $query3->fetchrow()) {
            if ($row3[1] eq "Yes") {
                $row3[0] .= " (Disabled)";
            }
            return $row3[0];
        }
    }

    return "<font color=red>Unknown Id $id</font>";
}

sub EmailToId {
    my ($email, $forcevalid) = (@_);

    $email =~ s/\(.*\)//g;
    $email = trim($email);

    my $query = Query("select id from users where email = " .
                      $::db->quote($email));
    my @row;
    if (@row = $query->fetchrow()) {
        return $row[0];
    }
    if ($forcevalid || $email ne "") {
        Punt("$email is not a registered email address.");
    }
    return 0;
}
    
