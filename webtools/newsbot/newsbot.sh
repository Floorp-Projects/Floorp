#!/bin/ksh
# Run this from cron to re-create newsbot.html if new messages
# have been sent to newsbot.
# Created 16 Mar 1999 by endico@mozilla.org
#
PATH=/opt/local/bin:/opt/cvs-tools/bin:/usr/ucb:$PATH
export PATH

# i had to use ksh because sh doesn't recognize -nt
# find a more portable way to do this that doesn't suck as badly as using find
#
# Run newsbot if new mail has arrived or the code has been updated.
if [ \( /var/mail/newsbot -nt /opt/newsbot/newsbot.html \) -o  \
     \( /opt/newsbot/newsbot.pl -nt /opt/newsbot/newsbot.html \) ]
then
    echo "rebuilding newsbot file"
    /opt/newsbot/newsbot.pl /var/mail/newsbot /opt/newsbot/newsbot.rdf > /opt/newsbot/newsbot.html

    # wrap file and place in /e/doc (the live web site) 
    /opt/newsbot/wrapnews.pl

    # copy raw html file to stage dir so it doesn't disappear the next
    # time the tree is updated.
    cp /opt/newsbot/newsbot.html /e/stage-docs/mozilla-org/html/newsbot/index.html

    # copy the wrappped file into the live web site.
    cp /opt/newsbot/wrapped.html /e/docs/newsbot/index.html

    # copy the rdf file into the live web site.
    cp /opt/newsbot/newsbot.rdf /e/docs/newsbot/newsbot.rdf

fi
