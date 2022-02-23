.. _mozilla_projects_nss_notes_on_tls_-_ssl_3_0_intolerant_servers:

Notes on TLS - SSL 3.0 Intolerant Servers
=========================================

`Problem <#problem>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A number of Netscape 6.x/7.x and Mozilla users have reported that some secure sites -- typically
   sites featuring online transactions or online banking over the HTTPS protocol -- do not display
   any content at all. The connection seems terminated and a blank page is displayed. This is the
   main symptom of the problem when Mozilla based browsers encounter TLS/SSL 3.0 intolerant servers.

`Cause <#cause>`__
~~~~~~~~~~~~~~~~~~

.. container::

   There are some number of web servers in production today which incorrectly implement the SSL 3.0
   specification. This incorrect implementation causes them to reject connection attempts from
   clients that are compliant with the **SSL 3.0** and **TLS (aka SSL 3.1)** specifications.

   Netscape 6.x/7.x and Mozilla browsers (0.9.1 and later versions) correctly implement the TLS
   specification, and the users cannot utilize sites which have this problem.

.. _technical_information:

`Technical Information <#technical_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The **SSL 3.0** and **TLS (aka SSL 3.1)** specs both contain a provision -- the same provision --
   for detecting "version rollback attacks". It is designed to permit a server to detect a
   man-in-the-middle that is altering the SSL client hello (connection) requests as they pass from
   the client to the server, altering them by changing the protocol version number to a lower
   version number. This feature was kind of meaningless until **TLS (SSL 3.1)** came along because
   there was no version higher than 3.0 from which to be rolled back. TLS is now available and used,
   and products that have implemented the roll-back detection incorrectly are not interoperable with
   TLS/SSL spec-compliant clients. Normally the servers which have this problem are not equipped to
   deal with the TLS protocol, but instead of rolling back to SSL 3.0 as the rollback provision
   provides, they terminate/drop the connection, thus resulting in most cases a blank page display.

   For up-to-date information, you can read a Bugzilla bug report which keeps track of this problem
   with Mozilla-based browsers. See
   `bug 59321 <https://bugzilla.mozilla.org/show_bug.cgi?id=59321>`__.

.. _servers_currently_known_to_exhibit_this_intolerant_behavior:

`Servers currently known to exhibit this intolerant behavior <#servers_currently_known_to_exhibit_this_intolerant_behavior>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   As of this writing, this problem has been reported for the following servers: (Wherever there is
   an upgraded version which fixes the problem, it is indicated by an asterisked remark in the
   parentheses. )

   -  Domino-Go-Webserver/4.6.2.6 (and perhaps some later versions)
   -  IBM_HTTP_Server/1.3.6.3 or earlier (\* Update to 1.3.6.4)
   -  IBM_HTTP_Server/1.3.12.1 or earlier (\* `Update to
      1.3.12.2 <http://www6.software.ibm.com/dl/websphere/http-p>`__)
   -  Java Web Server 2
   -  OSU/3.2 - DECthreads HTTP server for OpenVM
   -  Stronghold/2.2
   -  Webmail v. 3.6.1 by Infinite Technologies (\* Update available)

   N.B. There might be servers other than those listed above which exhibit this problem. If you find
   such a server, feel free to add it to this page. For up-to-date information, you can read this
   `bug 59321 <https://bugzilla.mozilla.org/show_bug.cgi?id=59321>`__ which keeps a list of TLS/SSL
   3.0 intolerant servers.

.. _users:_how_to_avoid_this_problem.3f:

