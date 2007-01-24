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

my $TEST_PLAN_AUTHOR = "Change TEST_PLAN_AUTHOR in import.pl user\@novell.com";
use constant TEST_PLAN_DOCUMENT => "Change TEST_PLAN_DOCUMENT in import.pl";
use constant TEST_PLAN_NAME => "Change TEST_PLAN_NAME";
use constant TEST_PLAN_PRODUCT => "TestProduct";
use constant TEST_PLAN_PRODUCT_VERSION => "other";
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
# Peek ahead one character in the file.
#
sub peek(*)
{
	my $fh = shift;
	
	# Get current position in file.
	my $position = tell $fh;
	# Read next character.
	read(CSVINPUT, my $next_char, 1);
	# Restore position in file.
	seek(CSVINPUT,$position,0);
	
	return $next_char;
}

#
# Print the fields.  
#
# In each field need to change '\"' to '"' and change all '"' to '""'.  The last field
# will also have a '"' at the end of the line that needs to be removed.
#
# If TCDB format the Test Case Name may contain a series of 4-6 digits after two underscores
# which need to be removed.  The TCDB appended the Test Case number to any Test Case created
# from a existing Test Case.  In Testopia the Test Case numbers have no meaning.
#
sub print_fields
{
	my ($file_descriptor,$fields_ref,$testcasenamefield,$tcdb_format) = @_;

	my $index = 0;
	while ( $index < @$fields_ref )
	{
		$fields_ref->[$index] =~ s/"$//g if ( $index == ( @$fields_ref -1 ) );
		$fields_ref->[$index] =~ s/\\"/"/g;
		$fields_ref->[$index] =~ s/"/""/g;
		if ( $tcdb_format && ( $index == $testcasenamefield ) )
		{
			$fields_ref->[$index] =~ s/__\d\d\d\d\d\d\d//g;
			$fields_ref->[$index] =~ s/__\d\d\d\d\d\d//g;
			$fields_ref->[$index] =~ s/__\d\d\d\d\d//g;
		}
		print $file_descriptor "\"$fields_ref->[$index]\"";
		print $file_descriptor "," if ( $index != ( @$fields_ref -1 ) );
		$index += 1;
	}
	print $file_descriptor "\n";
}

