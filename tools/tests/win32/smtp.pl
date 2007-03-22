#!/perl/bin/perl

@ARGV;
$BaseDir = $ARGV[0];

open (REPORT_FILE, '>>report.html');

$count = 0;
#if (-e 'c:\program files\netscape\seamonkey\x86rel\smtptest.exe')
print ($BaseDir, "\n");
$SmtpFile = $BaseDir . '\Smtptest.exe';
print ($SmtpFile, "\n");
if (-e $SmtpFile)
{
	open (STDOUT, ">smtpresult.txt");
	select STDOUT; $| =1;

	open (STDIN, "<smtptestdata.txt");
	
	$temp = $BaseDir;
	$temp =~ s/\\/\\\\/g;
	use Win32::Process;
	use Win32;
	$cmdLine = "smtptest";
	Win32::Process::Create($ProcessObj,
#				"c:\\program files\\netscape\\seamonkey\\x86rel\\smtptest.exe",
				"$temp.\\smtptest.exe",
				$cmdLine,
                         1,
                         NORMAL_PRIORITY_CLASS,
#                         "c:\\program files\\netscape\\seamonkey\\x86rel\\");
                         "$temp");

	#print APPRUNNER_LOG $_;
	sleep (3);
	$ProcessObj->GetExitCode( $ExitCode );
	$ProcessObj->Kill( $ExitCode );
	close (STDOUT);

	open (SMTPRES_FILE, '< smtpresult.txt');
	$ThisLine = "";
	while (<SMTPRES_FILE>)
	{
		$ThisLine = $_;
		chop ($ThisLine);
		if (/Message Sent: PASSED/)
		{
				$count = $count + 1;
		}
	}
	if ($count >= 1)
	{
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "<B>\n");
		print (REPORT_FILE "SMTP TEST PASSED");
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "$ThisLine");
		print (REPORT_FILE "</B>\n");
		print (REPORT_FILE "<BR>\n");	
	}
	else
	{
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "<B>\n");
		print (REPORT_FILE "SMTP TEST FAILED");
		print (REPORT_FILE "</B>\n");
		print (REPORT_FILE "<BR>\n");	
	}
}
else
{
	print (REPORT_FILE "<B>\n");
	print (REPORT_FILE "smtpTest.exe File does NOT exist");
	print (REPORT_FILE "</B>\n");
}
print (REPORT_FILE "<HR>\n");

close (REPORT_FILE);
