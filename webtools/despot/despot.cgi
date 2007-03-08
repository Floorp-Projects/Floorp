#!/usr/bin/perl -w
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
#                 Reed Loden <reed@reedloden.com>

use strict;
use diagnostics;

use lib qw(.);

# load our configuration file
#
use vars qw( $sitename $ownersurl $adminname $adminmail $db_host $db_name $db_user $db_pass );
do "config.pl" || die "Couldn't load config file";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $F::state;
    $zz = $F::description;
    $zz = $F::id;
    $zz = $F::newpassword2;
    $zz = $F::partition;
    $zz = $F::doclinks;
    $zz = $F::file;
    $zz = $F::newsgroups;
    $zz = $F::ownerspagedisplay;
}

my $emailregexp = q/^[^"'@|&, ]*@[^"'@|&, ]*\.[^"'@|&, ]*$/;
my $partregexp = q/^[-&:\.\w\d ]+$/;
my $partidregexp = q/^\d+$/;

use CGI::Carp qw(fatalsToBrowser);

use CGI qw(:standard :html3);

require 'utils.pl';

$::POSTTYPE = "POST";

$| = 1;

$::despot = param("_despot");
if (!defined $::despot) {
    $::despot = 0;
}

# Whether or not the HTTP response headers and beginning of the HTML page has
# already been returned to the client.  Necessary because some user actions
# (f.e. AddUser) result in PrintHeader() getting called twice.
$::header_done = 0;

$::disabled = "";
#$::disabled = "Despot is temporarily disabled.  Please try again later.";
if ($::disabled) {
    PrintHeader();
    print h1("Despot -- access control for $sitename.");
    print p($::disabled);
    exit;
}

# handle bad commandline based testing
my $server_name = defined $ENV{'SERVER_NAME'} ? $ENV{'SERVER_NAME'} : 'localhost';
my $script_name = defined $ENV{'SCRIPT_NAME'} ? $ENV{'SCRIPT_NAME'} : 'despot.cgi';
my $is_https = (exists $ENV{"HTTPS"} && (lc($ENV{"HTTPS"}) eq "on")) ? 1 : 0;
my $secure_url = "https://$server_name$script_name";
if (!param()) {
    PrintHeader();
    print h1("Despot -- access control for $sitename.");
    unless ($is_https) {
        print b("<font color=red>If possible, please use the " .
                a({href=>$secure_url}, "secure version of this form") .
                ".</font>");
    }
    print h2("Login, please.");
    print img({-align=>"right",-width=>72,-height=>84,-src=>"handcuff.gif"});
    print p("To manage $sitename users, or to change your $sitename " .
            "password, you must first log in.");
    PrintLoginForm();
    exit;
}

import_names("F");              # Makes all form values available as F::.

use DBI;

my $dsn = "DBI:mysql:host=$db_host;database=$db_name";
$::db = DBI->connect($dsn, $db_user, $db_pass)
    || die "Can't connect to database server";

my ($query, @row);

$::skiptrailer = 0;

sub Authenticate {
    my $query = $::db->prepare("SELECT passwd, despot, neednewpassword, id, disabled FROM users WHERE email = ?");
    $query->execute($F::loginname);
    my @row = $query->fetchrow_array();
    if (!@row || ($F::loginpassword && !checkpassword($F::loginpassword, $row[0]))) {
        PrintHeader();
        print h1("Authentication Failed");
        print p("I can't figure out who you are.  Either your email address/password " . 
                "were entered incorrectly, or you didn't enter them.  In any case, I " . 
                "need to know who you are before I let you do what you wanted to do, " . 
                "so please enter your email address and password now.");
        if ($F::loginname && $F::loginname !~ /@/) {
            print p("Note! You must type in your full e-mail address, including the '\@'.");
        }
        warn "DESPOT: Authentication failure for " . $F::loginname . "\n" if $F::loginname;
        PrintLoginForm();
        exit;
    }

    warn "DESPOT: Successfully authenticated " . $F::loginname . "\n";

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

    print start_html(-Title=>"Despot -- configure $sitename users$extra",
                     -BGCOLOR=>$bg);

    $::header_done = 1;
}


sub PrintLoginForm {
    # CGI seems to be misbehaving; creating the form tag manually as a workaround.
    print qq|<form method="$::POSTTYPE" enctype="application/x-www-form-urlencoded">\n|;
#    print start_form(-method=>$::POSTTYPE);
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
            a({href=>"mailto:$adminmail"}, $adminname) . ".");
