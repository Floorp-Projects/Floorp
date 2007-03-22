#!/usr/bin/perl

$viewer_path = &ParseArgs();

if (defined($OSNAME)) {
   $ostype = "unix";
}
else {
   $ostype = "win32";
}

#Win32 Autoconfig

if ($ostype eq "win32") {

    $viewer = $viewer_path . "\viewer.exe";
    print $viewer_path . "\\viewer.exe" . "\n";
    if (-e $viewer_path . "\\viewer.exe") {
        system ($viewer_path . '\\viewer.exe -v -d 15 -f url.txt > result.txt');
    }
    else {
       die $viewer_path . "viewer doesn't exist! Check your path.\n";
   }
}

#=================================================================

#Linux Autoconfig

if ($ostype eq "unix") {

    $viewer = $viewer_path . "/viewer";
    if (-e $viewer_path . "/viewer") {
        $ENV{'MOZILLA_HOME'}=$viewer_path;
        $ENV{'LD_LIBRARY_PATH'}=$viewer_path;
        system ($viewer_path . '/viewer -v -d 9 -f ./url.txt > result.txt');
    }
    else {
       die $viewer_path . "/viewer doesn't exist! Check your path.\n";
   }
}
#=================================================================

open (ANALYSIS_FILE, '>analysis.html') || die ("can't open file jim");
($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)=localtime(time);
%weekday= (
       "1", "$day",
       '2', 'Tuesday',
       '3', 'Wednesday',
       '4', 'Thursday',
       '5', 'Friday',
       '6', 'Saturday',
       '7', 'Sunday',
);
if ($hour > 12)
{
    $hour = $hour - 12;
}
$mon += 1;
print "Content-type:text/html\n\n";
print (ANALYSIS_FILE "<HTML><HEAD><TITLE> Load URl's Analysis File </TITLE></HEAD>\n");
print (ANALYSIS_FILE "<BODY>\n");
print (ANALYSIS_FILE "<H2><CENTER> Load URL's Analysis File </CENTER></H2>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<HR>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B><CENTER>\n");
print (ANALYSIS_FILE "Day Date Year and Time when Program started:  ");
print (ANALYSIS_FILE "</B></CENTER>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B><CENTER>\n");
print (ANALYSIS_FILE "$weekday{$wday} $mon/$mday/19$year $hour:$min:$sec");
print (ANALYSIS_FILE "</B></CENTER>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<HR>\n");

open (IN_FILE, '<result.txt');
open (OUT_FILE, '>loaded.txt');
open (URL_FILE, '<Url.txt');
open (NOMATCH_FILE, '>notloaded.txt');

$ThisLine;
@Url_List = (0..24,0..1);
$count=0;
while (<URL_FILE>)
{
	$ThisLine = $_;
	chomp ($ThisLine);
	#push (@Url_List, "$ThisLine 0");
	@Url_List[$count]->[0] = $ThisLine;
	@Url_List[$count]->[1] = 0;
	#print "@Url_List[$count]->[0] ";
	#print "@Url_List[$count]->[1] \n";
	$count++;
}
while (<IN_FILE>)
{
	$ThisLine = $_;
	if (/done loading/)
	{
		print (OUT_FILE "$ThisLine");
	}
}
close (OUT_FILE);

open (OUT_FILE, '<loaded.txt');

$ThisLine = "";
$NumItems = @Url_List;
$ThisItem;
$InList;
while (<OUT_FILE>)
{
	$ThisLine = $_;
	for ($i = 0; $i < $NumItems-1; $i += 1)
	{
		$InList = @Url_List[$i]->[0];
		if ($ThisLine =~ /$InList/)
		{
			#print @Url_List[$i]->[0];
			#print "\n";
			@Url_List[$i]->[1] = 1;	
		}
	}
}
for ($i = 0; $i < $NumItems-1; $i += 1)
{
	if (@Url_List[$i]->[1] == 0)
	{
		print (NOMATCH_FILE "@Url_List[$i]->[0]\n");
	}
}
close (OUT_FILE);
close (NOMATCH_FILE);

open (OUT_FILE, '<loaded.txt');
open (NOMATCH_FILE, '<notloaded.txt');

$LoadItems;
$NotLoadItems;

while (<OUT_FILE>)
{
	$LoadItems += 1;
}

while (<NOMATCH_FILE>)
{
	$NotLoadItems += 1;
}

close (OUT_FILE);
close (NOMATCH_FILE);

($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)=localtime(time);
%weekday= (
       "1", "$day",
       '2', 'Tuesday',
       '3', 'Wednesday',
       '4', 'Thursday',
       '5', 'Friday',
       '6', 'Saturday',
       '7', 'Sunday',
);
if ($hour > 12)
{
    $hour = $hour - 12;
}
$mon += 1;

print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B><CENTER>\n");
print (ANALYSIS_FILE "Day Date Year and Time when Program stopped:  ");
print (ANALYSIS_FILE "</B></CENTER>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B><CENTER>\n");
print (ANALYSIS_FILE "$weekday{$wday} $mon/$mday/19$year $hour:$min:$sec");
print (ANALYSIS_FILE "</B></CENTER>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<HR>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B>\n");
print (ANALYSIS_FILE "Number of URL's NOT loaded = ");
print (ANALYSIS_FILE "$NotLoadItems\n");
print (ANALYSIS_FILE "</B>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<BR>\n");

open (OUT_FILE, '<loaded.txt');
open (NOMATCH_FILE, '<notloaded.txt');

while (<NOMATCH_FILE>)
{
	$ThisLine = $_;
	print (ANALYSIS_FILE "<A HREF='$ThisLine'>$ThisLine</A>\n");
	print (ANALYSIS_FILE "<BR>\n");
}

print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<B>\n");
print (ANALYSIS_FILE "Number of URL's loaded = ");
print (ANALYSIS_FILE "$LoadItems\n");
print (ANALYSIS_FILE "</B>\n");
print (ANALYSIS_FILE "<BR>\n");
print (ANALYSIS_FILE "<BR>\n");

while (<OUT_FILE>)
{
	$ThisLine = $_;
	print (ANALYSIS_FILE "$ThisLine\n");
	print (ANALYSIS_FILE "<BR>\n");
}

print (ANALYSIS_FILE "</BODY>\n");
print (ANALYSIS_FILE "</HTML>\n");

close (URL_FILE);
close (IN_FILE);
close (OUT_FILE);
close (NOMATCH_FILE);
close (ANALYSIS_FILE);

sub ParseArgs {
    my($i);

    if( (@ARGV == 0) || (@ARGV > 1) ) {
        &PrintUsage;
    }
    else {
       $viewer_path = $ARGV[0];
    }
    return $viewer_path;
}

sub PrintUsage {
   die "usage: LoadUrl.pl <directory containing viewer app>";
}

