# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla page-loader test, released Aug 5, 2001
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2001 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    John Morrison <jrgm@netscape.com>, original author
#    

Rough notes on setting up this test app. jrgm@netscape.com 2001/08/05

1) this is intended to be run as a mod_perl application under an Apache web
   server. [It is possible to run it as a cgi-bin, but then you will be paying
   the cost of forking perl and re-compiling all the required modules on each
   page load].

2) it should be possible to run this under Apache on win32, but I expect that
   there are *nix-oriented assumptions that have crept in. (You would also need
   a replacement for Time::HiRes, probably by using Win32::API to directly
   call into the system to Windows 'GetLocalTime()'.)

3) You need to have a few "non-standard" Perl Modules installed. This script
   will tell you which ones are not installed (let me know if I have left some
   out of this test).

--8<--------------------------------------------------------------------
#!/usr/bin/perl 
my @modules = qw{
    LWP::UserAgent   SQL::Statement     Text::CSV_XS      DBD::CSV          
    DBI              Time::HiRes        CGI::Request      URI               
    MIME::Base64     HTML::Parser       HTML::Tagset      Digest::MD5      
    };
for (@modules) {
    printf "%20s", $_;
    eval "use $_;";
    if ($@) {
        print ", I don't have that.\n";
    } else {
        print ", version: ", eval "\$" . "$_" . "::VERSION", "\n";
    }
}
--8<--------------------------------------------------------------------

   For modules that are missing, you can find them at http://www.cpan.org/.
   Download the .tar.gz files you need, and then (for the most part) just 
   do 'perl Makefile.PL; make; make test; make install'.

   [Update: 28-Mar-2003] I recently installed Redhat 7.2, as server, which
   installed Apache 1.3.20 with mod_perl 1.24 and perl 5.6.0. I then ran the
   CPAN shell (`perl -MCPAN -e shell') and after completing configuration, I
   did 'install Bundle::CPAN', 'install Bundle::LWP' and 'install DBI' to
   upgrade tose modules and their dependencies.

   CGI::Request seems to have disappeared from CPAN, but you can get a copy
   from <http://stein.cshl.org/WWW/software/CGI::modules/> and then install
   with the standard `perl Makefile.PL; make; make test; make install'.

   To install the SQL::Statement, Text::CSV_XS, and DBD::CSV modules, there is
   a bundle available on CPAN, so you can use the CPAN shell and just enter
   'install Bundle::DBD::CSV'.

   At the end of this, the output for the test program above was the
   following.  (Note: you don't necessarily have to have the exact version
   numbers for these modules, as far as I know, but something close would be
   safest).

      LWP::UserAgent, version: 2.003
      SQL::Statement, version: 1.005
        Text::CSV_XS, version: 0.23
            DBD::CSV, version: 0.2002
                 DBI, version: 1.35
         Time::HiRes, version: 1.43
        CGI::Request, version: 2.75
                 URI, version: 1.23
        MIME::Base64, version: 2.18
        HTML::Parser, version: 3.27
        HTML::Tagset, version: 3.03
         Digest::MD5, version: 2.24

   I've also heard that installing with Apache 2.x requires a few changes,
   although I don't know what they are. If you know, tell jrgm@netscape.com.

4) There is code to draw a sorted graph of the final results, but I have
   disabled the place in 'report.pl' where its use would be triggered (look
   for the comment). This is so that you can run this without having gone
   through the additional setup of the 'gd' library, and the modules GD and
   GD::Graph. If you have those in place, you can turn this on by just
   reenabling the print statement in report.pl

   [Note - 28-Mar-2003: with Redhat 7.2, libgd.so.1.8.4 is preinstalled to
   /usr/lib. The current GD.pm modules require libgd 2.0.5 or higher, but you
   use 1.8.4 if you install GD.pm version 1.40, which is available at
   <http://stein.cshl.org/WWW/software/GD/old/GD-1.40.tar.gz>. Just do 'perl
   Makefile.PL; make; make install' as usual. I chose to build with JPEG
   support, but without FreeType, XPM and GIF support. I had a test error when
   running 'make test', but it works fine for my purposes. I then installed
   'GD::Text' and 'GD::Graph' from the CPAN shell.]

5) To set this up with Apache, create a directory in the cgi-bin for the web
   server called e.g. 'page-loader' and then place this in the Apache
   httpd.conf file to enable this for mod_perl (and then restart Apache).

--8<--------------------------------------------------------------------
Alias /page-loader/  /var/www/cgi-bin/page-loader/
<Location /page-loader>
SetHandler  perl-script
PerlHandler Apache::Registry
PerlSendHeader On
Options +ExecCGI
</Location>
--8<--------------------------------------------------------------------

   So, now you can run this as 'http://yourserver.domain.com/page-loader/loader.pl'

6) You need to create a subdirectory call 'db' under the 'page-loader'
   directory. This subdirectory 'db' must be writeable by UID that Apache
   executes as (e.g., 'nobody' or 'apache'). [You may want to figure out some
   other way to do this if this web server is not behind a firewall].

7) You need to assemble a set of content pages, with all images, included JS
   and CSS pulled to the same directory. These pages can live anywhere on the
   same HTTP server that is running this app. The app assumes that each page
   is in its own sub-directory, with included content below that
   directory. You can set the location and the list of pages in the file
   'urllist.txt'. [See 'urllist.txt' for further details on what needs to be
   set there.]

   There are various tools that will pull in complete copies of web pages
   (e.g. 'wget' or something handrolled from LWP::UserAgent). You should edit
   the pages to remove any redirects, popup windows, and possibly any platform
   specific JS rules (e.g., Mac specific CSS included with
   'document.write("LINK...'). You should also check that for missing content,
   or URLs that did not get changed to point to the local content. [One way to
   check for this is tweak this simple proxy server to check your links:
   http://www.stonehenge.com/merlyn/WebTechniques/col34.listing.txt)

8) The "hook" into the content is a single line in each top-level document like this:
      <!-- MOZ_INSERT_CONTENT_HOOK -->
   which should be placed immediately after the opening <HEAD> element. The script uses
   this as the way to substitute a BASE HREF and some JS into the page which will control
   the exectution of the test.

9) I've probably left some stuff out. Bug jrgm@netscape.com for the missing stuff.
