#!/perl/bin/perl

@ARGV;
$BaseDir = $ARGV[0];

open (REPORT_FILE, '>>report.html');

print (REPORT_FILE "<HR>\n");

print (REPORT_FILE "<B><CENTER><font size=+2>\n");
print (REPORT_FILE "Apprunner Mail Results");
print (REPORT_FILE "</font></CENTER></B>\n");

open (STDOUT, ">log.txt");
select STDOUT; $| =1;

$temp = $BaseDir;
$temp =~ s/\\/\\\\/g;
use Win32::Process;
use Win32;
$cmdLine = "apprunner -mail";
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

$status = 0;

#open (LOG_FILE, '< c:\program files\netscape\seamonkey\x86rel\MailSmokeTest.txt');
$MailRes = $BaseDir . '\MailSmokeTest.txt';
open (MAIL_FILE, "< $MailRes");
$ThisLine;
while (<LOG_FILE>)
{
	$ThisLine = $_;
	chop ($ThisLine);
	if (/Mailbox Done/)
	{
		print (REPORT_FILE "<B><Center>\n");
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "Mail Window Loaded SUCCESSFULLY");
		$status = 1;
		print (REPORT_FILE "<Center></B>\n");
		print (REPORT_FILE "<BR>\n");
	}
	else
	{
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "<B><Center>\n");
		print (REPORT_FILE "MAIL NOT LOADED");
		print (REPORT_FILE "</Center></B>\n");
		print (REPORT_FILE "<BR>\n");
	}
}
print ("AFTER Mail\n");
print (REPORT_FILE "<HR>\n");

if ($status >= 1)
{
	print (REPORT_FILE "<BR>\n");
	print (REPORT_FILE "<B><Center>\n");
	print (REPORT_FILE "Application Terminated.");
	print (REPORT_FILE "<BR>\n");
	print (REPORT_FILE "MAIL NOT LOADED");
	print (REPORT_FILE "</Center></B>\n");
	print (REPORT_FILE "<BR>\n");
#	die ("Application terminated\n");
}
#if (!(-e 'c:\program files\netscape\seamonkey\x86rel\MailSmokeTest.txt'))
print ($MailRes);
if ((-e $MailRes))
{
	print (REPORT_FILE "<B><Center>\n");
	print (REPORT_FILE "</Center></B>\n");
	print (REPORT_FILE "<BR>\n");
}
else
{
	print (REPORT_FILE "<B><Center>\n");
	print (REPORT_FILE "Mail Output file NOT found.");	
	print (REPORT_FILE "</Center></B>\n");
	print (REPORT_FILE "<BR>\n");
}
close (LOG_FILE);
close (REPORT_FILE);