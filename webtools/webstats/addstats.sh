#!/bin/sh

# steal code from http log cruncher for ftp.mozilla.org.  yes, its hacky 
# but it works
#
# awk '{print $4,$7,$9}' # Grab the date, url and http status code
# grep 200$              # Only use the status code 200 (complete downloads)
# awk '{print $1}'       # Remove http status code, leaving only the url
# sed "s/\?.*//"         # Remove query strings
# these should be equivalent ('...blah/index.html' == '...blah/' == '...blah')
# sed "s/index.html//"   # Remove trailing index.html so these entries group
                         # with urls without
# sed "s/\(\w\)\/$/\1/"  # Remove ending slash on directories so they group
                         # together. Not all directories have them. But don't
                         # remove a lone slash. (the root url)


cat  $* |   awk '{print $4, $7, $9}' | grep 200$ | awk '{print $1, $2}'  | sed "s/index.html//" | sed "s/\?.*//" | sed "s/\(\w\)\/$/\1/" | ./addstats.pl
