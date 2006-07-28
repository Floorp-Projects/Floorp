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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Dawn Endico <endico@mozilla.org>
#                 Gregary Hendricks <ghendricks@novell.com>
#                 Vance Baarda <vrb@novell.com> 


# This script reads in xml bug data from standard input and inserts 
# a new bug into bugzilla. Everything before the beginning <?xml line
# is removed so you can pipe in email messages.

use strict;

#####################################################################
#
# This script is used import bugs from another installation of bugzilla.
# It can be used in two ways.
# First using the move function of bugzilla
# on another system will send mail to an alias provided by
# the administrator of the target installation (you). Set up an alias
# similar to the one given below so this mail will be automatically 
# run by this script and imported into your database.  Run 'newaliases'
# after adding this alias to your aliases file. Make sure your sendmail
# installation is configured to allow mail aliases to execute code. 
#
# bugzilla-import: "|/usr/bin/perl /opt/bugzilla/importxml.pl"
#
# Second it can be run from the command line with any xml file from 
# STDIN that conforms to the bugzilla DTD. In this case you can pass 
# an argument to set whether you want to send the
# mail that will be sent to the exporter and maintainer normally.
#
# importxml.pl [--sendmail] < bugsfile.xml
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

use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Xml;

use XML::Twig;
use Getopt::Long;
use Pod::Usage;

require "globals.pl";

my $debug = 0;
my $help = 0;

my $result = GetOptions("verbose|debug+" => \$debug,
                        "help|?"         => \$help);

pod2usage(0) if $help;
                        
use constant DEBUG_LEVEL => 2;
use constant ERR_LEVEL => 1;


GetVersionTable();
our $log;
our @attachments;
our $bugtotal;
our @recipients;
my $xml;

# This can go away as soon as data/versioncache is removed. Since we still
# have to use GetVersionTable() though, it stays for now.

sub Debug {
    return unless ($debug);
    my ($message, $level) = (@_);
    print STDERR "ERR: ". $message ."\n" if ($level == ERR_LEVEL);
    print STDERR "$message\n" if (($debug == $level) && ($level == DEBUG_LEVEL));
}

sub Error {
    my ($reason,$errtype) = @_;
    my $subject = "Bug import error: $reason";
    my $message = "Cannot import these bugs because $reason "; 
    $message .=   "\n\nPlease re-open the original bug.\n" if ($errtype);
    $message .=   "For more info, contact ". Param("maintainer") . ".\n";
    my @to = (Param("maintainer"));
    Debug($message, ERR_LEVEL);
    exit(1);
}

Debug("Reading xml", DEBUG_LEVEL);
# Read STDIN in slurp mode. VERY dangerous, but we live on the wild side ;-)
local($/);
$xml = <>;

Debug("Parsing tree", DEBUG_LEVEL);

my $testopiaXml = Bugzilla::Testopia::Xml->new();
$testopiaXml->parse($xml);

exit 0;

__END__

=head1 NAME

importxml - Import bugzilla bug data from xml.

=head1 SYNOPSIS

    importxml.pl [options] [file ...]

 Options:
       -? --help        Brief help message.
       -v --verbose     Print error and debug information. 
                        Multiple -v options increase verbosity.

=head1 OPTIONS

=over 8

=item B<-?>

    Print a brief help message and exits.

=item B<-v>

    Print error and debug information. Mulltiple -v increases verbosity

=item B<-m>

    Send mail to exporter with a log of bugs imported and any errors.

=back

=head1 DESCRIPTION

     This script is used import bugs from another installation of bugzilla.
     It can be used in two ways.
     First using the move function of bugzilla
     on another system will send mail to an alias provided by
     the administrator of the target installation (you). Set up an alias
     similar to the one given below so this mail will be automatically 
     run by this script and imported into your database.  Run 'newaliases'
     after adding this alias to your aliases file. Make sure your sendmail
     installation is configured to allow mail aliases to execute code. 

     bugzilla-import: "|/usr/bin/perl /opt/bugzilla/importxml.pl --mail"

     Second it can be run from the command line with any xml file from 
     STDIN that conforms to the bugzilla DTD. In this case you can pass 
     an argument to set whether you want to send the
     mail that will be sent to the exporter and maintainer normally.

     importxml.pl [options] < bugsfile.xml

=cut
