#!/usr/bin/perl

# Perl script to analyze the log output from xpcom and print
# the results in html.
#
# To get the xpcom log output to a file say xpcom.log:
#	setenv NSPR_LOG_MODULES nsComponentManager:5
#	setenv NSPR_LOG_FILE xpcom.log
#	./mozilla
#	<quit>
#
# To get registry (used to figure out progid mappings)
#	./regExport component.reg > registry.txt

# Usage:
#
# a) To get simple output
#    cat xpcom.log | perl xpcom-log-analyze.pl > xpcom-log.html
#
# b) To get all possible cid->progid mappings filled in
#    cat xpcom.log registry.txt | perl xpcom-log-analyze.pl > xpcom-log.html
#
#
# Author: Suresh Duddi <dp@netscape.com>
# Created Aug 9 2000

while (<>)
{
    chomp;
    if ( /ProgIDToClassID.*\}/ )
    {
        # Passed progid to cid mapping. Add progid to cid mapping
        $cid = GetCID();
        $progid = GetProgID();
        $progid_map{$cid} = $progid;
        $progid_passed{$progid}++;
        $nprogid_passed++;
        next;
    }

    if ( /ProgIDToClassID.*FAILED/ )
    {
        # Failed progid. Collect it.
        $progid = GetProgID();
        $progid_failed{$progid}++;
        $nprogid_failed++;
        next;
    }

    if ( /CreateInstance.*succeeded/ )
    {
        # Successful create instance
        $objects{GetCID()}++;
        $nobjects++;
        next;
    }

    if ( /CreateInstance.*FAILED/ )
    {
        # Failed create instance
        $objects_failed{GetCID()}++;
        $nobjects_failed++;
        next;
    }

    if ( /: loading/ )
    {
        $dll = GetDll();
        # make the name a little pretty
        $dll =~ s/^.*bin\///;
        $dll_loaded[@dll_loaded] = $dll;
        next;
    }

    if ( / classID -  \{/ )
    {
        # this is from the output of registry. Try to update progid_map
        $cid = GetCID();
        # Get the next progid or classname line until a empty new line
        $_ = <STDIN> until (/ProgID|ClassName/ || length == 1);
        chomp;
        $progid = $_;
        $progid =~ s/^.*= //;
        $progid_map{$cid} = $progid;
    }
}

PrintHTMLResults();


sub GetProgID() {
    # Get a proid from a line
    my($progid) = $_;
    $progid =~ s/^.*\((.*)\).*$/$1/;
#    print "Got Progid: $progid\n";
    return $progid;
}

sub GetCID() {
    # Get a CID from a line
    my($cid) = $_;
    $cid =~ s/^.*\{(.*)\}.*$/$1/;
#    print "Got cid: $cid\n";
    return $cid;
}

sub GetDll() {
    # Get a Dll from a line
    my($dll) = $_;
    $dll =~ s/^.*\"(.*)\".*$/$1/;
#    print "Got dll: $dll\n";
    return $dll;
}


#
# Print the results of our log analysis in html
#
sub PrintHTMLResults()
{
    $now_time = localtime();
    print "<HTML><HEAD><title>XPCOM Log analysis dated: $now_time\n";
    print "</TITLE></HEAD>\n";
    print "<H1><center>\n";
    print "XPCOM Log analysis dated: $now_time\n";
    print "</center></H1>\n";
# ========================================================================
# Performance analysis
# ========================================================================
    print "<H2>Performance Analysis</H2>\n";

# Number of dlls loaded
    $n = @dll_loaded;
    print "<H3>Dlls Loaded : $n</H3>\n";
    print "<blockquote><pre>\n";
    PrintArray(@dll_loaded);
    print "</blockquote></pre>\n";

# Objects created with a histogram
    print "<H3>Objects created : $nobjects</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%objects);
    foreach $cid (@sorted_key)
    {
        printf("%5d %s [%s]\n", $objects{$cid}, $cid, $progid_map{$cid});
    }
    print "</blockquote></pre>\n";

# Passed progid calls
    print "<H3>Succeeded ProgIDToClassID() : $nprogid_passed</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%progid_passed);
    foreach $progid (@sorted_key)
    {
        printf("%5d %s\n", $progid_passed{$progid}, $progid);
    }
    print "</blockquote></pre>\n";


# ========================================================================
# Error analysis
# ========================================================================
    print "<H2>Error Analysis</H2>\n";

# CreateInstance() FAILED
    print "<H3>Failed CreateInstance() : $nobjects_failed</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%objects_failed);
    foreach $cid (@sorted_key)
    {
        printf("%5d %s [%s]\n", $objects_failed{$cid}, $cid, $progid_map{$cid});
    }
    print "</blockquote></pre>\n";

# ProgIDToClassID() FAILED with a histogram
    print "<H3>Failed ProgIDToClassID() : $nprogid_failed</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%progid_failed);
    foreach $progid (@sorted_key)
    {
        printf("%5d %s\n", $progid_failed{$progid}, $progid);
    }
    print "</blockquote></pre>\n";

# end the html listing
    print "</HTML>\n";
}


sub PrintArray()
{
    my(@array) = @_;
    for ($i=0; $i<@array; $i++)
    {
        print "$array[$i]\n";
    }
}

#
# Sort a given hash by reverse numeric order of value
# return the sorted keylist
#
sub SortKeyByValue()
{
    my(%hash) = @_;
    my(@sorted_keys) = sort { $hash{$b} <=> $hash{$a} } keys %hash;
    return @sorted_keys;
}
