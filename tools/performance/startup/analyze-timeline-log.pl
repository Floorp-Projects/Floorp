#
# perl file to get a list of files that we load from a timeline log
#
# Usage: perl getlines.pl < timline.log > file.log

# Configuration options
#
# Set this option to take all timings only upto the main window being visible
# All activity beyond main window being visible is ignored.
# default is 0. We take timings upto main1 finish
$stopWithVisibleWindow = 0;

# dlls loaded
my @dlls = ();
# Files of a particular extensions
my %ext = ();
# Number of files in a paticular extension. Used to sort them output by order of extension
my %extnum = ();
# List of all urls that either exist or are jar. Non dups.
my @allurls = ();
# List of nonexistent file urls
my @nonexistentfiles = ();
# Number of file urls
my $nfileurl = 0;
# Number of jar urls
my $njarurl = 0;
# Urls hash to figure out dups
my %seen = ();
# Number of dups per extension
my %extnumdups = ();
# interesting total times
my %breakdown = ();
# main1 cost
my $main1 = 0;
# when main window was visible
my $window = 0;
# pref files loaded
my @prefurl = ();
while (<>)
{
    chomp;

    # Computer breakdown of startup cost
    if (/total:/) {
        # This is a cumulative timer. Keep track of it.
        /^[0-9\.: ]*(.*) total: ([0-9\.]*)/;
        $breakdown{$1} = $2;
    }
    if (/InitXPCom\.\.\./) {
        $breakdown{"Before InitXPCom"} = getTimelineStamp($_);
        next;
    }
    if (/InitXPCOM done/) {
        $breakdown{"InitXPCom"} = getTimelineStamp($_) - $breakdown{"Before InitXPCom"};
        next;
    }

    # Find main1's cost to compute percentages
    if (/\.\.\.main1/) {
        $main1 = getTimelineStamp($_);
    }
    # find when we showed the window
    if (/Navigator Window visible now/) {
        $window = getTimelineStamp($_);
        if ($stopWithVisibleWindow) {
            $main1 = $window;
            last;
        }
    }

    # Find all files loaded
    if (/PR_LoadLibrary/) {
        my $dll = getdll($_);
        push @dlls, $dll;
        next;
    }
    if (/file:/) {
        $url = getfileurl($_);
        $e = getext($url);
        if (!-f $url) {
            push @nonexistentfiles, $url;
        } else {
            $seen{$url}++;
            if ($seen{$url} > 1) {
                $extnumdups{$e}++;
                next;
            }
            push @allurls, $url;
            if (exists $ext{$e}) {
                $ext{$e} .= "---";
            }
            $ext{$e} .= $url;
            $extnum{$e}++;
            $nfileurl++;
        }
        next;
    }
    if (/jar:/) {
        $url = getjarurl($_);
        $e = getext($url);
        $seen{$url}++;
        if ($seen{$url} > 1) {
            $extnumdups{$e}++;
            next;
        }
        push @allurls, $url;
        if (exists $ext{$e}) {
            $ext{$e} .= "---";
        }
        $ext{$e} .= "$url";
        $extnum{$e}++;
        $njarurl++;
        next;
    }
    if (/load pref file/) {
        $url = getfile($_);
        push @prefurl, $url;
    }
}

# print results
print "SUMMARY\n";
print "----------------------------------------------------------------------\n";
print  "Total startup time : $main1 sec\n";
printf "Main window visible: $window sec (%5.2f%%)\n", main1Percent($window);
print "dlls loaded : ", $#dlls+1, "\n";
print "Total unique: ", $#allurls+1, " [jar $njarurl, file $nfileurl]\n";
# print the # of files by extensions sorted
my @extsorted = sort { $extnum{$b} <=> $extnum{$a} } keys %extnum;
my $sep = "              ";
foreach $i (@extsorted)
{
    print "$sep.$i $extnum{$i}";
    $sep = ", ";
}
print "\n";
# print number of dups per extension
my $sep = "        dups: ";
foreach $i (@extsorted)
{
    next unless exists($extnumdups{$i});
    print "$sep.$i $extnumdups{$i}";
    $sep = ", ";
}
print "\n";
print "Total non existent files : ", $#nonexistentfiles+1, "\n";

print "\n";
print "Cost Breakdown\n";
print "----------------------------------------------------------------------\n";
# sort by descending order of breakdown cost
my @breakdownsorted = sort { $breakdown{$b} <=> $breakdown{$a} } keys %breakdown;
my $totalAccounted = 0;
foreach $e (@breakdownsorted)
{
    # ignore these breakdowns as they are already counted otherwise
    next if ($e =~ /nsNativeComponentLoader::GetFactory/);
    my $p = main1Percent($breakdown{$e});
    #next if ($p == 0);
    printf "%6.2f%% %s\n", $p, $e;
    $totalAccounted += $p;
}
print "----------------------\n";
printf "%6.2f%% Total Accounted\n", $totalAccounted;

print "\n";
printf "[%d] List of dlls loaded:\n", $#dlls+1;
print "----------------------------------------------------------------------\n";
my $n = 1;
foreach $e (@dlls)
{
    printf "%2d. %s\n", $n++, $e;
}

print "\n";
printf "[%d] List of existing unique files and jar urls by extension:\n", $#allurls+1;
print "----------------------------------------------------------------------\n";
foreach $e (@extsorted)
{
    $n = 1;
    print "[$extnum{$e}] .$e\n";
    foreach $i (split("---", $ext{$e})) {
        printf "%2d. %s", $n++,  $i;
        if ($seen{$i} > 1) {
            printf " [%d]", $seen{$i}-1;
        }
        printf "\n";
    }
    print "\n";
}
#foreach $i (@allurls)
#{
#    printf "%2d. %s\n", $n++,  $i;
#}

print "\n";
printf "[%d] List of non existent file urls:\n", $#nonexistentfiles+1;
print "----------------------------------------------------------------------\n";
my $n = 1;
foreach $i (@nonexistentfiles)
{
    printf "%2d. %s\n", $n++,  $i;
}

# print prefs loaded
print "\n";
printf "[%d] List of pref files loaded:\n", $#prefurl+1;
print "----------------------------------------------------------------------\n";
$n = 1;
foreach $i (@prefurl)
{
    printf "%2d. %s\n", $n++,  $i;
}

# Subrouties
sub getTimelineStamp() {
    my $line = shift;
    $line =~ /^([0-9\.]*)/;
    return $1+0;
}

sub getfileurl() {
    my $f = shift;
    $f =~ s/^.*file:\/*(.*)\).*$/\1/;
    # unescape the url
    $f =~ s/\|/:/;
    $f =~ s/\%20/ /g;
    return $f;
}

sub getjarurl() {
    my $f = shift;
    $f =~ s/^.*(jar:.*)\).*$/\1/;
    return $f;
}

sub getdll() {
    my $f = shift;
    $f =~ s/^.*\((.*)\).*$/\1/;
    return $f;
}

sub getext() {
    my $f = shift;
    $f =~ s/^.*\.([^\.]*)$/\1/;
    return $f;
}

sub getfile() {
    my $f = shift;
    $f =~ s/^.*\((.*)\)$/\1/;
    return $f;
}

# what % is this of startup
sub main1Percent() {
    my $i = shift;
    return $i/$main1 * 100;
}
