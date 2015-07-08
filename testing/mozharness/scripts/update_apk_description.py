#!/usr/bin/env python
""" update_apk_description.py

    Update the descriptions of an application (multilang)
    Example of beta update:
    $ python update_apk_description.py --service-account foo@developer.gserviceaccount.com --package-name org.mozilla.firefox --credentials key.p12 --update-apk-description
"""
import sys
import os
import urllib2
import json

from oauth2client import client

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.script import BaseScript
from mozharness.mozilla.googleplay import GooglePlayMixin
from mozharness.base.python import VirtualenvMixin


class UpdateDescriptionAPK(BaseScript, GooglePlayMixin, VirtualenvMixin):
    all_actions = [
        'create-virtualenv',
        'update-apk-description',
        'test',
    ]

    default_actions = [
        'create-virtualenv',
        'test',
    ]

    config_options = [
        [["--service-account"], {
            "dest": "service_account",
            "help": "The service account email",
        }],
        [["--credentials"], {
            "dest": "google_play_credentials_file",
            "help": "The p12 authentication file",
            "default": "key.p12"
        }],
        [["--package-name"], {
            "dest": "package_name",
            "help": "The Google play name of the app",
        }],
        [["--l10n-api-url"], {
            "dest": "l10n_api_url",
            "help": "The L10N URL",
            "default": "https://l10n.mozilla-community.org/~pascalc/google_play_description/"
        }],
        [["--force-locale"], {
            "dest": "force_locale",
            "help": "Force a specific locale (instead of all)",
        }],
    ]

    # We have 3 apps. Make sure that their names are correct
    package_name_values = ("org.mozilla.fennec_aurora",
                           "org.mozilla.firefox_beta",
                           "org.mozilla.firefox")

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):

        # Default configuration
        default_config = {
            'debug_build': False,
            'pip_index': True,
            # this will pip install it automajically when we call the
            # create-virtualenv action
            'virtualenv_modules': ['google-api-python-client'],
            "find_links": [
                "http://pypi.pvt.build.mozilla.org/pub",
                "http://pypi.pub.build.mozilla.org/pub",
            ],
            'virtualenv_path': 'venv',
        }
        default_config.update(config)

        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions,
        )

        self.all_locales_url = self.config['l10n_api_url'] + "api/?done&channel={channel}"
        self.locale_url = self.config['l10n_api_url'] + "api/?locale={locale}&channel={channel}"
        self.mapping_url = self.config['l10n_api_url'] + "api/?locale_mapping&reverse"

    def check_argument(self):
        """ Check that the given values are correct,
        files exists, etc
        """
        if self.config['package_name'] not in self.package_name_values:
            self.fatal("Unknown package name value " +
                       self.config['package_name'])

        if not os.path.isfile(self.config['google_play_credentials_file']):
            self.fatal("Could not find " + self.config['google_play_credentials_file'])

    def get_list_locales(self, package_name):

        """ Get all the translated locales supported by Google play
        So, locale unsupported by Google play won't be downloaded
        Idem for not translated locale
        """
        try:
            response = urllib2.urlopen(self.all_locales_url.format(channel=package_name))
        except urllib2.HTTPError:
            self.fatal("Could not download the locale list. Channel: '" + package_name + "', URL: '" + self.all_locales_url + "'")

        return json.load(response)

    def get_mapping(self):
        """ Download and load the locale mapping
        """
        try:
            response = urllib2.urlopen(self.mapping_url)
        except urllib2.HTTPError:
            self.fatal("Could not download the locale mapping list. URL: '" + self.mapping_url + "'")
        self.mappings = json.load(response)

    def locale_mapping(self, locale):
        """ Google play and Mozilla don't have the exact locale code
        Translate them
        """
        if locale in self.mappings:
            return self.mappings[locale]
        else:
            return locale

    def get_translation(self, package_name, locale):
        """ Get the translation for a locale
        """
        localeURL = self.locale_url.format(channel=package_name, locale=locale)
        try:
            response = urllib2.urlopen(localeURL)
        except urllib2.HTTPError:
            self.fatal("Could not download the locale translation. Locale: '" + locale + "', Channel: '" + package_name + "', URL: '" + localeURL + "'")
        return json.load(response)

    def update_desc(self, service, package_name):
        """ Update the desc on google play

        service -- The session to Google play
        package_name -- The name of the package
        locale -- The locale to update
        description -- The new description
        """

        edit_request = service.edits().insert(body={},
                                              packageName=package_name)
        result = edit_request.execute()
        edit_id = result['id']

        # Retrieve the mapping
        self.get_mapping()

        if self.config.get("force_locale"):
            # The user forced a locale, don't need to retrieve the full list
            locales = [self.config.get("force_locale")]
        else:
            # Get all the locales from the web interface
            locales = self.get_list_locales(package_name)
        nb_locales = 0
        for locale in locales:
            translation = self.get_translation(package_name, locale)
            title = translation.get("title")
            short_desc = translation.get("short_desc")
            long_desc = translation.get("long_desc")

            # Google play expects some locales codes (de-DE instead of de)
            locale = self.locale_mapping(locale)

            try:
                self.log("Updating " + package_name + " for '" + locale +
                         "' /  title: '" + title + "', short_desc: '" +
                         short_desc[0:20] + "'..., long_desc: '" +
                         long_desc[0:20] + "...'")
                listing_response = service.edits().listings().update(
                    editId=edit_id, packageName=package_name, language=locale,
                    body={'fullDescription': long_desc,
                          'shortDescription': short_desc,
                          'title': title}).execute()
                nb_locales += 1
            except client.AccessTokenRefreshError:
                self.log('The credentials have been revoked or expired,'
                         'please re-run the application to re-authorize')

        # Commit our changes
        commit_request = service.edits().commit(
            editId=edit_id, packageName=package_name).execute()
        self.log('Edit "%s" has been committed. %d locale(s) updated.' % (commit_request['id'], nb_locales))

    def update_apk_description(self):
        """ Update the description """
        self.check_argument()
        service = self.connect_to_play()
        self.update_desc(service, self.config['package_name'])

    def test(self):
        """
        Test if the connexion can be done and if the various method
        works as expected
        """
        self.check_argument()
        self.connect_to_play()
        package_name = 'org.mozilla.fennec_aurora'
        locales = self.get_list_locales(package_name)
        if not locales:
            self.fatal("get_list_locales() failed")

        self.get_mapping()
        if not self.mappings:
            self.fatal("get_mapping() failed")

        loca = self.locale_mapping("fr")
        if loca != "fr-FR":
            self.fatal("fr locale_mapping failed")
        loca = self.locale_mapping("hr")
        if loca != "hr":
            self.fatal("hr locale_mapping failed")

        translation = self.get_translation(package_name, 'cs')
        if len(translation.get('title')) < 5:
            self.fatal("get_translation title failed for the 'cs' locale")
        if len(translation.get('short_desc')) < 5:
            self.fatal("get_translation short_desc failed for the 'cs' locale")
        if len(translation.get('long_desc')) < 5:
            self.fatal("get_translation long_desc failed for the 'cs' locale")

        package_name = "org.mozilla.firefox_beta"
        translation = self.get_translation(package_name, 'fr')
        if len(translation.get('title')) < 5:
            self.fatal("get_translation title failed for the 'fr' locale")
        if len(translation.get('short_desc')) < 5:
            self.fatal("get_translation short_desc failed for the 'fr' locale")
        if len(translation.get('long_desc')) < 5:
            self.fatal("get_translation long_desc failed for the 'fr' locale")

        package_name = "org.mozilla.firefox"
        translation = self.get_translation(package_name, 'de')
        if len(translation.get('title')) < 5:
            self.fatal("get_translation title failed for the 'de' locale")
        if len(translation.get('short_desc')) < 5:
            self.fatal("get_translation short_desc failed for the 'de' locale")
        if len(translation.get('long_desc')) < 5:
            self.fatal("get_translation long_desc failed for the 'de' locale")

# main {{{1
if __name__ == '__main__':
    myScript = UpdateDescriptionAPK()
    myScript.run_and_exit()
