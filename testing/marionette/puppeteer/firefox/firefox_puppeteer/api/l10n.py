# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# -----------------
# DEPRECATED module
# -----------------
# Replace its use in tests when Firefox 45 ESR support ends with
# marionette_driver.localization.L10n

from __future__ import absolute_import

import copy

from marionette_driver.errors import (
    NoSuchElementException,
    UnknownCommandException,
)
from marionette_driver.localization import L10n as L10nMarionette

from firefox_puppeteer.base import BaseLib


class L10n(BaseLib):
    """An API which allows Marionette to handle localized content.

    .. deprecated:: 52.2.0
       Use the localization module from :py:mod:`marionette_driver` instead.

    The `localization`_ of UI elements in Gecko based applications is done via
    entities and properties. For static values entities are used, which are located
    in .dtd files. Whereby for dynamically updated content the values come from
    .property files. Both types of elements can be identifed via a unique id,
    and the translated content retrieved.

    .. _localization: https://mzl.la/2eUMjyF
    """

    def __init__(self, marionette):
        super(L10n, self).__init__(marionette)

        self._l10nMarionette = L10nMarionette(self.marionette)

    def localize_entity(self, dtd_urls, entity_id):
        """Returns the localized string for the specified DTD entity id.

        To find the entity all given DTD files will be searched for the id.

        :param dtd_urls: A list of dtd files to search.
        :param entity_id: The id to retrieve the value from.

        :returns: The localized string for the requested entity.

        :raises NoSuchElementException: When entity id is not found in dtd_urls.
        """
        # Add xhtml11.dtd to prevent missing entity errors with XHTML files
        try:
            return self._l10nMarionette.localize_entity(dtd_urls, entity_id)
        except UnknownCommandException:
            dtds = copy.copy(dtd_urls)
            dtds.append("resource:///res/dtd/xhtml11.dtd")

            dtd_refs = ''
            for index, item in enumerate(dtds):
                dtd_id = 'dtd_%s' % index
                dtd_refs += '<!ENTITY %% %s SYSTEM "%s">%%%s;' % \
                    (dtd_id, item, dtd_id)

            contents = """<?xml version="1.0"?>
                <!DOCTYPE elem [%s]>

                <elem id="entity">&%s;</elem>""" % (dtd_refs, entity_id)

            with self.marionette.using_context('chrome'):
                value = self.marionette.execute_script("""
                    var parser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
                                 .createInstance(Components.interfaces.nsIDOMParser);
                    var doc = parser.parseFromString(arguments[0], "text/xml");
                    var node = doc.querySelector("elem[id='entity']");

                    return node ? node.textContent : null;
                """, script_args=[contents])

            if not value:
                raise NoSuchElementException('DTD Entity not found: %s' % entity_id)

            return value

    def localize_property(self, property_urls, property_id):
        """Returns the localized string for the specified property id.

        To find the property all given property files will be searched for
        the id.

        :param property_urls: A list of property files to search.
        :param property_id: The id to retrieve the value from.

        :returns: The localized string for the requested entity.

        :raises NoSuchElementException: When property id is not found in
            property_urls.
        """
        try:
            return self._l10nMarionette.localize_property(property_urls, property_id)
        except UnknownCommandException:
            with self.marionette.using_context('chrome'):
                value = self.marionette.execute_script("""
                    let property = null;
                    let property_id = arguments[1];

                    arguments[0].some(aUrl => {
                      let bundle = Services.strings.createBundle(aUrl);

                      try {
                        property = bundle.GetStringFromName(property_id);
                        return true;
                      }
                      catch (ex) { }
                    });

                    return property;
                """, script_args=[property_urls, property_id])

            if not value:
                raise NoSuchElementException('Property not found: %s' % property_id)

            return value
