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

sub Debug {
    return unless ($debug);
    my ($message, $level) = (@_);
    print STDERR "ERR: ". $message ."\n" if ($level == ERR_LEVEL);
    print STDERR "$message\n" if (($debug == $level) && ($level == DEBUG_LEVEL));
}

Debug("Reading xml", DEBUG_LEVEL);

my $xml;
my $filename;
if ( $#ARGV == -1 )
{
	# Read STDIN in slurp mode. VERY dangerous, but we live on the wild side ;-)
	local($/);
	$xml = <>;
}
elsif ( $#ARGV == 0 )
{
	$filename = $ARGV[0];
}
else
{
	pod2usage(0);
}

Debug("Parsing tree", DEBUG_LEVEL);

my $testopiaXml = Bugzilla::Testopia::Xml->new();
$testopiaXml->parse($xml,$filename);

exit 0;

__END__

=head1 NAME

tr_importxml - Import Testopia data from xml.

=head1 SYNOPSIS

    tr_importxml.pl [options] [file]

 Options:
       -? --help        Brief help message.
       -v --verbose     Print error and debug information. 
                        Multiple -v options increase verbosity.
                        
 With no file read standard input.

=head1 OPTIONS

=over 8

=item B<-?>

    Print a brief help message and exits.

=item B<-v>

    Print error and debug information. Mulltiple -v increases verbosity

=back

=head1 DESCRIPTION

     This script is used import Test Plans and Test Cases into Testopia.
     

=cut
