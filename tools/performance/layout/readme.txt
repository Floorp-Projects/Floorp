readme.txt - if there is a perf.html file, read that, it contains the full documentation. This file is a stop-gap until the documentation is completed.

The cached websites must be downloaded and installed under the
performance\layout directory. You can find them at
http://techno/users/attinasi/publish and click on the WebSites in ZIP format.
Unzip the archive and make sure the websites extracted correctly.

BASICS: to run the performance tool simply execute the perf.pl script.
 eg. perl perf.pl Daily-021400 s:\mozilla\dist\win32_o.obj CPU

This command will run viewer, crawl the top 40 URLs, dump the results to a file and subsequently parse the file to create the HTML performance table.

To run Mozilla instead of viewer, edit the perf.pl script and change the $UseViewer variable value to 0. Then, run the perf.pl script, except there is an additional argument:
 perl perf.pl Daily-021400 s:\mozilla\dist\win32_o.obj CPU profilename

Including the profilename is essential, and the profilename specified must reference a valid profile.

The output from the scripts is:

1) BUILD_NAME.html in the Tables directory - the performance table
2) BUILD_NAME-TrendTable.html in the Tables directory - the trending table
3) BUILD_NAME directory under Logs - includes all of the original log files
4) history.txt - appends the latest averages to the history file

If a run is aborted or is not to be used, the history.txt file must be edited to remove the bogus run.

If problems arise in the scripts, there are debug routeins in each script file. Simply remove the comment from the print line and re-run to get diagnostics dumped to the console.



... more to come...

