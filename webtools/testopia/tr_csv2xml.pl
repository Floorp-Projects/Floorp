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
#                 David Koenig <dkoenig@novell.com>

use strict;

use Class::CSV;
use Getopt::Long;
use File::Temp "tempfile";
use Pod::Usage;

=head1 NAME

tr_csv2xml.pl - Convert CSV file to Testopia XML format.

=head1 SYNOPSIS

    tr_csv2xml.pl [ Options ] csvfilename xmlfilename

=head1 OPTIONS

=over 8

=item B<-h --help>

This usage statement.
    
=item B<-t --tcdb>

Preprocess CSV file to correct problems with TCDB CSV format.

=item B<-u --usage>

This usage statement.
    
=head1 DESCRIPTION

This script converts the file csvfilename in CSV format to the file
xmlfilename in Testopia XML format.

=cut


my $TCDB_CORRECTION_EXTENSION = ".TCDBcorrections";
my $TEST_PLAN_AUTHOR = "Change TEST_PLAN_AUTHOR in import.pl user\@novell.com";
use constant TEST_PLAN_DOCUMENT => "Change TEST_PLAN_DOCUMENT in import.pl";
my $TEST_PLAN_EDITOR = "Change TEST_PLAN_EDITOR in import.pl user\@novell.com";
use constant TEST_PLAN_NAME => "Change TEST_PLAN_NAME";
# Test Plan types are: Database_id, Database_description or Xml_description.
use constant TEST_PLAN_NAME_TYPE => "Xml_description";

my $debug = 0;
my $tcdb = 0;
my $usage = 0;

#
#Display error message to stderr and exit.
#
sub error {
    my ($reason,$errtype) = @_;
    print STDERR $reason . ".\n";
    exit(1);
}

# 
# Creat mapping from TCDB user ids to email addresses.
#
sub map_TCDB_users
{
	my ($tcdb_user) = @_;
	if ( -r "tcdbUsers" )
	{
		open(TCDBUSERS, "tcdbUsers") || error("Cannot open tcdbUsers");
		while (<TCDBUSERS>)
		{
			chop;
			my ($email_addr,$user_id) = split(/ /);
			$tcdb_user->{$user_id} = $email_addr;
		}
		close(TCDBUSERS);
	}
}

