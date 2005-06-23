#!/usr/bin/perl -w

#
# Test wrapper for reportdata.pl, e.g. "Is my apache server
# collecting data?" this is an easy way to just shove in some
# random data to test things out.
#

use strict;
use warnings;

require "reportdata.pl";

{
    ReportData::send_results_to_server("web.java", 250, "--", "foo", "lespaul");

}
