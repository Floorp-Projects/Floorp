Mozilla Build Verification Scripts
---

--
Contents
--

updates -> AUS and update verification
l10n    -> l10n vs. en-US verification
common  -> useful utility scripts

--
Update verification
--

verify.sh
  does a low-level check of all advertised MAR files. Expects to have a
  file named all-locales, but does not (yet) handle platform exceptions, so 
  these should be removed from the locales file.

  Does the following:

  1) download update.xml from AUS for a particular release
  2) download the partial and full mar advertised
  3) check that the partial and full match the advertised size and sha1sum
  4) downloads the latest release, and an older release
  5) applies MAR to the older release, and compares the two releases.
  
  Step 5 is repeated for both the complete and partial MAR.

  Expects to have an updates.cfg file, describing all releases to try updating 
  from.

-
Valid Platforms for AUS
-
Linux_x86-gcc3
Darwin_Universal-gcc3
Linux_x86-gcc3
WINNT_x86-msvc
Darwin_ppc-gcc3

--
l10n verification
--

verify_l10n.sh
  unpacks an en-US build for a particular release/platform, and
  then unpacks and compares all locales for that particular release/platform.

  Expects to have a file named all-locales, but does not (yet) handle platform 
  exceptions, so these should be removed from the locales file.

  Best practice is to take a directory full of diffs for a particular release
  and compare to a directory full of diffs for the current release, to see
  what l10n changes have occurred. For maintenance releases, this should
  be slim to none.

