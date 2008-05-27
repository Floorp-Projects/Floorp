##
# TinderLogParse - A tinderbox log parser
##

package MozBuild::TinderLogParse;

sub new {
    my $class = shift;
    my %args = @_;

    my $logFile = $args{'logFile'};
    if (! defined($logFile)) {
        die("ASSERT: TinderLogParse::new logFile is a required argument");
    }

    my $this = { logFile => $logFile };
    bless($this, $class);
    return $this;
}

## 
# GetBuildID - attempts to find build ID in a tinderbox log file
#
# Searches for a string of the form:
#  buildid: 2007030311
# Only the buildID, '2007030311', is to be returned.
#
##

sub GetBuildID {
    my $this = shift;
    my %args = @_;

    my $log = $this->GetLogFileName();

    my $buildID = undef;
    my $searchString = "buildid: ";

    open (FILE, "< $log") or die("Cannot open file $log: $!");
    while (<FILE>) {
        if ($_ =~ /$searchString/) {
            $buildID = $_;
 
            # remove search string
            $buildID =~ s/$searchString//;

            # remove trailing slash
            $buildID =~ s/\.$//;
            last;
        }
    }

    close FILE or die("Cannot close file $log: $!");

    if (! defined($buildID)) {
        return undef;
    }
    if (! $buildID =~ /^\d+$/) {
        die("ASSERT: Build: build ID is not numerical: $buildID")
    }

    chomp($buildID);
    return $buildID;
}

## 
# GetPushDir - attempts to find the directory Tinderbox pushed to build to
#
# Searches for a string of the form:
#  Linux Firefox Build available at: 
#  localhost/2007-04-07-18-firefox2.0.0.4/ 
# Matches on "Build available at:" and returns the next line.
##

sub GetPushDir {
    my $this = shift;
    my %args = @_;

    my $log = $this->GetLogFileName();

    my $found = 0;
    my $pushDir = undef;
    my $searchString = "Build available at:";

    open (FILE, "< $log") or die("Cannot open file $log: $!");
    while (<FILE>) {
        if ($found) {
            $pushDir = $_;
            last;
        }
        if ($_ =~ /$searchString/) {
            $found = 1;
        }
    }

    close FILE or die("Cannot close file $log: $!");
 
    if (! defined($pushDir)) {
        return undef;
    }

    chomp($pushDir);
    # remove empty space at end of URL
    $pushDir =~ s/\s+$//;

    return $pushDir;
}

sub GetLogFileName {
    my $this = shift;
    my %args = @_;

    return $this->{'logFile'};
}

1;
