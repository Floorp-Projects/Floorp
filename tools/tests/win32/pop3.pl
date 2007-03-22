#!/perl/bin/perl

@ARGV;
$BaseDir = $ARGV[0];

open (REPORT_FILE, '>>report.html');

$count = 0;
#if (-e 'c:\program files\netscape\seamonkey\x86rel\pop3test.exe')
$PopFile = $BaseDir . '\pop3test.exe';
if (-e $PopFile)
{
	open (STDOUT, ">popresult.txt");
	select STDOUT; $| =1;

	open (STDIN, "<poptestdata.txt");

	$temp = $BaseDir;
	$temp =~ s/\\/\\\\/g;
	use Win32::Process;
	use Win32;
	$cmdLine = "pop3test";
	Win32::Process::Create($ProcessObj,
#				"c:\\program files\\netscape\\seamonkey\\x86rel\\pop3test.exe",
				"$temp.\\pop3test.exe",
				$cmdLine,
                         1,
                         NORMAL_PRIORITY_CLASS,
#                        "c:\\program files\\netscape\\seamonkey\\x86rel\\");
                         "$temp");
	#print APPRUNNER_LOG $_;
	sleep (3);
	$ProcessObj->GetExitCode( $ExitCode );
	$ProcessObj->Kill( $ExitCode );
	close (STDOUT);
	close (STDIN);

	open (POPRES_FILE, '< popresult.txt');
	$ThisLine = "";
	$count = 0;
	while (<POPRES_FILE>)
	{
		$ThisLine = $_;
		chop ($ThisLine);
		if ((/User Name: smoketst/) || (/Pop Server: nsmail-1/) || (/Pop Password: l00N!e/))
		{
			$count = 0;
		}
		
		if (/Ya'll got mail!/)
		{
			$count = 0;
		}
	}
	if ($count == 0)
	{
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "<B>\n");
		print (REPORT_FILE "POP TEST PASSED");
		print (REPORT_FILE "</B>\n");
		print (REPORT_FILE "<BR>\n");
	}
	else
 	{
		print (REPORT_FILE "<BR>\n");
		print (REPORT_FILE "<B>\n");
		print (REPORT_FILE "POP TEST FAILED");
		print (REPORT_FILE "</B>\n");
		print (REPORT_FILE "<BR>\n");
	}
}
else
{
	print (REPORT_FILE "<B>\n");
	print (REPORT_FILE "Pop3Test.exe File does NOT exist");
	print (REPORT_FILE "</B>\n");
}

close (REPORT_FILE);
close (POPRES_FILE);

print (REPORT_FILE "<HR>\n");
print (REPORT_FILE "<BR>\n");

print (REPORT_FILE "</BODY>\n");
print (REPORT_FILE "</HTML>\n");
