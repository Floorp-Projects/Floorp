Release process
===============

#. Set the NSPR version number. Five files need to be updated:
   configure.in, configure, repackage.sh, prinit.h, and vercheck.c. Look
   at their CVS histories for
   `examples <http://bonsai.mozilla.org/cvsquery.cgi?module=SecurityServices&date=explicit&mindate=1278542334&maxdate=1278548483>`__.
#. Make a dummy change (add or remove a blank line) to
   mozilla/nsprpub/config/prdepend.h.
#. Make sure the `NSS
   tinderboxes <http://tinderbox.mozilla.org/showbuilds.cgi?tree=NSS>`__
   (which also build and test NSPR) are all green. The build+test cycles
   of the NSS tinderboxes are very long, so you usually need to wait
   half a day for them to cycle through.
#. Create a BETA CVS tag. The naming convention is NSPR_x_y_z_BETAn for
   NSPR x.y.z Beta n.
#. `Push the BETA tag to
   mozilla-central </en/Updating_NSPR_or_NSS_in_mozilla-central>`__ for
   testing in Firefox trunk builds. Ideally you should test it for a
   week.
#. Remove "Beta" from the NSPR version number for the final release.
    Usually you just need to update prinit.h.
#. Make a dummy change (add or remove a blank line) to
   mozilla/nsprpub/config/prdepend.h.
#. Create a RTM CVS tag. The naming convention is NSPR_x_y_z_RTM for
   NSPR x.y.z.
#. Upload a source tar file to
   `https://ftp.mozilla.org/pub/mozilla....nspr/releases/ <https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/>`__
#. Write the release notes.
#. Announce the release in the Mozilla NSPR newsgroup.
