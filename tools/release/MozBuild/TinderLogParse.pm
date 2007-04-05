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
#  Got Build ID: 2007030311
# Only the buildID, '2007030311', is to be returned.
#
##

sub GetBuildID {
    my $this = shift;
    my %args = @_;

    my $log = $this->GetLogFileName();

    my $buildID = undef;
    my $searchString = "Got build ID ";

    open (FILE, "< $log") or die("Cannot open file $log: $!");
    while (<FILE>) {
        if ($_ =~ /$searchString/) {
            $buildID = $_;
 
            # remove search string
            $buildID =~ s/$searchString//;

            # remove trailing slash
            $buildID =~ s/^\.//;
            last;
        }
    }

    close FILE or die("Cannot close file $log: $!");

    return $buildID;
}

sub GetLogFileName {
    my $this = shift;
    my %args = @_;

    return $this->{'fileName'};
}