#
# 
# Create the work file from the input file that will be passed to Class::CSV.  The input file's
# first line is field descriptions which are not passed into Class::CSV->parse.  
#
# The Test Case Data Base (TCDB) CSV files also need to be processed to clean up format errors.  
#
# The TCDB CSV errors are:
#	1) Does not escape " used in a field with "".  May use \" instead of "" but not always.
#	2) Runs the CSV across multiple lines.
#	3) In some cases a line may be missing the last field.
#
sub remove_field_list
{
	my ($input_filename,$work_filename,$tcdb_format) = @_;
	my $field_list = "";
	my @field_buffer;
	my @fields;
	my $fields_index = 0;
	my $in_quote_field = 0;
	my $line = "";
	my $line_count = 0;
	my $number_of_fields = 0;
	my $parse_line = "";
	my $testcasenamefield = "";

	open(CSVINPUT, $input_filename) or error("Cannot open file $input_filename");
	open(CSVWORK, ">", $work_filename) or error("Cannot open file $work_filename");

	while (<CSVINPUT>)
	{
		chop;

		s/\r//g;
		#
		# Map extended characters into HTML entities.
		#
		s/\342\200\223/-/g;
		s/\342\200\224/&#8212;/g;
		s/\342\200\230/&#8216;/g;
		s/\342\200\231/&#8217;/g;
		s/\342\200\234/&#8220;/g;
		s/\342\200\235/&#8221;/g;
		s/\342\200\246/&#133;/g;
		s/\302\240/&nbsp;/g;
		s/\302\251/&copy;/g;
		s/\031/'/g;
		s/\221/&8216;/g;					# left single quotation mark
		s/\222/&8217;/g;					# right single quotation mark
		s/\223/&8220;/g;					# left double quotation mark
		s/\224/&8221;/g;					# right double quotation mark
		s/\226/-/g;
		s/\337/&#223;/g;					# beta
		s/\341/&#224;/g;					# small letter a with acute accent
		s/\342/&#225;/g;					# small letter a with grave accent
		s/\344/&#228;/g;					# small letter a with tilde
		s/\346/&#229;/g;					# small letter a with umlaut
		s/\347/&#230;/g;					# small ae 
		s/\350/&#231;/g;					# small letter c cedilla
		s/\351/&#232;/g;					# small letter e with acute accent
		s/\364/&#244;/g;					# small letter o with circumflex
		
		$line_count += 1;
		if ( $line_count == 1 )
		{
			$field_list = $_;
			$number_of_fields = $field_list;
			$number_of_fields =~ s/[^,]//g;
			# Counting the number of commas so number of fields is one more.
			$number_of_fields = (length $number_of_fields) + 1;
			# Returned $field_list needs to be lower case, all spaces, " and \ removed.
			$field_list = lc $field_list;
			$field_list =~ s/[\s"\/]//g;
			my $index = 0;
			# Find the field that contains the Test Case name.
			foreach my $field ( split(/,/,$field_list) )
			{
				if ( $field eq "testcasename" )
				{
					$testcasenamefield = $index ;
					last;
				}
				$index++;
			}
			next;
		}
		
		if ( ! $tcdb_format )
		{
			print CSVWORK $_ . "\n";
			next;
		}
		
		# Missing or empty environment in TCDB has this value in the environment field.  Set it to
		# null and hope it was not in the middle of a field.  
		s/"\$EMPTYENV"/""/;
		
		# TCDB CSV options that are not handled correctly:
		#   If a field contains some thing like:
		#	   '2. Click "Roles and Tasks " , "Storage" , click "Volumes" and''
		#   the "," will be seen as field seperator and not as part of the field.

		# Add the current line onto the line to parse.
		$parse_line .= $_;

		# The end of the TCDB CSV line will be a double quote at the end of the line.  Keep combining
		# lines until we have a double quote at the end of the line and try to parse the line.
		if ( ! ($parse_line =~ /.+\n*"$/) )
		{
			$parse_line .= "\\n";
			next;
		}

		# At this point $parse_line will hopefully contain the full CSV line.  It may not though since
		# we could have found a double quote at the end of the line used in field that spans multiple
		# lines.  Parse it  below and if we have not found all the fields we will loop back and read
		# more lines until the next double quote at the end of the line is found.

		$in_quote_field = 0;
		my $index = 0;
		@fields = ();
		@field_buffer = ();
		my @chars = split(//,$parse_line);
		while ( $index <= $#chars )
		{
			my $char = $chars[$index];
			if ( $char eq "\"" )
			{
				# Following check is for character sequence \".  Look at last character in the current
				# field_buffer and if it is a \ this " is not the end of the field.
				if ( $#field_buffer>=0 && $field_buffer[$#field_buffer] eq "\\" )
				{
					push (@field_buffer,$char);
				}
				elsif ( ! $in_quote_field )
				{
					$in_quote_field = 1;
				}
				else
				{
					# If this double quote is followed by a comma double quote ',"' it would be the end of the field
					# otherwise it should included in the field.  Need to ignore white space when searching for the 
					# next two characters.
					# The TCDB never breaks a line at field separators (have not seen it yet anyway) so the code does
					# not need to worry about finding a comma at the end of the line and checking for a double quote at
					# the beginning of the next line.
					my $comma_index = $index+1;
					while ( $comma_index<=$#chars )
					{
						last if ( $chars[$comma_index] =~ m/\S/ );
						$comma_index++;
					}
					my $double_quote_index = $comma_index+1;
					while ( $double_quote_index<=$#chars )
					{
						last if ( $chars[$double_quote_index] =~ m/\S/ && $chars[$double_quote_index] ne ',' );
						$double_quote_index++;
					}
					# Is the next non-white space character a comma followed by a double quote?  If yes then we
					# have reached the end of the field.
					if ( ( $comma_index <= $#chars && $chars[$comma_index] eq "," ) &&
					     ( $double_quote_index <= $#chars && $chars[$double_quote_index] eq "\"" ) )
					{	
						push (@fields,join("",@field_buffer));
						@field_buffer = ();
						# Skip past the comma.
						$index++;
						$in_quote_field = 0;
					}
					# This quote is at end of the line.  Assume it's the last double quote on the CSV line.
					elsif ( $index == $#chars )
					{
						push (@fields,join("",@field_buffer));
						@field_buffer = ();
						$in_quote_field = 0;
					}
					else
					{
						push (@field_buffer,$char);
					}

				}
			}
			#
			# This check is for empty fields, i.e. "field1",,"field2".  
			#
			elsif ( $char eq "," && ! $in_quote_field )
			{
				push (@fields,"");
			}
			else
			{
				if ( $in_quote_field )
				{
					push (@field_buffer,$char);
				}
				else
				{
					# Only allow white space between fields.
					error("Found unexpected character $char after phrase '" . 
					      join("",@field_buffer) . 
					      "' on line $line_count in file $input_filename") if ( $char =~ '\S');
				}
			}
			$index++;
		}
		
		my $next_char = peek(*CSVINPUT);
		my $looks_like_end_of_csv_line = $next_char eq "\"" || $next_char eq "";
		
		# Do we have all the fields we need?
		if ( ($#fields == ($number_of_fields-1)) && (! $in_quote_field) && $looks_like_end_of_csv_line )
		{
			print_fields(\*CSVWORK,\@fields,$testcasenamefield,$tcdb_format);
			$parse_line = "";
			@fields = ();
		}
		# Is this the TCDB export error?  We have one less field than needed and the next line begins with a
		# double quote.
		elsif ( ($#fields == ($number_of_fields-2)) && (! $in_quote_field ) && $looks_like_end_of_csv_line )
		{
			if ( $_ =~ m/^"/ )
			{
				# Pull double quote of the end of the last field.
				$fields[$#fields] =~ s/"$//;
				# Create the missing field.  Need to insert a double quote since print_fields expects a double
				# quote at end of last field.
				push (@fields,"\"");
				print_fields(\*CSVWORK,\@fields,$testcasenamefield,$tcdb_format);
				$parse_line = "";
				@fields = ();
			}
		}
		elsif ( $#fields >= $number_of_fields )
		{
			error("Read too many lines.  Parse line '$parse_line' at line $line_count in file $input_filename");
		}
		else
		# Not enough fields yet.  Need to append a \n to the $parse_line buffer and starting reading 
		# until we find another double quote at the end of the line.
		{
			$parse_line .= "\\n";
		}
	}
	# When End of File is read the parse_line is suppose to be empty.
	error("Reached end of file while parsing '$parse_line'") if ( $parse_line ne "" );
	close(CSVINPUT);
	close(CSVWORK);
	error("Did not find the last double quote in file") if ( $in_quote_field );

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
# Some new lines have been showing up as \\n in exports.
#
sub fix_entities {
	my ($line) = @_;
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
open(XMLOUTPUT, ">", $xml_output_filename) or error("Cannot open file $xml_output_filename");
my %tcdb_user;
my $field_list = remove_field_list($csv_input_filename,$csv_work_filename,$tcdb);
map_TCDB_users(\%tcdb_user) if ( $tcdb );

#
# Process the $field_list variable which comes from the first line of the CSV file.  This line
# defines the columns and column order of the CSV file.
#
# Format of the first line should be in the form: 
#    "Testcase Name","Attributes","Priority","Description","Folder","Creator","Owner",
#    "Pass/Fail Definition","Setup Steps","Cleanup Steps","Steps"
# 
# Columns currently used if they exist are: 
#    attributes - split apart at each comma to become a tag.
#    category - category for test case.
#    cleanupsteps - added to Break Down section.
#    component - component for test case.
#    description - summary unless testcasename is defined.  added to Action section if -tcdb flag
#                  used.
#    environment - split apart at each comma to become a tag.
#    folder - only processed if -tcdb option is supplied.  split apart at each '/'.  based on each
#             teams input one field becomes the category and others tags.  each team defines which
#             sub folders they want to use.
#    longdescription - added to Action section if -tcdb flag used.
#    owner (required) - in TCDB this is a ID that is mapped to a email address from the file
#                       tcdbUsers.
#    passfaildefinition - added to Expected Results section.
#    priority - becomes the priority.  I just a number P is prepended.
#    resdetails - added to Action section if -tcdb flag used.
#    setupsteps - added to Set Up section.
#    steps - added to Action section.
#    testcasename - becomes the summary.  if testcasename is not supplied the description
#                   is the summary.  if testcasename and description are both null a error is
#                   generated.  added to Action section if -tcdb flag used.
#
# The order of the columns is not important.  The columns supplied to Class::CSV will be in
# order found on the first line of the CSV file.
#
# The field_list returned from remove_field_list() will have been:
#    Transformed to lower case.
#    All white space characters removed.
#    All double quotes (") removed.
#    All forward slashes (/) removed.
#

# Column name mapping.  $field_list contains the name of each column in the CSV file.  If your
# column name is that same as a default column name you can covert the column name in $field_list
# and no additional code is needed for the field.
#
# For example if you have a column named 'author' that is really the 'owner' of the Test Case you
# just change 'author' to 'owner' in $field_list.
#
# Add , to front and end of $field_list to make substitution logic easier.  They are remove when
# substitutions are finished.
$field_list = ",$field_list,";
# author is mapped to owner
$field_list =~ s/,author,/,owner,/g;
# result is mapped to passfaildefinition
$field_list =~ s/,result,/,passfaildefinition,/g;
# summary is mapped to testcasename
$field_list =~ s/,summary,/,testcasename,/g;
# tags is mapped to attributes
$field_list =~ s/,tags,/,attributes,/g;
# remove , from beginning of $field_list
$field_list =~ s/^,//;
# remove , from end of $field_list
$field_list =~ s/,$//;

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
print XMLOUTPUT "<testopia version=\"1.1\">\n";

if ( $TEST_PLAN_AUTHOR ne "Change TEST_PLAN_AUTHOR in import.pl user\@novell.com" )
{
	print XMLOUTPUT "	<testplan author=\"" . $TEST_PLAN_AUTHOR . "\" type=\"System\" archived=\"False\">\n";
	print XMLOUTPUT "		<name>" . TEST_PLAN_NAME . "</name>\n";
	print XMLOUTPUT "		<product>" . TEST_PLAN_PRODUCT . "</product>\n";
	print XMLOUTPUT "		<productversion>" . TEST_PLAN_PRODUCT_VERSION . "</productversion>\n";
	print XMLOUTPUT "		<document>" . TEST_PLAN_DOCUMENT . "</document>\n";
	print XMLOUTPUT "	</testplan>\n";
}

my $line_count = 0;
foreach my $line (@{$csv->lines()}) {
	$line_count += 1;
	print XMLOUTPUT "	<testcase ";
	
	error("No owner for Test Case at line $line_count in $csv_work_filename") if ( ! defined($fields{'owner'}) );
	my $owner = $line->owner();
	$owner = $tcdb_user{$line->owner()} if ( $tcdb );
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
	print XMLOUTPUT "		<testplan_reference type=\"" . TEST_PLAN_NAME_TYPE . "\">" . TEST_PLAN_NAME . "</testplan_reference>\n";
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
		$summary .= " - " . fix_entities($environment) if ( $environment ne "" );
	}
	error("No summary for Test Case at line $line_count in $csv_work_filename") if ( $summary eq "" );
	print XMLOUTPUT "		<summary>" . $summary . "</summary>\n";
	print XMLOUTPUT "		<defaulttester>" . fix_entities($owner) . "</defaulttester>\n";
	if ( $tcdb && defined($fields{'folder'}) )
	{
		my @folder = split(/\\/,$line->folder()); 
		print XMLOUTPUT "		<tag>" . fix_entities($folder[4]) . "</tag>\n" if ( defined( $folder[4] ) );
		if ( defined($fields{'category'}) )
		{
			print XMLOUTPUT "		<tag>" . fix_entities($folder[5]) . "</tag>\n" if ( defined( $folder[5] ) );
		}
		else 
		{
			print XMLOUTPUT "		<categoryname>" . fix_entities($folder[5]) . "</categoryname>\n" if ( defined( $folder[5] ) );
		}
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
		print XMLOUTPUT "		<component product=\"" . TEST_PLAN_PRODUCT . "\">" . fix_entities(remove_white_space($line->component())) . "</component>\n";
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
	if ( $tcdb )
	{
		if ( defined($fields{'testcasename'}) && ( $line->testcasename() ne "") )
		{
			print XMLOUTPUT "[TCDB Test Case Name]\n";
			print XMLOUTPUT fix_entities($line->testcasename());
		}
		if ( defined($fields{'description'}) && ( $line->description() ne "") )
		{
			print XMLOUTPUT "\n\n[TCDB Description]\n";
			print XMLOUTPUT fix_entities($line->description());
		}
		if ( defined($fields{'longdescription'}) && ( $line->longdescription() ne "") )
		{
			print XMLOUTPUT "\n\n[TCDB Long Description]\n";
			print XMLOUTPUT fix_entities($line->longdescription());
		}
		if ( defined($fields{'resdetails'}) && ( $line->resdetails() ne "") )
		{
			print XMLOUTPUT "\n\n[TCDB Resolution Details]\n";
			print XMLOUTPUT fix_entities($line->resdetails());
		}
	}
	if ( defined($fields{'steps'}) && ( $line->steps() ne "") )
	{
		print XMLOUTPUT "\n\n[TCDB Steps]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->steps());
	}
	print XMLOUTPUT "</action>\n";
	if ( defined($fields{'passfaildefinition'}) && ( $line->passfaildefinition() ne "") )
	{
		print XMLOUTPUT "		<expectedresults>";
		print XMLOUTPUT "[TCDB Pass Fail Definition]\n" if ( $tcdb );
		print XMLOUTPUT fix_entities($line->passfaildefinition());
		print XMLOUTPUT "</expectedresults>\n";
	}
	print XMLOUTPUT "	</testcase>\n";
  }
print XMLOUTPUT "</testopia>\n";

unlink $csv_work_filename;

exit 0;

__END__

