#!/bin/ksh
# Run this from cron to re-create newsbot.html if new messages
# have been sent to newsbot.
# Created 16 Mar 1999 by endico@mozilla.org
#
# i had to use ksh because sh doesn't recognize -nt
# find a more portable way to do this that doesn't suck as badly as using find

PATH=/opt/gnu/bin:/opt/tools/bin:/usr/ucb:$PATH
export PATH

CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
export CVSROOT

# update the newsbot sources
cvs -q -d $CVSROOT update -dP


# Run newsbot if new mail has arrived or the code has been updated.
if [ \( /var/mail/newsbot -nt /opt/newsbot/newsbot.html \) -o  \
     \( /opt/newsbot/newsbot.pl -nt /opt/newsbot/newsbot.html \) ]
then
    echo "rebuilding newsbot file"
    /opt/newsbot/newsbot.pl /var/mail/newsbot /opt/newsbot/newsbot.rdf.tmp > /opt/newsbot/newsbot.html

    # copy the rdf file into file used by web site
    cp /opt/newsbot/newsbot.rdf.tmp /opt/newsbot/newsbot.rdf

fi

# Do this separately in case wrapnews failed the last time this script ran. 
# Sometimes it fails because this script runs at the same time the staging
# area is being rebuilt and the wrapper scripts are missing.
if [ \( /opt/newsbot/newsbot.html -nt /opt/newsbot/index.html \) -a \
     \( -e "/e/stage-docs/mozilla-org/tools/wrap.pl" \) -a \
     \( -e "/e/stage-docs/mozilla-org/html/template.html" \) ]
then
    echo "wrapping newsbot file"
    # wrap file and place in newsbot directory
    /opt/newsbot/wrapnews.pl

    # copy the wrappped file into file used by web site.
    cp /opt/newsbot/wrapped.html /opt/newsbot/index.html
fi
