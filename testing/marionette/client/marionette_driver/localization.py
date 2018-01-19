# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import


class L10n(object):
    """An API which allows Marionette to handle localized content.

    The `localization`_ of UI elements in Gecko based applications is done via
    entities and properties. For static values entities are used, which are located
    in .dtd files. Whereby for dynamically updated content the values come from
    .property files. Both types of elements can be identifed via a unique id,
    and the translated content retrieved.

    For example::

        from marionette_driver.localization import L10n
        l10n = L10n(marionette)

        l10n.localize_entity(["chrome://branding/locale/brand.dtd"], "brandShortName")
        l10n.localize_property(["chrome://global/locale/findbar.properties"], "FastFind"))

    .. _localization: https://mzl.la/2eUMjyF
    """

    def __init__(self, marionette):
        self._marionette = marionette

    def localize_entity(self, dtd_urls, entity_id):
        """Retrieve the localized string for the specified entity id.

        :param dtd_urls: List of .dtd URLs which will be used to search for the entity.
        :param entity_id: ID of the entity to retrieve the localized string for.

        :returns: The localized string for the requested entity.
        :raises: :exc:`NoSuchElementException`
        """
        body = {"urls": dtd_urls, "id": entity_id}
        return self._marionette._send_message("localization:l10n:localizeEntity",
                                              body, key="value")

    def localize_property(self, properties_urls, property_id):
        """Retrieve the localized string for the specified property id.

        :param properties_urls: List of .properties URLs which will be used to
                                search for the property.
        :param property_id: ID of the property to retrieve the localized string for.

        :returns: The localized string for the requested property.
        :raises: :exc:`NoSuchElementException`
        """
        body = {"urls": properties_urls, "id": property_id}
        return self._marionette._send_message("localization:l10n:localizeProperty",
                                              body, key="value")
