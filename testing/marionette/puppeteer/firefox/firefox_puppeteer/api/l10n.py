# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import copy

from marionette_driver.errors import MarionetteException

from firefox_puppeteer.base import BaseLib


class L10n(BaseLib):

    def get_entity(self, dtd_urls, entity_id):
        """Returns the localized string for the specified DTD entity id.

        To find the entity all given DTD files will be searched for the id.

        :param dtd_urls: A list of dtd files to search.
        :param entity_id: The id to retrieve the value from.

        :returns: The localized string for the requested entity.

        :raises MarionetteException: When entity id is not found in dtd_urls.
        """
        # Add xhtml11.dtd to prevent missing entity errors with XHTML files
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
            raise MarionetteException('DTD Entity not found: %s' % entity_id)

        return value

    def get_property(self, property_urls, property_id):
        """Returns the localized string for the specified property id.

        To find the property all given property files will be searched for
        the id.

        :param property_urls: A list of property files to search.
        :param property_id: The id to retrieve the value from.

        :returns: The localized string for the requested entity.

        :raises MarionetteException: When property id is not found in
            property_urls.
        """

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
            raise MarionetteException('Property not found: %s' % property_id)

        return value
