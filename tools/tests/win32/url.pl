#!/perl/bin/perl

@ARGV;
$BaseDir = $ARGV[0];

#############################################

open (REPORT_FILE, '>>report.html');

print (REPORT_FILE "<HR>\n");
print (REPORT_FILE "<B><CENTER><font size=+2>\n");
print (REPORT_FILE "Loading Sites Results");
print (REPORT_FILE "</font></CENTER></B>\n");

open (URL_FILE, '<url.txt');

$i = 0;
while (<URL_FILE>)
{
	$ThisLine = $_;
	chop ($ThisLine);
	$url_list[$i] = $ThisLine;
	$i += 1;
}
for ($i = 0; $i < 21; $i ++)
{ 
	$count = 0;
	open (STDOUT, ">log.txt");
	select STDOUT; $| =1;
	
	$temp = $BaseDir;
	$temp =~ s/\\/\\\\/g;
	use Win32::Process;
	use Win32;
	$cmdLine = "apprunner $url_list[$i]";
	Win32::Process::Create($ProcessObj,
#				"c:\\program files\\netscape\\seamonkey\\x86rel\\apprunner.exe",
				"$temp.\\apprunner.exe",
				$cmdLine,
                         1,
                         NORMAL_PRIORITY_CLASS,
#                         "c:\\program files\\netscape\\seamonkey\\x86rel\\");
				 "$temp");

	sleep (60);
	$ProcessObj->GetExitCode( $ExitCode );
	$ProcessObj->Kill( $ExitCode );
	close (STDOUT);
	
	open (APP_LOG_FILE, '< log.txt');
	while (<APP_LOG_FILE>)
	{
		$ThisLine = $_;
		chop ($ThisLine);
		if (/loaded successfully/)
		{
			$count = $count + 1;	
		}
	}
	if ($count >= 1)
	{
		print (REPORT_FILE "<B>\n");
		print (REPORT_FILE "@url_list[$i] Loaded Successfully");
		print (REPORT_FILE "</B>\n");
		print (REPORT_FILE "<BR>\n");
		$count = 0;
	}
	else
	{
		print (REPORT_FILE "<B><FONT COLOR='#FF0000'>\n");
		print (REPORT_FILE "$url_list[$i] NOT Loaded Successfully");
		print (REPORT_FILE "</FONT></B>\n");
		print (REPORT_FILE "<BR>\n");
	}
	close (APP_LOG_FILE);
}
close (REPORT_FILE);
