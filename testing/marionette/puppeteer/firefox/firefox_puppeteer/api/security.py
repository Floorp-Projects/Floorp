# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import re

from firefox_puppeteer.base import BaseLib
from firefox_puppeteer.errors import NoCertificateError


class Security(BaseLib):
    """Low-level access to security (SSL, TLS) related information."""

    # Security related helpers #

    def get_address_from_certificate(self, certificate):
        """Retrieves the address of the organization from the certificate information.

        The returned address may be `None` in case of no address found, or a dictionary
        with the following entries: `city`, `country`, `postal_code`, `state`, `street`.

        :param certificate: A JSON object representing the certificate, which can usually be
         retrieved via the current tab: `self.browser.tabbar.selected_tab.certificate`.

        :returns: Address details as dictionary
        """
        regex = re.compile('.*?L=(?P<city>.+?),ST=(?P<state>.+?),C=(?P<country>.+?)'
                           ',postalCode=(?P<postal_code>.+?),STREET=(?P<street>.+?)'
                           ',serial')
        results = regex.search(certificate['subjectName'])

        return results.groupdict() if results else results

    def get_certificate_for_page(self, tab_element):
        """The SSL certificate assiciated with the loaded web page in the given tab.

        :param tab_element: The inner tab DOM element.

        :returns: Certificate details as JSON object.
        """
        cert = self.marionette.execute_script("""
          var securityUI = arguments[0].linkedBrowser.securityUI;
          var status = securityUI.secInfo.SSLStatus;

          return status ? status.serverCert : null;
        """, script_args=[tab_element])

        uri = self.marionette.execute_script("""
          return arguments[0].linkedBrowser.currentURI.spec;
        """, script_args=[tab_element])

        if cert is None:
            raise NoCertificateError('No certificate found for "{}"'.format(uri))

        return cert

    def get_domain_from_common_name(self, common_name):
        """Retrieves the domain associated with a page's security certificate from the common name.

        :param certificate: A string containing the certificate's common name, which can usually
         be retrieved like so: `certificate['commonName']`.

        :returns: Domain as string
        """
        return self.marionette.execute_script("""
          return Services.eTLD.getBaseDomainFromHost(arguments[0]);
        """, script_args=[common_name])
