#!/usr/bin/env perl

# Version: 1.0
# Script last updated: 30May1999 GMD
# Change log:
# <none>

# usually open restrictions.csv
open(F,"$ARGV[0]") || die "Can't open restriction file $ARGV[0]:$!";

print <<EOM;
/*
  ======================================================================
  File: restrictionrecords.c
    
 (C) COPYRIGHT 1999 Graham Davison
 mailto:g.m.davison\@computer.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 ======================================================================*/


/*
 * THIS FILE IS MACHINE GENERATED DO NOT EDIT
 */


typedef struct icalrestriction_record {
	icalproperty_method method;
	icalcomponent_kind component;
	icalproperty_kind property;
	icalcomponent_kind subcomponent;
	icalrestriction_kind restriction;
} icalrestriction_record;


icalrestriction_record icalrestriction_records[] = 
{
EOM

my $last_method = "";
my $last_component = "";
my $last_property = "";
my $need_header = 0;

while(<F>)
{
	chop;
	
	# split line at commas
	my ($method,$component,$property,$subcomponent,$restriction)=split(/\,/,$_);
	
	#
	#put in code to generate comments here!
	#
	if ($method ne $last_method)
	{
		$need_header = 1;
		$last_method = $method;
	}
	if ($component ne $last_component)
	{
		$need_header = 1;
		$last_component = $component;
	}
	
	if ($need_header)
	{
		print "\n\t/* METHOD: ${method}, COMPONENT: ${component} */\n";
		$need_header = 0;
	}
	
	foreach $item ($component,$property,$subcomponent,$restriction)
	{
		# handle special cases.
		if ($item eq "NONE")
			{ $item = "NO"; }
		else { if (substr($item,0,1) eq "X")
			{ $item = "X"; }}
		
		# strip out dashes
		$item = join("",split(/-/,$item));
	}
	# strip leading V from component names
	$component =~ s/^(V?)(\w+?)((SAVINGS)?)((TIME)?)$/$2/;
	$subcomponent =~ s/^V(\w+)/$1/;

	print "\t\{ICAL_METHOD_${method},ICAL_${component}_COMPONENT,";
	print "ICAL_${property}_PROPERTY,ICAL_${subcomponent}_COMPONENT,";
	print "ICAL_RESTRICTION_${restriction}\},\n";
	
}

print <<EOM;
	
	/* END */
	{ICAL_METHOD_NONE,ICAL_NO_COMPONENT,ICAL_NO_PROPERTY,ICAL_NO_COMPONENT,ICAL_RESTRICTION_NONE}
};
EOM
