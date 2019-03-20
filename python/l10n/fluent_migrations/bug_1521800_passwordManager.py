# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY, CONCAT

def migrate(ctx):
    """Bug 1521800 - Move Strings from passwordManager.dtd and passwordmgr.properties to Fluent, part {index}"""
    ctx.add_transforms(
        "toolkit/toolkit/passwordmgr/passwordManagerList.ftl",
        "toolkit/toolkit/passwordmgr/passwordManagerList.ftl",
        transforms_from(
"""
saved-logins =
    .title = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","savedLogins.title") }
window-close =
    .key = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","windowClose.key") }
focus-search-shortcut =
    .key = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","focusSearch1.key") }
focus-search-altshortcut =
    .key = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","focusSearch2.key") }
copy-site-url-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copySiteUrlCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copySiteUrlCmd.accesskey") }
launch-site-url-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","launchSiteUrlCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","launchSiteUrlCmd.accesskey") }
copy-username-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copyUsernameCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copyUsernameCmd.accesskey") }
edit-username-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","editUsernameCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","editUsernameCmd.accesskey") }
copy-password-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copyPasswordCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","copyPasswordCmd.accesskey") }
edit-password-cmd =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","editPasswordCmd.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","editPasswordCmd.accesskey") }
search-filter =
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","searchFilter.accesskey") }
    .placeholder =  { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","searchFilter.label") }
column-heading-site =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.site.label") }
column-heading-username =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.username.label") }
column-heading-password =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.password.label") }
column-heading-time-created =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.timeCreated.label") }
column-heading-time-last-used =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.timeLastUsed.label") }
column-heading-time-password-changed =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.timePasswordChanged.label") }
column-heading-times-used =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","treehead.timesUsed.label") }
remove =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","remove.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","remove.accesskey") }
import =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","import.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","import.accesskey") }
close-button =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","closebutton.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordManager.dtd","closebutton.accesskey") }
show-passwords =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","showPasswords") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","showPasswordsAccessKey") }
hide-passwords =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","hidePasswords") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","hidePasswordsAccessKey") }
logins-description-all = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","loginsDescriptionAll2") }
logins-description-filtered = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","loginsDescriptionFiltered") }
remove-all =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAll.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAll.accesskey") }
remove-all-shown =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAllShown.label") }
    .accesskey = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAllShown.accesskey") }
remove-all-passwords-prompt = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAllPasswordsPrompt") }
remove-all-passwords-title = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","removeAllPasswordsTitle") }
no-master-password-prompt = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","noMasterPasswordPrompt") }
auto-fill-logins-and-passwords =
    .label = { COPY("toolkit/chrome/passwordmgr/passwordmgr.properties","autofillLoginsAndPasswords") }
"""
        )
    )
