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
# To get registry (used to figure out contractid mappings)
#	./regExport component.reg > registry.txt

# Usage:
#
# a) To get simple output
#    cat xpcom.log | perl xpcom-log-analyze.pl > xpcom-log.html
#
# b) To get all possible cid->contractid mappings filled in
#    cat xpcom.log registry.txt | perl xpcom-log-analyze.pl > xpcom-log.html
#
#
# Author: Suresh Duddi <dp@netscape.com>
# Created Aug 9 2000

while (<>)
{
    chomp;
    if ( /ContractIDToClassID.*\}/ )
    {
        # Passed contractid to cid mapping. Add contractid to cid mapping
        $cid = GetCID();
        $contractid = GetContractID();
        $contractid_map{$cid} = $contractid;
        $contractid_passed{$contractid}++;
        $ncontractid_passed++;
        next;
    }

    if ( /ContractIDToClassID.*FAILED/ )
    {
        # Failed contractid. Collect it.
        $contractid = GetContractID();
        $contractid_failed{$contractid}++;
        $ncontractid_failed++;
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
        # this is from the output of registry. Try to update contractid_map
        $cid = GetCID();
        # Get the next contractid or classname line until a empty new line
        $_ = <STDIN> until (/ContractID|ClassName/ || length == 1);
        chomp;
        $contractid = $_;
        $contractid =~ s/^.*= //;
        $contractid_map{$cid} = $contractid;
    }
}

PrintHTMLResults();


sub GetContractID() {
    # Get a proid from a line
    my($contractid) = $_;
    $contractid =~ s/^.*\((.*)\).*$/$1/;
#    print "Got Progid: $contractid\n";
    return $contractid;
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
        printf("%5d %s [%s]\n", $objects{$cid}, $cid, $contractid_map{$cid});
    }
    print "</blockquote></pre>\n";

# Passed contractid calls
    print "<H3>Succeeded ContractIDToClassID() : $ncontractid_passed</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%contractid_passed);
    foreach $contractid (@sorted_key)
    {
        printf("%5d %s\n", $contractid_passed{$contractid}, $contractid);
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
        printf("%5d %s [%s]\n", $objects_failed{$cid}, $cid, $contractid_map{$cid});
    }
    print "</blockquote></pre>\n";

# ContractIDToClassID() FAILED with a histogram
    print "<H3>Failed ContractIDToClassID() : $ncontractid_failed</H3>\n";
    print "<blockquote><pre>\n";
    @sorted_key = SortKeyByValue(%contractid_failed);
    foreach $contractid (@sorted_key)
    {
        printf("%5d %s\n", $contractid_failed{$contractid}, $contractid);
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
