#
# perl file to get a list of files that we load from a timeline log
#
# Usage: perl getlines.pl < timline.log > file.log

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
while (<>)
{
    chomp;
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
}

# print results
print "SUMMARY\n";
print "----------------------------------------------------------------------\n";
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
print "List of existing unique files and jar urls by extension:\n";
print "----------------------------------------------------------------------\n";
foreach $e (@extsorted)
{
    my $n = 1;
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
print "List of non existent file urls:\n";
print "----------------------------------------------------------------------\n";
my $n = 1;
foreach $i (@nonexistentfiles)
{
    printf "%2d. %s\n", $n++,  $i;
}

# Subrouties
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

sub getext() {
    my $f = shift;
    $f =~ s/^.*\.([^\.]*)$/\1/;
    return $f;
}
