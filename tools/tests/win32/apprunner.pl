#!/perl/bin/perl

@ARGV;
$BaseDir = $ARGV[0];
#print ($BaseDir, "\n");
#############################################

open (REPORT_FILE, '>report.html');
#open (XUL_FILE, '< c:\program files\netscape\seamonkey\x86rel\res\samples\navigator.xul') || die "cannot open hardcode path \n";
$NavXul = $BaseDir . '\res\samples\navigator.xul';
print ($NavXul, "\n");
open (XUL_FILE, "< $NavXul") || die "cannot open $NavXul";

$BuildNo = "";
$LineList;
while (<XUL_FILE>)
{
	$ThisLine = $_;
	chop ($ThisLine);
	if (/Build ID/)
	{
		@LineList = split (/ /, $ThisLine);
		$BuildNo = $LineList[3];
	}
}
$BuildNo =~ s/"/ /g;

($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)= localtime;
%weekday= (
       "1", "$day",
       '2', 'Tuesday',
       '3', 'Wednesday',
       '4', 'Thursday',
       '5', 'Friday',
       '6', 'Saturday',
       '7', 'Sunday',
);
$mon += 1;
print (REPORT_FILE "<HTML><HEAD><TITLE>Smoke Test Report File </TITLE></HEAD>\n");
print (REPORT_FILE "<BODY>\n");
print (REPORT_FILE "<H1><CENTER> Seamonkey Build Smoke Tests Report\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE "Win32</CENTER></H1>\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE "<B><CENTER>\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE "$weekday{$wday} $mon/$mday/19$year  ");
printf (REPORT_FILE "%02d:%02d:%02d", $hour,$min,$sec);
print (REPORT_FILE "</B></CENTER>\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE "<B><CENTER>\n");
print (REPORT_FILE "Build Number: $BuildNo");
print (REPORT_FILE "</CENTER></B>\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE "<HR>\n");

#############################################

open (STDOUT, ">log.txt");
select STDOUT; $| =1;

$temp = $BaseDir;
$temp =~ s/\\/\\\\/g;
use Win32::Process;
use Win32;
$cmdLine = "apprunner";
Win32::Process::Create($ProcessObj,
#				"c:\\program files\\netscape\\seamonkey\\x86rel\\apprunner.exe",
				"$temp.\\apprunner.exe",
				$cmdLine,
                         1,
                         NORMAL_PRIORITY_CLASS,
#                         "c:\\program files\\netscape\\seamonkey\\x86rel\\");
                         "$temp");

#print APPRUNNER_LOG $_;
sleep (30);
$ProcessObj->GetExitCode( $ExitCode );
$ProcessObj->Kill( $ExitCode );
close (STDOUT);

#############################################

print (REPORT_FILE "<B><CENTER><font size=+2>\n");
print (REPORT_FILE "Launch Apprunner Results");
print (REPORT_FILE "</font></CENTER></B>\n");

$count = 0;
open (APP_LOG_FILE, '< log.txt');
while (<APP_LOG_FILE>)
{
	$ThisLine = $_;
	chop ($ThisLine);
	if (/loaded successfully/)
	{
		#print (REPORT_FILE "<B>\n");
		#print (REPORT_FILE "$ThisLine");
		#print (REPORT_FILE "</B>\n");
		#print (REPORT_FILE "<BR>\n");
		$count = $count + 1;	
	}
}
if ($count >= 1)
{
	print (REPORT_FILE "<B><Center>\n");
	print (REPORT_FILE "Apprunner Loaded Successfully");
	print (REPORT_FILE "</B></Center>\n");
	print (REPORT_FILE "<BR>\n");
}
else
{
	print (REPORT_FILE "<B><Center>\n");
	print (REPORT_FILE "Apprunner NOT Loaded Successfully");
	print (REPORT_FILE "</B></Center>\n");
	print (REPORT_FILE "<BR>\n");
}
close (APP_LOG_FILE);

#########################################

close (REPORT_FILE);
