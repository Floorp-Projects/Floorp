#! /usr/bin/perl
# hacked together by bsharma@netscape.com and phillip@netscape.com
use Cwd;
use FileHandle;
use Benchmark;

#some vars that depend on the location of apprunner.
$layout_time = 4; # seconds after document is loaded successfully before apprunner is killed
$test_duration = 30; # seconds before i deem this url timed out

# TODO: get apprunner working with relative paths. the problem with 
#       this is LD_LIBRARY_PATH and MOZILLA_FIVE_HOME want absolute
#       paths, so we need to get the pwd and replace 'tools/tests/unix'
#       with 'dist/bin' -- then we're golden.
#$apprunner_bin = '/u/phillip/seamonkey/linux/package';


chop($start_dir = `pwd`);
$apprunner_bin = '../../../dist/bin';

chdir($apprunner_bin);

# need the absolute path, not the relative one
chop($apprunner_bin = `pwd`);

#nothing else needs to be changed
$apprunner = "$apprunner_bin/apprunner";
$apprunner_samples = "$apprunner_bin/res/samples";
$apprunner_log = $start_dir . '/smoketest.log';
#$mail_log = '/tmp/maillog.txt'; 
#open (LOG_FILE, ">> $apprunner_log");
#print LOG_FILE "Starting directory is $start_dir \n";
#print LOG_FULE "apprunner binary is $apprunner \n";
#print LOG_FULE "Samples directory is $apprunner_samples \n";
#close (LOG_FILE);

# we fork and launch apprunner in a few spots around here, so let's define
# this just once:
$ENV{'MOZILLA_FIVE_HOME'}="$apprunner_bin";
$ENV{'LD_LIBRARY_PATH'}="/usr/lib:/lib:$apprunner_bin";

# here are a few subroutines we use:
# get_build_date - look through navigator.xul for the build id (1999-04-18-08)
#                  returns the string
# get_date - returns a preformatted string containing the date and time
# test_url - takes a string url as an optional parameter 

$t0 = new Benchmark;

&main();

$t1 = new Benchmark;
$launchtimea = timediff($t1,$t0);
@launchtime = split (/ /, timestr($launchtimea));
$sec = $launchtime[0];

use integer;
$hours = $sec / 3600;
$sec -= $hours * 3600;
$mins = $sec / 60;
$sec -= $mins * 60;
print "the whole Smoketest took $hours hours, $mins mins, and $sec seconds.\n";





# so it begins
sub main {
    open (REPORT_FILE, '>report.html');
    REPORT_FILE->autoflush();
    print (REPORT_FILE "<HTML><HEAD><TITLE>Smoke Test Report File </TITLE></HEAD>\n");
    print (REPORT_FILE "<BODY>\n");
    print (REPORT_FILE "<H1><CENTER> Seamonkey Build Smoke Tests Report\n");
    print (REPORT_FILE "<BR>Linux</CENTER></H1><B><CENTER>\n");
    print (REPORT_FILE "<BR>\n");
    print (REPORT_FILE &get_date);
    print (REPORT_FILE "</B></CENTER>\n <BR>\n <B><CENTER>\n Build Number: " . &get_build_date . " </CENTER></B>\n <BR>\n <HR>\n");
    
    
    print "testing launch";
    $load_result = "";
    $ret = &test_url;
    $load_result = "NOT" if ( $ret eq 'E1-FAILED' );
    $load_result = "NOT" if ( $ret eq 'E2-FAILED' );
    $load_result = "NOT" if ( $ret eq 'E3-FAILED' );
    $load_result = "NOT" if ( $ret eq 'E4-FAILED' );
    if ( $load_result eq "NOT" ) {
        print "FAILED\n";
    } else {
        print "OK\n";
    }
    
    print (REPORT_FILE "<B><Center>\n");
    # load_result tells you if the test failed
    print (REPORT_FILE "Apprunner $load_result Loaded Successfully");
    
    print (REPORT_FILE "</B></Center>\n");
    print (REPORT_FILE "<BR>\n");
    
    print (REPORT_FILE "<HR>\n");
    print (REPORT_FILE "<B><CENTER><font size=+2>\n");
    print (REPORT_FILE "Loading Sites Results");
    print (REPORT_FILE "</font></CENTER></B>\n");
    
    &get_url_list;

    my $i;
    my $style;
    
    # launch apprunner once for every browser test in the url_list
    ################################################################################
    foreach $cur_url (@url_list)
    { 
        $load_result = "";
    
        print "$cur_url";
    
        $ret = &test_url( $cur_url );
        $load_result = "NOT" if ( $ret eq 'E1-FAILED' );
        $load_result = "NOT" if ( $ret eq 'E2-FAILED' );
        $load_result = "NOT" if ( $ret eq 'E3-FAILED' );
        $load_result = "NOT" if ( $ret eq 'E4-FAILED' );
    
        # print in red if there's a failure 
        if ( $load_result eq "NOT" ) {
            $style = "style='color: red;'";
        print "FAILED\n";
        } else {
            $style = "";
        print "OK\n";
        }
        print (REPORT_FILE "<B $style>\n<a href=\"$cur_url\">$cur_url</a> $load_result Loaded Successfully</B>\n<BR>\n");
    }
    
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
    
}