`Users: How to avoid this problem? <#users:_how_to_avoid_this_problem.3f>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Netscape 6.1 Preview Release 1, or Mozilla 0.9.1 and earlier
      :name: netscape_6.1_preview_release_1.2c_or_mozilla_0.9.1_and_earlier

   These versions shipped with the TLS option turned **ON** as the default but with no way to deal
   with the problem servers. If you are using these old versions, please update to the latest
   Netscape or Mozilla versions. You can also avoid such a problem by editing an existing profile --
   check the preference option setting at: Edit \| Preferences \| Privacy and Security \| SSL \|
   Enable TLS, and turn it **OFF** if it is **ON** for these earlier browsers.

   .. rubric:: Netscape 6.1 or Mozilla 0.9.2 and later
      :name: netscape_6.1_or_mozilla_0.9.2_and_later

   These browsers shipped with the TLS option **ON** but also included a graceful rollback mechanism
   on the client side when they encounter known TLS/SSL 3.0 intolerant servers.

   .. rubric:: Firefox 2 and later
      :name: firefox_2_and_later

   Starting with Firefox 2, support for SSL 2.0 has been disabled by default; unless it is expressly
   re-enabled by the user using about:config. See `Security in Firefox
   2 <https://developer.mozilla.org/en-US/docs/Mozilla/Firefox/Releases/2/Security_changes>`__ for
   details.

.. _website_administrators:_how_to_avoid_this_problem.3f:

`Website Administrators: How to avoid this problem? <#website_administrators:_how_to_avoid_this_problem.3f>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Upgrade to a newer version if available, which corrects this problem. There will be other
      network clients which implement TLS/SSL 3.0 specification correctly and have a problem with
      your site. Let's not perpetuate the problem servers.
   -  Contact the manufacturer and inquire if there is a new version available which fixes this
      problem.
   -  Post a note on your site instructing users of old versions of browsers like Netscape
      6.0/6.01/6.1 Preview Release 1 and Mozilla 0.9.1 and earlier to turn **OFF** the TLS option
      at: Edit \| Preferences \| Privacy and Security \| SSL \| Enable TLS. However, this is a
      temporary workaround at best. We recommend strongly that you urge users to upgrade to the
      latest Netscape version (or at least Netscape 6.1) since the newer versions have the graceful
      rollback implemented in the code.
   -  If you have questions concerning Netscape browsers and problem servers, please contact us
      using the feedback address at the top of this page.

.. _detecting_intolerant_servers:

`Detecting intolerant servers <#detecting_intolerant_servers>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Because newer versions of Netscape and Mozilla have built-in workaround for the problem servers,
   it is now unlikely that you will experience this problem. But if you're running Netscape
   6.0/6.01/6.1 PR 1 or Mozilla build (prior to 6/11/2001), you should look out for the symptom
   described below. You may also run this test with versions later than the older versions of
   Netscape 6.x or Mozilla -- just in case code changes in Netscape 6.1/Mozilla 0.9.2 or later may
   not catch all problem servers.

   -  When you find a secure site which simply does not display any page content or drops the
      connection, check to see if the preference option Edit \| Preferences \| Privacy and Security
      \| SSL \| Enable TLS is turned **ON**. If so, turn it **OFF**.
   -  Now re-visit the problem site. If the content displays this time, you are most likely
      witnessing a TLS/SSL 3.0 intolerant server.
   -  Report such a problem to the server's administrator.

.. _how_to_report_an_intolerant_server:

`How to report an intolerant server <#how_to_report_an_intolerant_server>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  *Optional:*\ Get the name of the SSL server software used by the website. To do so, add
      ``http://toolbar.netcraft.com/site_report?url=`` to the beginning of the URL. The server
      software will appear next to **Server** under **SSL Certificate Information**.
      For instance, to check ``https://bugzilla.mozilla.org/``, then visit
      `http://toolbar.netcraft.com/site_rep...a.mozilla.org/ <http://toolbar.netcraft.com/site_report?url=https://bugzilla.mozilla.org/>`__.
   -  Add the information on such a server (software, URL) to
      `bug 59321 <https://bugzilla.mozilla.org/show_bug.cgi?id=59321>`__ at Bugzilla. (Note: You
      will be asked to provide your email address before you can file a bug at Bugzilla.)

.. _original_document_information:

`Original Document Information <#original_document_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Author : Katsuhiko Momoi
   -  Last Updated Date: January 27th, 2003
   -  Copyright © 2001-2003 Netscape. All rights reserved.