#
# The TCDB export does not quote doube quotes in the fields.  This method finds those
# double quotes and quotes them.
#
sub quote_the_doublequote {
	my ($line) = @_;

	# Seeing \" in some files as the quote for a double guote.
	$line =~ s/\\"/""/g;
	
	my @chars = split(//,$line);
	my $index = 0;
	my $in_quote_field = 0;
	my @return_line;

	while ( $index <= $#chars )
	{
		my $char = $chars[$index];

		if ( $char eq "\"" )
		{
			if ( $in_quote_field == 0 )
			{

				$in_quote_field = 1;
				push (@return_line,$char);
			}
			else
			{
				push (@return_line,$char);
				my $index2 = $index+1;
				while ( $index2<=$#chars )
				{
					last if ( $chars[$index2] =~ m/\S/ );
					$index2++;
				}
				if ( # Special condition for end of line.
				     # If the next character is last character of string and is a " we need to
				     # quote our current ".
				     ( $index2==$#chars && $chars[$index2] eq "\"" ) ||
				     # Special condition for eand of field.
				     # If the next two characters are ", we need to quote our current ".
				     ( ($index2+1)<=$#chars && $chars[$index2] eq "\"" && $chars[$index2+1] eq ",") ||
				     # If the next non white space character is not a , or " we need to quote the
				     # current ".
				     ( $index2<$#chars && $chars[$index2] ne ","  && $chars[$index2] ne "\"" ) )
				{

					push (@return_line,"\"") ;
				}
				else
				{
					$in_quote_field = 0;
				}
			}
		}
		else
		{
			push (@return_line,$char);
		}
		$index++;
	}
	return join("",@return_line);
}

#
# 
# Create the work file from the input file that will be passed to Class::CSV.  The input file's
# first line is field descriptions which are not passed into Class::CSV->parse.  
#
# The Test Case Data Base (TCDB) CSV files also need to be processed to clean up format errors.  
#
# The TCDB CSV errors are:
#	1) Does not escape " used in a field with "".
#	2) Runs the CSV across multiple lines.
#	3) In some cases a line may be missing the last field.
#
sub remove_field_list
{
	my ($input_filename,$work_filename,$tcdb_format) = @_;
	my $field_list = "";
	my $line = "";
	my $line_count = 0;
	my $matching_expression = "";
	my $matching_expression_tcdb_error = "";
	my $matching_expression_too_long = "";
	open(CSVINPUT, $input_filename) or error("Cannot open file $input_filename");
	open(CSVWORK, ">" . $work_filename) or error("Cannot open file $work_filename");

	while (<CSVINPUT>)
	{
		chop;
		my $current_line = $_;

		$current_line =~ s/\r//g;
		$current_line =~ s/\342\200\231/&#8217;/g;
		$current_line =~ s/\342\200\230/&#8216;/g;
		$current_line =~ s/\342\200\246/&#133;/g;
		$current_line =~ s/\342\200\223/-/g;
		$current_line =~ s/\342\200\224/&#8212;/g;
		$current_line =~ s/\342\200\234/&#8221;/g;
		$current_line =~ s/\342\200\235/&#8222;/g;
  
		$line_count += 1;
		if ( $line_count == 1 )
		{
			$matching_expression .= "^";
			$field_list = $current_line;
			my @fields = split(/,/);
			for ( my $i=1; $i<=$#fields; $i++ )
			{
				$matching_expression .= "(\".*\",|,)";
			}
			$matching_expression_too_long = $matching_expression . "(\".*\",|,).+\$";
			$matching_expression .= "(\".*\")?\$";	
			$matching_expression_tcdb_error = $matching_expression;
			$matching_expression_tcdb_error =~ s/^\^\("\.\*",\|,\)/^/;
			next;
		}
		
		if ( ! $tcdb_format )
		{
			print CSVWORK $current_line . "\n";
			next;
		}
		
		error("Found substitution key <TRANSLATE_DOUBLEQUOTE> in $input_filename at line $line_count") if ( /<TRANSLATE_DOUBLEQUOTE>/ );
		error("Found substitution key <TRANSLATE_NEWLINE> in $input_filename at line $line_count") if ( /<TRANSLATE_NEWLINE>/ );
		if ( $line ne "" )
		{	
			# If we have all the csv fields the line is ready to print.
			if ( $line =~ m/$matching_expression/ )
			{
				# The TCDB does not double quote double quotes which causes a problem when the last 
				# field contains a double quote at the end of the line that is should be part of the
				# field.  The match above is true but we have not really reached the end of the field.
				# So we need to break the line up at each field, if the last field contains a even
				# number of double quotes we have not reached the end of the line and need to just
				# append the current line onto our line buffer.
				my @fields = split /","/, $line;
				$fields[$#fields] =~ s/[^"]//g;
				if ( ((length $fields[$#fields]) % 2) == 0 )
				{
					$line .= $current_line;
					next;
				}
				print CSVWORK quote_the_doublequote($line) . "\n";
				$line = $current_line;
			}
			# If current line begins with a ", see if it's the start of a new line. Check to see if this line might have
			# TCDB error of a missing field.  If line has exceeded the matching criteria display a error.
			elsif ( $current_line =~ /^"/ )
			{
				if ( $line =~ m/$matching_expression_tcdb_error/ )
				{
					$line .= ",\"\"";
					print CSVWORK quote_the_doublequote($line) . "\n";
					$line = $current_line;
				}
				elsif ( $line =~ m/$matching_expression_too_long/ )
				{
					error("Confused in $input_filename at line $line_count.  Cannot figure out how to proceed");
				}
				else
				{
					$line .= "<TRANSLATE_NEWLINE>" . $current_line;
				}
			}
			else
			{
				$line .= "<TRANSLATE_NEWLINE>" . $current_line;
			}
		}
		else 
		{
			$line = $current_line;
		}
		print STDERR "        line=$line\ncurrent_line=$current_line\n" if ( $debug );
	}
	# Probably will still have a line in the $line buffer.
	if ( $line ne "" )
	{
		if ( $line =~ m/$matching_expression/ )
		{
			print CSVWORK quote_the_doublequote($line) . "\n";
		}
		else
		{
			$line .= ",\"\"" if ( $line =~ m/$matching_expression_tcdb_error/ );
			if ( $line =~ m/$matching_expression/ )
			{
				print CSVWORK quote_the_doublequote($line) . "\n";
			}
			else
			{
				error("Incomplete line in $input_filename at line $line_count.  Line follows" . $line);
			}
		}
	}
	close(CSVINPUT);
	close(CSVWORK);

	#
	# Sort the corrected CSV to remove duplicate records.
	#
	if ( $tcdb )
	{
		my @args = ( "sort -u -o " . $work_filename . " " . $work_filename );
		system(@args) == 0 or error("Could not sort $work_filename");
	}
	
	return $field_list;
}

#
# Remove the leading and trailing white space characters.  Remove commas at end of line.
#
sub remove_white_space {
	my ($line) = @_;
	$line =~ s/^\s+//g;
	$line =~ s/\s+$//g;
	$line =~ s/,$//g;

	return $line;
}

#
# Characters that are entities in XML and HTML need to be
# converted to their entity representation.
#
# TRANSLATE_NEWLINE is a character inserted by this script
# where a new line needs to be but not orginally contained
# in the field.
#
# Some new lines have been showing up as \\n in exports.
#
sub fix_entities {
	my ($line) = @_;
	$line =~ s/<TRANSLATE_NEWLINE>/\n/g;
	$line =~ s/\\n/\n/g;
	$line =~ s/\&/&amp;/g;
	$line =~ s/\</&lt;/g;
	$line =~ s/\>/&gt;/g;
	$line =~ s/\'/&apos;/g;
	$line =~ s/\"/&quot;/g;

	return $line;
}

GetOptions("debug" => \$debug, "tcdb" => \$tcdb, "help|usage|?" => \$usage);

pod2usage(0) if $usage;

error("Must supply a CSV file to convert") if ( $#ARGV == -1 );
error("Need to supply XML output file") if ( $#ARGV == 0 );
error("Too many arguments") if ( $#ARGV >= 2 );

my $csv_input_filename = $ARGV[0];

my $xml_output_filename = $ARGV[1];
my $csv_work_filename = $csv_input_filename . ".work";
open(XMLOUTPUT, "> $xml_output_filename") or error("Cannot open file $xml_output_filename");
my %tcdb_user;
my $field_list = remove_field_list($csv_input_filename,$csv_work_filename,$tcdb);
map_TCDB_users(\%tcdb_user);

#
# Process the $field_list variable which comes from the first line of the CSV file.
#
# Format of the first line should be in the form: 
#    "Testcase Name","Attributes","Priority","Description","Status","Folder","Creator",
#    "Owner","ResDetails","Build","InstanceID","Long Description","Pass/Fail Definition",
#    "Setup Steps","Cleanup Steps","Steps"
# 
# Fields currently used if they exist are: 
#    attributes - split apart at each comma to become a tag.
#    category - category for test case.
#    cleanupsteps - added to Break Down section.
#    component - component for test case.
#    description - summary unless testcasename is defined.  added to Action section.
#    environment - split apart at each comma to become a tag.
#    folder - only processed if -tcdb option is supplied.  split apart at each '/'.  based on each
#             teams input one field becomes the category and others tags.  each team defines which
#             sub folders they want to use.
#    longdescription - added to Action section.
#    owner (required) - in TCDB this is a ID that is mapped to a email address from the file
#                       tcdbUsers.
#    passfaildefinition - added to Expected Results section.
#    priority - becomes the priority.  I just a number P is prepended.
#    resdetails - added to Action section.
#    setupsteps - added to Set Up section.
#    steps - added to Action section.
#    testcasename - becomes the summary.  if testcasename is not supplied the description
#                   is the summary.  if testcasename and description are both null a error is
#                   generated.
#
# The order of the fields is not important.  The fields supplied to Class::CSV will be in
# order found on the first line of the CSV file.
#
# Steps needed are:
#    Change to lower case.
#    Remove spaces.
#    Remove all "'s
#    Remove all /'s.'
#
$field_list = lc $field_list;
$field_list =~ s/[\s"\/]//g;
# More sources for the CSV's other than TCDB, transform some of the column names.
$field_list =~ s/author/owner/g;
$field_list =~ s/result/passfaildefinition/g;
$field_list =~ s/summary/testcasename/g;
$field_list =~ s/tags/attributes/g;
my %fields;
foreach my $field ( split(/,/,$field_list) )
{
	$fields{$field} = "";
}

my $csv = Class::CSV->parse(
	filename => $csv_work_filename,
	fields   => [ split(/,/,$field_list) ]
);

print XMLOUTPUT "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n";
print XMLOUTPUT "<!DOCTYPE testopia [\n";
my $testopia_dtd = "testopia.dtd";
open(DTDINPUT,$testopia_dtd)  || die "Cannot open $testopia_dtd\n";
while (<DTDINPUT>)
{
	print XMLOUTPUT $_;
}
close(DTDINPUT);
print XMLOUTPUT "]>\n";
print XMLOUTPUT "<testopia version=\"2.21\">\n";

if ( $TEST_PLAN_AUTHOR ne "Change TEST_PLAN_AUTHOR in import.pl user\@novell.com" )
{
	print XMLOUTPUT "	<testplan author=\"" . $TEST_PLAN_AUTHOR . "\" type=\"System\" archived=\"False\">\n";
	print XMLOUTPUT "		<name>" . TEST_PLAN_NAME . "</name>\n";
	print XMLOUTPUT "		<product>TestProduct</product>\n";
	print XMLOUTPUT "		<productversion>other</productversion>\n";
	print XMLOUTPUT "		<editor>" . $TEST_PLAN_EDITOR . "</editor>\n";
	print XMLOUTPUT "		<document>" . TEST_PLAN_DOCUMENT . "</document>\n";
	print XMLOUTPUT "	</testplan>\n";
}

my $line_count = 0;
foreach my $line (@{$csv->lines()}) {
	$line_count += 1;
	print XMLOUTPUT "	<testcase ";
	
	error("No owner for Test Case at line $line_count in $csv_work_filename") if ( ! defined($fields{'owner'}) );
	my $owner = $line->owner();
	$owner = $tcdb_user{$line->owner()} if ( $owner =~ /\d+/ );
	error("Could not find owner for Test Case at line $line_count in $csv_work_filename") if ( $owner eq "" );
	
	print XMLOUTPUT "author=\"" . fix_entities($owner) . "\" ";
	if ( defined($fields{'priority'}) )
	{
		my $priority = fix_entities($line->priority());
		$priority =~ s/ .*//g;
		$priority = uc $priority;
		$priority = "P5" if ( $priority eq "" );
		$priority = "P" . $priority if ( ! ( $priority =~ m/^P.*/ ) );
		print XMLOUTPUT "priority=\"" . fix_entities($priority) . "\" ";
	}
	print XMLOUTPUT "automated=\"Manual\" ";
	print XMLOUTPUT "status=\"CONFIRMED\">\n";
	print XMLOUTPUT "         <testplan_reference type=\"" . TEST_PLAN_NAME_TYPE . "\">" . TEST_PLAN_NAME . "</testplan_reference>\n";
	my $summary;
	if ( defined($fields{'testcasename'}) )
	{
		$summary = fix_entities($line->testcasename());
	}
	elsif ( defined($fields{'description'}) )
	{
		$summary = fix_entities($line->description());
	}
	if ( defined($fields{'environment'}) )
	{
		my $environment = $line->environment();
		$environment = "" if ( $environment eq "\$EMPTYENV" );
		$summary .= " - " . fix_entities($environment) if ( $environment ne "" );
	}
	error("No summary for Test Case at line $line_count in $csv_work_filename") if ( $summary eq "" );
	print XMLOUTPUT "		<summary>" . $summary . "</summary>\n";
	print XMLOUTPUT "		<defaulttester>" . fix_entities($owner) . "</defaulttester>\n";
	if ( $tcdb && defined($fields{'folder'}) )
	{
		my @folder = split(/\\/,$line->folder()); 
		print XMLOUTPUT "		<tag>" . fix_entities($folder[4]) . "</tag>\n" if ( defined( $folder[4] ) );
		print XMLOUTPUT "		<categoryname>" . fix_entities($folder[5]) . "</categoryname>\n" if ( defined( $folder[5] ) );
		my $fieldstart = 6;
		while ( defined $folder[$fieldstart] )
		{
			print XMLOUTPUT "		<tag>" . fix_entities($folder[$fieldstart]) . "</tag>\n";
			$fieldstart += 1;
		}
	}
	if ( defined($fields{'attributes'}) )
	{
		my @attributes = split(/,/,$line->attributes());
		foreach my $attribute (@attributes)
		{
			print XMLOUTPUT "		<tag>" . fix_entities(remove_white_space($attribute)) . "</tag>\n";
		}
	}
	if ( defined($fields{'environment'}) )
	{
		my @environments = split(/,/,$line->environment());
		foreach my $environment (@environments)
		{
			print XMLOUTPUT "		<tag>" . fix_entities(remove_white_space($environment)) . "</tag>\n";
		}
	}
	if ( defined($fields{'component'}) && ( $line->component() ne "")  )
	{
		print XMLOUTPUT "		<component>" . fix_entities(remove_white_space($line->component())) . "</component>\n";
	}
	if ( defined($fields{'category'}) && ( $line->category() ne "") )
	{
		print XMLOUTPUT "		<categoryname>" . fix_entities(remove_white_space($line->category())) . "</categoryname>\n";
	}
	if ( defined($fields{'setupsteps'}) && ( $line->setupsteps() ne "") )
	{
		print XMLOUTPUT "		<setup>";
		print XMLOUTPUT "[TCDB Setup Steps]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->setupsteps());
		print XMLOUTPUT "</setup>\n";
	}
	if ( defined($fields{'cleanupsteps'}) && ( $line->cleanupsteps() ne "") )
	{
		print XMLOUTPUT "		<breakdown>";
		print XMLOUTPUT "[TCDB Cleanup Steps]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->cleanupsteps());
		print XMLOUTPUT "</breakdown>\n";
	}
	print XMLOUTPUT "		<action>";
	if ( defined($fields{'testcasename'}) && ( $line->testcasename() ne "") )
	{
		print XMLOUTPUT "[TCDB Test Case Name]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->testcasename());
	}
	if ( defined($fields{'description'}) && ( $line->description() ne "") )
	{
		print XMLOUTPUT "\n\n[TCDB Description]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->description());
	}
	if ( defined($fields{'longdescription'}) && ( $line->longdescription() ne "") )
	{
		print XMLOUTPUT "\n\n[TCDB Long Description]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->longdescription());
	}
	if ( defined($fields{'resdetails'}) && ( $line->resdetails() ne "") )
	{
		print XMLOUTPUT "\n\n[TCDB Resolution Details]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->resdetails());
	}
	if ( defined($fields{'steps'}) && ( $line->steps() ne "") )
	{
		print XMLOUTPUT "\n\n[TCDB Steps]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->steps());
	}
	print XMLOUTPUT "</action>\n";
	print XMLOUTPUT "		<expectedresults>";
	if ( defined($fields{'passfaildefinition'}) && ( $line->passfaildefinition() ne "") )
	{
		print XMLOUTPUT "[TCDB Pass Fail Definition]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->passfaildefinition());
	}
	print XMLOUTPUT "</expectedresults>\n";
	print XMLOUTPUT "	</testcase>\n";
  }
print XMLOUTPUT "</testopia>\n";

unlink $csv_work_filename;

exit 0;

__END__