################################################################################
################################################################################
##### Subroutines go here . . . ################################################
################################################################################
################################################################################


#
# my_kill -- takes a pid or the string "apprunner".
#            essentially calls `kill -9 $pid`.
#            the reason this exists is because i can't figure
#            out how to kill apprunner yet, since i can't grab
#            it's pid. (it's harder than you think.)
#            when i figure it out, i'll fix it here, then all will 
#            be great. until then, all apprunners are killed!)
################################################################
sub my_kill{
    local $pid = shift || "";
    
    if ($pid eq ""){
        return;
    }
    elsif ($pid eq "apprunner"){
        system ("killall -9 apprunner");
    } else {
        kill 9, $pid;
    }
}
    
#
# test_url -- takes a url as a parameter and launches apprunner
#             with it. returns the page-loading-performance as
#             a string of numbers with a floating point *IF*
#             the page actually loads. if the page doesn't load,
#             it returns the string 'En-FAILED', with n in 1..3
################################################################
sub test_url {
    $| = 1; # gotta love autoflushing
    local $reaperchild;
    local $url = shift || "";
    if( $url ne "" ) { $url = "-url " .  $url; }
    local $run_time = 'E2-FAILED';

    if ( $reaperchild = fork ) {
    ## ok, the parent spins until apprunner
    ## loads a doc successfully or unsuccessfully, or
    ## until the reaperchild kills apprunner
    open STATUS, "$apprunner $url 2>&1 |";

    open LOG_FILE, ">> $apprunner_log";
    LOG_FILE->autoflush();
    unless ($url eq ""){ print LOG_FILE "testing url: $url\n" }
    while (<STATUS>){
    	print LOG_FILE;
        #     Document: Done (7.918 secs)
        if (/Document: Done \((\d*\.\d*) secs/) {
            $run_time = $1;
	    last; # we can quit if we get a Document: Done message
        }
        
        if (/loaded successfully/) {
	    if ($run_time eq 'E2-FAILED') { 
		$run_time = -1 ;
	    }
	    last; # we can quit if we get a loaded successfully message
	}
        $run_time = 'E3-FAILED' and last if (/Error loading URL/);
            
    }

    # now that the testing ugliness is over, we make
    # sure everybody is dead. then we can return our
    # performance findings.
    print "|";
    sleep $layout_time;
    &my_kill( $reaperchild );
    &my_kill( "apprunner" );
    # wait for reaperchild to die
    if ( kill 0 => $reaperchild ){
        waitpid $reaperchild, 0;
    }
    if ( -f "timeout.txt" ) {
        unlink "timeout.txt";
        $run_time = 'E1-FAILED';
    }

    if ( $run_time eq "0" ){
    	print "error, runtime == 0. this is a problem with Timer:Init() being called with a bogus value";
	$run_time = 'E4-FAILED';
    }
    unless ($url eq ""){ print LOG_FILE "done testing url: $url\n" }
    close LOG_FILE;
    return $run_time;
    } else {
    # child waits for a while, then kills apprunner 
    # the test must go on.
    for( $j=0; $j<$test_duration; $j++){
        sleep 1 && print ".";
    }
    print "don't fear the reaper, man\n" and sleep 1;
    # yuck. killall is very bad for solaris. 
    system ("killall -9 apprunner");
    system ("echo timeout > timeout.txt");
    exit 0;
    }
}

#
# get_date -- returns a string of the current time and date
#             formatted for your enjoyment.
################################################################
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
    return sprintf "%s %02d/%02d/19%02d  %02d:%02d:%02d", $weekday{$wday},$mon,$mday,$year,$hour,$min,$sec;
}

#
# get_build_date -- returns apprunner's build string,
#                    e.g. '1999-05-18-07'
################################################################
sub get_build_date {
    open (XUL_FILE, "< $apprunner_samples/navigator.xul");
    $BuildNo = "";
    $LineList;
    while (<XUL_FILE>) {
        chop;
        if (/Build ID/) {
            @LineList = split / /;
            $BuildNo = $LineList[5];
        }
    }
    close( XUL_FILE );
    $BuildNo =~ s/[<>]/ /g;
    return $BuildNo;
}


#
# get_url_list -- opens the $url file and loads up all the urls.
################################################################
sub get_url_list {
    local $url = shift || $start_dir . "/url.txt";
    my $i = 0;

    open (URL_FILE, "< $url");
    while (<URL_FILE>) {
        chop;
        $url_list[$i++] = $_;
    }
}


__END__