#     print p("If you do not yet have a $sitename account, or you have one " .
#             "but have forgotten your password, please fill in your e-mail " .
#             "address above, and click <nobr>here: " .
#             submit({name=>"mailMePassword"}, "Email me a password") . "</nobr>");

    print end_form();
}

sub MainMenu {
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
#     my $query = $::db->prepare("SELECT partitionid, partitions.name, repositories.name, repositories.id " .
#                                "FROM members, partitions, repositories " .
#                                "WHERE userid = ? " .
#                                  "AND class != 'Member' " .
#                                  "AND partitions.id = members.partitionid " .
#                                  "AND repositories.id = partitions.repositoryid");
#     $query->execute($::loginid);
#     my @row;
#     while (@row = $query->fetchrow_array()) {
#         my ($partid,$partname,$repname,$repid) = (@row);
#         my $title = "Edit partition '$partname' ($repname)";
#         param("partitionid", $partid);
#         push(@list,
#              li(MyForm("EditPartition") . submit($title) .
#                 hidden(-name=>"partitionid") . end_form()));
#     }

    my $query = $::db->prepare("SELECT id, name FROM repositories ORDER BY name");
    $query->execute();
    my @vals = ();
    my %labels;
    while (my @row = $query->fetchrow_array()) {
        my $v = $row[0];
        push(@vals, $v);
        $labels{$v} = $row[1];
    }
    my $radio = popup_menu(-name=>"repid",
                           "-values"=>\@vals,
                           -default=>$vals[0],
                           -labels=>\%labels);
    push(@list, li(MyForm("FindPartition") .
                   submit("Find partition containing file with path") .
                   "<nobr> " .
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


sub AddUser {
    EnsureDespot();
    if ($F::email eq "") {
        Punt("You must enter an email address.");
    }
    my $email = $F::email;
    my $query;
    my @row;
    if ($email !~ /$emailregexp/) {
        Punt("Email addresses must contain exactly one '\@', and at least one '.' after the \@, and may not contain any pipes, ampersands, commas or spaces.");
    }

    $query = $::db->prepare("SELECT COUNT(*) FROM users WHERE email=?");
    $query->execute($email);
    @row = $query->fetchrow_array();

    if ($row[0] < 1) {
        my $p = "";
        my $realname = "";
        my $plain = pickrandompassword();
        $p = cryptit($plain);
        my $feedback = "'" . tt($plain) . "'";
        my $mailwords = "of '$plain'";
        my $sth = $::db->do("INSERT INTO users (email, passwd, neednewpassword, realname) VALUES (?,?,?,?)",
            undef, $email, $p, 'Yes', $realname);
        PrintHeader();
        print p("New account created.  Password initialized to $feedback; " .
                "please " .
                a({href=>"mailto:$email?subject=Change your " .
                   "$sitename password&body=Your new " .
                   "$sitename account has been created.  It " .
                   "initially has a%0apassword $mailwords.  Please go to " .
                   "$secure_url%0aand change your password as " .
                   "soon as possible.  You won't actually be%0aable to use " .
                   "it for anything until you do."},
                  "send mail") .
                " and have the user change the password!");
        print hr();
    }
    EditUser();
}


sub EditUser {
    EnsureDespot();
    PrintHeader();
    print h1("Edit a user");
    my $query = $::db->prepare("SELECT * FROM users WHERE email=?");
    $query->execute($F::email);
    my @row;
    @row = $query->fetchrow_array();
    my $query2 = $::db->prepare("SHOW COLUMNS FROM users");
    $query2->execute();
    my @list;
    my @desc;
    for (my $i=0 ; $i<@row ; $i++) {
        @desc = $query2->fetchrow_array();
        my $line = th({-align=>"right"}, "$desc[0]:");
        if ($desc[0] ne $query->{NAME}->[$i]) {
            die "show columns in different order than select???";
        }
        $_ = $desc[1];
        if (/^varchar/ || /int\([0-9]*\)$/) {
            if ($desc[0] eq "voucher") {
                $row[$i] = IdToEmail($row[$i]);
            }
            $line .= td(textfield(-name=>$query->{NAME}->[$i],
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
    print MyForm("UserHistory") . hidden(-name=>"email");
    print submit("View the changes history for this user");
    print end_form();
    print MyForm("GeneratePassword") . hidden(-name=>"email");
    print submit("Generate a new random password for this user");
    print end_form();
    print MyForm("DeleteUser") . hidden(-name=>"email");
    print submit("Delete user");
    print end_form();
}

sub DeleteUser {
    EnsureDespot();
    my $id = EmailToId($F::email, 1);
    $::db->do("DELETE FROM members WHERE userid = ?", undef, $id);
    $::db->do("DELETE FROM users WHERE id = ?", undef, $id);
    $::db->do("INSERT INTO changes (email, field, oldvalue, newvalue, who) VALUES (?,?,?,?,?)",
              undef, $F::email, 'deleted', 'No', 'Yes', $F::loginname);
    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
    PrintHeader();
    print h1("OK, $F::email is gone.");
    print hr();
    MainMenu();
}

sub ChangeUser {
    EnsureDespot();
    foreach my $field ("email", "voucher") {
        my $value = param($field);
        if ($value ne param("orig_$field")) {
            if ($field == "email") {
                my $query = $::db->prepare("SELECT COUNT(*) FROM users WHERE email = ?");
                $query->execute($value);
                my @row;
                @row = $query->fetchrow_array();
                if ($row[0] > 0) {
                   Punt("Can't change email to '$value'; already used.");
                }
            }
            elsif ($field == "voucher") {
                # EmailToId() will Punt() if the voucher is not valid.
                $value = EmailToId($value, 1);
            }
        }
    }

    my $query = $::db->prepare("SHOW COLUMNS FROM users");
    $query->execute();
    my @list;
    my @values;
    my @row;
    while (@row = $query->fetchrow_array()) {
        my $old = param("orig_$row[0]");
        my $new = param($row[0]);
        if ($old ne $new) {
            if ($row[0] eq "voucher") {
                $old = EmailToId($old);
                $new = EmailToId($new);
            }
            $::db->do("INSERT INTO changes (email, field, oldvalue, newvalue, who) VALUES (?,?,?,?,?)",
                undef, $F::orig_email, $row[0], $old, $new, $F::loginname);
            push(@list, "$row[0] = ?");
            push(@values, $new);
        }
    }
    if (@list) {
        my $qstr = "UPDATE users SET " . join(",", @list) . " WHERE email = ?";
        $::db->do($qstr, undef, @values, $F::orig_email);
    }
    PrintHeader();
    print h1("OK, record for $F::email has been updated.");
    print hr();
    MainMenu();
    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
}

sub GeneratePassword {
    EnsureDespot();
    my $email = $F::email;
    Punt("Email address is too scary for this web application") unless $email =~ /$emailregexp/;
    my $query = $::db->prepare("SELECT id FROM users WHERE email = ?");
    $query->execute($email);
    Punt("$email is not an email address in the database.") unless ($query->fetchrow_array());
    my $plain = pickrandompassword();
    my $p = cryptit($plain);
    my $query2 = $::db->prepare("SELECT neednewpassword FROM users WHERE email = ?");
    $query2->execute($email);
    my $old_neednewpassword = $query2->fetchrow_array();
    $::db->do("INSERT INTO changes (email, field, oldvalue, newvalue, who) VALUES (?,?,?,?,?)",
        undef, $email, 'neednewpassword', $old_neednewpassword, 'Yes', $F::loginname);
    $::db->do("UPDATE users SET passwd = ?, neednewpassword='Yes' WHERE email = ?",
        undef, $p, $email);
    PrintHeader();
    print h1("OK, new password generated.");
    print "$email now has a new password of '" . tt($plain) . "'.  ";
    print "Please " .
      a({href=>"mailto:$email?subject=Change your $sitename " .
         "password&body=Your $sitename account now has a password " .
         "of '$plain'.  Please go to%0a $secure_url and change " .
         "your password as soon as%0apossible.  You won't actually be able " .
         "to use your $sitename account%0afor anything until you ".
         "do."},
        "send mail") .
          " to have the user change the password!";
    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
}
    
sub UserHistory {
    EnsureDespot();
    PrintHeader();
    my $wherepart = "email = " . $::db->quote($F::email);
    ListSomething("changes", "changed_when", "UserHistory", "", "changed_when", "changed_when,field,oldvalue,newvalue,who", "", {}, $wherepart);
}


sub ListPartitions {
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


sub ListUsers {
    EnsureDespot();

    PrintHeader();
    # ListSomething("users", "email", "ListUsers", "EditUser", "email", "email,realname,gila_group,cvs_group", "users as u2", {"voucher"=>"u2.email,u2.id=users.voucher"});
    my $wherepart = "";
    if ($F::match ne "") {
        # very very narrow whitelist for matchtype.
        my $matchtype = $F::matchtype eq 'not regexp' ? 'not regexp' : 'regexp';
        $wherepart = "email $matchtype " . $::db->quote($F::match);
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
    my $query = $::db->prepare("SHOW COLUMNS FROM $tablename");
    $query->execute();
    my @allcols = ();
    my @cols = ();
    while (@row = $query->fetchrow_array()) {
        push(@allcols, $row[0]);
    }

    my $sortorder = $defaultsortorder;
    if (defined $F::sortorder) {
        $sortorder = $F::sortorder;
        my @sortorder = ();
        my @passedsortorder = split(",",$sortorder);
        foreach my $column (@passedsortorder) {
            my $dir = "";
            if ($column =~ m/(\S+)( ASC| DESC)$/i) {
                ($column, $dir) = ($1, $2);
            }
            if (!grep {$column eq $_} @allcols) {
                die "Invalid sort order passed";
            }
            push @sortorder, $column.$dir;
        }
        $sortorder = join(",",@sortorder);
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
        $wherepart = " WHERE " . $extrawhere;
    }
    foreach my $c (@cols) {
        my $t = $columnremap{$c};
        push(@mungedcols,$t);
        $usedtables{$columntable{$c}} = 1;
        if (defined $columnwhere{$c}) {
            if ($wherepart eq "") {
                $wherepart = " WHERE ";
            } else {
                $wherepart .= " AND ";
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
    # XXX: need to verify where the callers are getting the values they pass in here.
    # no clue if it's been sanitized or not, and we can't use placeholders because of
    # the way it's been passed in.
    $query = $::db->prepare("SELECT $tablename.$idcolumn," . join(",", @mungedcols) . " FROM " . join(",", keys(%usedtables)) . $wherepart . " ORDER BY $orderby");
    $query->execute();
    while (@row = $query->fetchrow_array()) {
        my $i;
        for ($i=1 ; $i<@row ; $i++) {
            if (!defined $row[$i]) {
                $row[$i] = "";
            }
        }
        my $thisid = shift(@row);
        push(@emaillist, $thisid);
        if ($editprocname) {
            push(@list, td(MyForm($editprocname) . hidden($idcolumn, $thisid) .
                           submit("Edit") . end_form()) .
                 td(\@row));
        } else {
           push(@list, td() . td(\@row));
        }
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
    my $query = $::db->prepare("SELECT * FROM users WHERE email = ?");
    $query->execute($F::loginname);
    my @row;
    @row = $query->fetchrow_array();
    my $query2 = $::db->prepare("SHOW COLUMNS FROM users");
    $query2->execute();
    my @list;
    my @desc;
    my $userid;
    for (my $i=0 ; $i<@row ; $i++) {
        @desc = $query2->fetchrow_array();
        my $line = th({-align=>"right"}, "$desc[0]:");
        if ($desc[0] ne $query->{NAME}->[$i]) {
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
    $query = $::db->prepare("SELECT partitions.id, repositories.name, partitions.name, class " .
                            "FROM members, repositories, partitions " .
                            "WHERE members.userid = ? " .
                              "AND partitions.id = members.partitionid " .
                              "AND repositories.id = partitions.repositoryid " .
                            "ORDER BY class DESC");
    $query->execute($::loginid);
    while (@row = $query->fetchrow_array()) {
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
    # Excludes the mozilla-toplevel module which would otherwise always match.
    # XXX: installation-specific stuff hardcoded here! (fix eventually)
    my $query = $::db->prepare("SELECT files.pattern, partitions.name, partitions.id " .
                               "FROM files, partitions " .
                               "WHERE partitions.id = files.partitionid " .
                                 "AND partitions.repositoryid = ? " .
                                 "AND files.pattern != 'mozilla/%'");
    $query->execute($repid);
    my @matches = ();
    while (@row = $query->fetchrow_array()) {
        my ($pattern,$name,$id) = (@row);
        if (FileMatches($file, $pattern) || FileMatches($pattern, $file)) {
            push(@matches, { 'name' => $name, 'id' => $id, 'pattern' => $pattern });
        }
        elsif ($file !~ /^mozilla(-org)?\//
               && (FileMatches("mozilla/$file", $pattern)
                   || FileMatches($pattern, "mozilla/$file")
                   || FileMatches("mozilla-org/$file", $pattern)
                   || FileMatches($pattern, "mozilla-org/$file")))
        {
            push(@matches, { 'name' => $name, 'id' => $id, 'pattern' => $pattern });
        }
    }

    if (scalar(@matches) > 0) {
        # The "view" parameter means the request came from the "look up owners of file"
        # form on the owners page, so we should redirect the user to the anchor on that
        # page for a single match.
        if (scalar(@matches) == 1 && param("view")) {
            print "Location: $ownersurl#" . name_to_id_token($matches[0]->{name}) . "\n\n";
            exit;
        }
        PrintHeader();
        print h1("Searching for partitions matching " . html_quote($file) . " ...");
        foreach my $match (@matches) {
            if (param("view")) {
                # This should display modules just as they are displayed
                # on owners.html, but that code is embedded into syncit.pl.
                print qq|Module: <a href="$ownersurl#| .
                                 name_to_id_token($match->{name}) . qq|">$match->{name}</a>
                                 (matches pattern $match->{pattern})<br>\n|;
            }
            else {
                print MyForm("EditPartition") . 
                      hidden("partitionid", $match->{id}) . 
                      submit($match->{name}) . 
                      "(matches pattern $match->{pattern})" .
                      end_form();
            }
        }
    }
    else {
        PrintHeader();
        print h1("Searching for partitions matching " . html_quote($file) . " ...");
        print "No partitions found.";
    }

    if (param("view")) {
        # No need for the footer with the "Main Menu" button
        # for "look up owners of file" users.
        print "</body></html>\n";
        exit;
    }
}
                    




sub AddPartition {
    EnsureDespot();
    my $partition = $F::partition;
    if ($partition eq "") {
        Punt("You must enter a partition name.");
    }
    Punt("Partition names may only contain letters, numbers, spaces, dashes, ampersands, and colons") unless $partition =~ /$partregexp/;
    my $repid = $F::repid;
    my $query;
    my @row;

    $query = $::db->prepare("SELECT id FROM partitions WHERE repositoryid = ? AND name = ?");
    $query->execute($repid, $partition);

    if (!(@row = $query->fetchrow_array())) {
        $::db->do("INSERT INTO partitions (name, repositoryid, state, branchid) VALUES (?,?,?,?)",
             undef, $partition, $repid, 'Open', 1);
        PrintHeader();
        print p("New partition created.") . hr();
        my $query2 = $::db->prepare("SELECT LAST_INSERT_ID()");
        $query2->execute();
        @row = $query2->fetchrow_array();
    }

    $F::partitionid = $row[0];
    EditPartition();
}

sub EditPartition {
    my $partitionid;
    if (defined $F::partitionid) {
        $partitionid = $F::partitionid;
    } else {
        $partitionid = $F::id;
    }
    my $canchange = CanChangePartition($partitionid);
    my $query = $::db->prepare("SELECT partitions.name, partitions.description, state, " .
                                      "repositories.name, repositories.id, branches.name, " .
                                      "newsgroups, doclinks, ownerspagedisplay " .
                               "FROM partitions, repositories, branches " .
                               "WHERE partitions.id = ? " .
                                 "AND repositories.id = repositoryid " .
                                 "AND branches.id = branchid");
    $query->execute($partitionid);

    my ($partname,$partdesc,$state,$repname,$repid,$branchname,$newsgroups,
        $doclinks,$ownerspagedisplay) = $query->fetchrow_array();
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

    push(@list, Tr(th("Name:"),
                   td(textfield(-name=>'partition',
                                -default=>$partname,
                                -size=>30,
                                -override=>1))));

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

    push(@list, Tr(th("Display on website:"),
                   td(checkbox(-name=>'ownerspagedisplay',
                               -checked=>($ownerspagedisplay eq "Yes") ? 1 : 0,
                               -value=>1,
                               -label=>'Display this partition on '.$ownersurl))));

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

    $query = $::db->prepare("SELECT pattern FROM files WHERE partitionid = " .
        $::db->quote($partitionid) . " ORDER BY pattern");
    push(@list, CreateListRow("Files", $query));

    foreach my $class ("Owner", "Peer", "Member") {
        $query = $::db->prepare("SELECT users.email, users.disabled " .
                                "FROM members, users " .
                                "WHERE members.partitionid = " . $::db->quote($partitionid) .
                                 " AND members.class = " . $::db->quote($class) .
                                 " AND users.id = members.userid " .
                                "ORDER BY users.email");
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
    my $partitionid = $F::partitionid;

    $::db->do("LOCK TABLES partitions WRITE, branches WRITE, files WRITE, members WRITE, users READ");

    # Sanity checking first...

    my @files = split(/\n/, param("Files"));

    $query = $::db->prepare("SELECT files.pattern, partitions.name " .
                            "FROM files, partitions, branches " .
                            "WHERE files.partitionid != ? " .
                              "AND partitions.id = files.partitionid " .
                              "AND partitions.repositoryid = ? " .
                              "AND branches.id = partitions.branchid " .
                              "AND branches.name = ?");
    $query->execute($F::partitionid, $F::repid, $F::branchname);
    while (@row = $query->fetchrow_array()) {
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
            # XXX: need to sanitize $F::repname
            $query = $::db->prepare("SELECT id, ${F::repname}_group FROM users WHERE email = ?");
            $query->execute($n);
            if (!(@row = $query->fetchrow_array())) {
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

    if ($F::partition eq "") {
        Punt("You must enter a partition name.");
    }
    Punt("Partition names may only contain letters, numbers, spaces, dashes, ampersands, and colons") unless $F::partition =~ /$partregexp/;



    # And now actually update things.

    $query = $::db->prepare("SELECT id FROM branches WHERE name = ?");
    $query->execute($F::branchname);
    if (!(@row = $query->fetchrow_array())) {
        $::db->do("INSERT INTO branches (name) VALUES (?)",
             undef, $F::branchname);
        $query = $::db->prepare("SELECT LAST_INSERT_ID()");
        $query->execute();
        @row = $query->fetchrow_array();
    }
    my $branchid = $row[0];
    $::db->do("UPDATE partitions SET name = ?, " .
                                    "description = ?, " .
                                    "branchid = ?, " .
                                    "state = ?, " .
                                    "newsgroups = ?, " .
                                    "doclinks = ?, " .
                                    "ownerspagedisplay = ? " .
              "WHERE id = ?",
        undef, $F::partition, $F::description, $branchid, $F::state, $F::newsgroups, $F::doclinks, defined($F::ownerspagedisplay) ? 'Yes' : 'No', $F::partitionid);

    $::db->do("DELETE FROM files WHERE partitionid = ?", undef, $F::partitionid);
    foreach my $f2 (@files) {
        $f2 = trim($f2);
        if ($f2 eq "") {
            next;
        }
        $::db->do("INSERT INTO files (partitionid, pattern) VALUES (?, ?)",
            undef, $F::partitionid, $f2);
    }

    $::db->do("DELETE FROM members WHERE partitionid = ?", undef, $F::partitionid);
    foreach my $class ("Owner", "Peer", "Member") {
        my @names = split(/\n/, param($class));
        foreach my $n (@names) {
            $n =~ s/\(.*\)//g;
            $n = trim($n);
            if ($n eq "") {
                next;
            }
            $::db->do("INSERT INTO members (userid, partitionid, class) VALUES (?, ?, ?)",
                undef, $idhash{$n}, $F::partitionid, $class);
        }
    }

    PrintHeader();
    print h1("OK, the partition has been updated.");
    print hr();
    $::db->do("UNLOCK TABLES");
    MainMenu();

    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
}


sub DeletePartition {
    EnsureCanChangePartition($F::partitionid);
    my $partitionid = $F::partitionid;

    $::db->do("DELETE FROM partitions WHERE id = ?", undef, $F::partitionid);
    $::db->do("DELETE FROM files WHERE partitionid = ?", undef, $F::partitionid);
    $::db->do("DELETE FROM members WHERE partitionid = ?", undef, $F::partitionid);
    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
    PrintHeader();
    print h1("OK, the partition is gone.");
    print hr();
    MainMenu();
}

sub FileMatches {
    my ($pattern, $name) = (@_);
    my $regexp = $pattern;
    if ($pattern =~ /\%$/) {
        $regexp =~ s:\%$:[^/]*\$:;
    } elsif ($pattern =~ /\*$/) {
        $regexp =~ s:\*$:.*:;
    }
    if ($name =~ /$regexp/) {
        return 1;
    }
    return 0;
}


sub CreateListRow {
    my ($title, $query) = (@_);
    my $result = "";
    my @row;
    $query->execute();
    while (my @row = $query->fetchrow_array()) {
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
    print h1("Change your $sitename password.");
    $F::loginpassword = "";
    # CGI seems to be misbehaving; creating the form tag manually as a workaround.
    print qq|<form method="$::POSTTYPE" enctype="application/x-www-form-urlencoded">\n|;
#    print start_form(-method=>$::POSTTYPE);
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

    $::db->do("UPDATE users SET passwd = ?, neednewpassword = 'No' WHERE email = ?",
        undef, $pass, $F::loginname);
    PrintHeader();
    print h1("Password has been updated.");
    $::db->do("INSERT INTO syncneeded (needed) VALUES (1)");
    if ($::despot) {
        param("loginpassword", $F::newpassword1);
        print hr();
        MainMenu();
    }
}





sub MyForm {
    my ($command) = @_;
    # CGI seems to be misbehaving; creating the form tag manually as a workaround.
    my $result = qq|<form method="$::POSTTYPE" enctype="application/x-www-form-urlencoded">| . 
        hidden("loginname", $F::loginname) .
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
    Punt("Invalid partitionid.") unless $id =~ /$partidregexp/;
    if (!CanChangePartition($id)) {
        Punt("You must be an Owner or Peer of the partition to do this.");
    }
}


sub CanChangePartition {
    my ($id) = (@_);
    if ($::despot) {
        return 1;
    }
    my $query = $::db->prepare("SELECT class FROM members WHERE userid = ? AND partitionid = ?");
    $query->execute($::loginid, $id);
    my @row;
    if (@row = $query->fetchrow_array()) {
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
            $::db->prepare("SELECT email, disabled FROM users WHERE id = ?");
        $query3->execute($id);
        my @row3;
        if (@row3 = $query3->fetchrow_array()) {
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

    my $query = $::db->prepare("SELECT id FROM users WHERE email = ?");
    $query->execute($email);
    my @row;
    if (@row = $query->fetchrow_array()) {
        return $row[0];
    }
    if ($forcevalid || $email ne "") {
        Punt("$email is not an email address in the database.");
    }
    return 0;
}

{
    my $EnableDeleteUser = 1;
    for ( defined $F::command ? $F::command : 'MainMenu')
    {
        # Only allow anonymous users access to the view parameter
        # of FindPartition() (bug 346660)
        (/^FindPartition$/ && param("view")) || do { Authenticate(); };
        param("command", "");
        /^AddPartition$/ && do { AddPartition(); last; };
        /^AddUser$/ && do { AddUser(); last; };
        /^ChangePartition$/ && do { ChangePartition(); last; };
        /^ChangePassword$/ && do { ChangePassword(); last; };
        /^ChangeUser$/ && do { ChangeUser(); last; };
        /^DeletePartition$/ && do { DeletePartition(); last; };
        /^DeleteUser$/ && $EnableDeleteUser && do { DeleteUser(); last; };
        /^EditPartition$/ && do { EditPartition(); last; };
        /^EditUser$/ && do { EditUser(); last; };
        /^FindPartition$/ && do { FindPartition(); last; };
        /^GeneratePassword$/ && do { GeneratePassword(); last; };
        /^ListPartitions$/ && do { ListPartitions(); last; };
        /^ListUsers$/ && do { ListUsers(); last; };
        /^SetNewPassword$/ && do { SetNewPassword(); last; };
        /^UserHistory$/ && do { UserHistory(); last; };
        /^ViewAccount$/ && do { ViewAccount(); last; };
        /^MainMenu$|/ && do { PrintHeader(), MainMenu(); last; };
    }
}


if (!$::skiptrailer) {
    print hr();
    print MyForm("MainMenu") . submit("Main menu") . end_form();
}

$query = $::db->prepare("SELECT needed FROM syncneeded WHERE needed = 1");
$query->execute();
@row = $query->fetchrow_array();
if ($row[0]) {
    print hr();
    print p("Updating external machines...");
    print "<PRE>";
    if (!open(DOSYNC, "./syncit.pl -user $F::loginname|")) {
        print p("Can't do sync (error $?).  Please send mail to " .
                a({href=>"mailto:$adminmail"}, $adminname) . ".");
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
