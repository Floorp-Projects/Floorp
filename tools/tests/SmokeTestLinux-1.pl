#! /usr/bin/perl
# hacked together by bsharma@netscape.com and phillip@netscape.com
use Cwd;
use FileHandle;

#some vars that depend on the location of apprunner.
#apprunner_bin should be the only thing you need to change to get this to work...
$apprunner_bin = '/u/phillip/seamonkey/linux/package';
#$apprunner_bin = '/build/mozilla/dist/bin';

$apprunner = "$apprunner_bin/apprunner";
$apprunner_samples = "$apprunner_bin/res/samples";
$apprunner_log = '/tmp/apprunnerlog.txt';
$mail_log = '/tmp/maillog.txt';
$test_duration = 30; # seconds

# we fork and launch apprunner in a few spots around here, so let's define
# this just once:
$ENV{'MOZILLA_FIVE_HOME'}="$apprunner_bin";
$ENV{'LD_LIBRARY_PATH'}="/usr/lib:/lib:$apprunner_bin";
# here are a few subroutines we use:
# get_build_date - look through navigator.xul for the build id (1999-04-18-08)
#                  returns the string
# get_date - returns a preformatted string containing the date and time
# launch_apprunner - takes a parameter (or the null string) 


# so it begins
open (REPORT_FILE, '>report.html');
REPORT_FILE->autoflush();
print (REPORT_FILE "<HTML><HEAD><TITLE>Smoke Test Report File </TITLE></HEAD>\n");
print (REPORT_FILE "<BODY>\n");
print (REPORT_FILE "<H1><CENTER> Seamonkey Build Smoke Tests Report\n");
print (REPORT_FILE "<BR>Linux</CENTER></H1><B><CENTER>\n");
print (REPORT_FILE "<BR>\n");
print (REPORT_FILE &get_date);
print (REPORT_FILE "</B></CENTER>\n <BR>\n <B><CENTER>\n Build Number: " . &get_build_date . " </CENTER></B>\n <BR>\n <HR>\n");


&launch_apprunner();

# pessimistically assume that this will fail
# just look for the string loaded successfully in the log file
$load_result = "NOT";
open (APP_LOG_FILE, "< $apprunner_log");
while (<APP_LOG_FILE>)
{
    chop;
    if (/loaded successfully/)
    {    #since the start page was loaded successfully, 
        #we can say that Apprunner loaded successfully
        $load_result = "";
    }
}
close (APP_LOG_FILE);

print (REPORT_FILE "<B><Center>\n");
# load_result tells you if the test failed
print (REPORT_FILE "Apprunner $load_result Loaded Successfully");

print (REPORT_FILE "</B></Center>\n");
print (REPORT_FILE "<BR>\n");

print (REPORT_FILE "<HR>\n");
print (REPORT_FILE "<B><CENTER><font size=+2>\n");
print (REPORT_FILE "Loading Sites Results");
print (REPORT_FILE "</font></CENTER></B>\n");

@url_list = ("http://www.yahoo.com",
            "http://www.netscape.com",
            "http://www.excite.com",
            "http://www.microsoft.com",
            "http://www.city.net",
            "http://www.mirabilis.com",
            "http://www.pathfinder.com/welcome",
            "http://www.warnerbros.com/home_moz3_day.html",
            "http://www.cnn.com",
            "http://www.usatoday.com",
            "http://www.disney.go.com",
            "http://www.hotwired.com",
            "http://www.hotbot.com",
            "http://slip/projects/marvin/bft/browser/bft_frame_index.html",
            "file://$apprunner_samples/test6.html", 
            "http://slip/projects/marvin/bft/browser/bft_browser_applet.html",
            "http://www.abcnews.com",
            "http://slip/projects/marvin/bft/browser/bft_browser_imagemap.html",
            "file://$apprunner_samples/test2.html",
            "file://$apprunner_samples/test13.html",
            "file://$apprunner_samples/test13.html",
            "file://$apprunner_samples/test2.html",
            "http://slip/projects/marvin/bft/browser/bft_browser_html_mix3.html",
            "http://slip/projects/marvin/bft/browser/bft_browser_link.html");

my $i;
my $style;

# launch apprunner once for every browser test in the url_list
################################################################################
for ($i = 0; $i < $#url_list; $i ++)
{ 
    print " @url_list[$i]\n";
    &launch_apprunner(@url_list[$i]);
        $load_result = "NOT";
    open (APP_LOG_FILE, "< $apprunner_log");
    while (<APP_LOG_FILE>)
    {
        chop;
        #if (/@url_list[$i]/ and /loaded successfully/)
        if (/loaded successfully/)
        {
            $load_result = "";
        }
    }
    # print in red if there's a failure 
    if ( $load_result eq "NOT" ) {
        $style = "style='color: red;'";
    } else { $style = ""; }
    print (REPORT_FILE "<B $style>\n<a href=\"@url_list[$i]\">@url_list[$i]</a> $load_result Loaded Successfully</B>\n<BR>\n");
}
close (APP_LOG_FILE);

# now it's time to check mail
# not yet functional
################################################################################

#open (LOG_FILE, "< $mail_log");
#while (<LOG_FILE>)
#{
#    chop;
#       $load_result = "NOT";
#    if (/Mailbox Done/){
#        $load_result = "";
#    }
#    print (REPORT_FILE "<B>\n$load_result</B>\n<BR>\n");
#}

# close the report, and we're done!
################################################################################
print (REPORT_FILE "<HR>\n<BR>\n<BR>\n<B><CENTER>\n");
print (REPORT_FILE &get_date());
print (REPORT_FILE "</CENTER></B>\n");
print (REPORT_FILE "<BR>\n<BR>\n<HR>\n<BR>\n<BR>\n");
print (REPORT_FILE "</BODY>\n");
print (REPORT_FILE "</HTML>\n");
close (REPORT_FILE);


################################################################################
################################################################################
##### Subroutines go here . . . ################################################
################################################################################
################################################################################
sub get_build_date {
    open (XUL_FILE, "< $apprunner_samples/navigator.xul");
    $BuildNo = "";
    $LineList;
    while (<XUL_FILE>)
    {
        chop;
        if (/Build ID/)
        {
            @LineList = split / /;
            $BuildNo = $LineList[4];
        }
    }
    close( XUL_FILE );
    $BuildNo =~ s/"/ /g;
    return $BuildNo;
}

sub launch_apprunner{
    my $url = shift || "";
    if ( $pid = fork ) {
        # parent will wait $test_duration seconds, then kill kid.
        sleep ( $test_duration );
        system("killall -9 apprunner");
    }
    else {
        #child runs aprrunner to see if it even launches.
        print "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n";
        if ( $url eq "" ){
            exec "$apprunner > $apprunner_log 2>&1";
        } else {
            exec "$apprunner -url $url   > $apprunner_log 2>&1";
        }
        print "exec error: this line of code should never be reached\n" and die;
    }
}

sub get_date {
    ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)= localtime;
    %weekday= (
       "1", "Monday",
       '2', 'Tuesday',
       '3', 'Wednesday',
       '4', 'Thursday',
       '5', 'Friday',
       '6', 'Saturday',
       '7', 'Sunday',
    );
    $mon += 1;
    return sprintf "%s %02d/%02d/19%02d  %02d:%02d:%02d" ,$weekday{$wday},$mon,$mday,$year,$hour,$min,$sec;
}

