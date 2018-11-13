# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import CONCAT
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1498451 - Migrate Device Manager Dialog of Preferences Section to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "security/manager/security/certificates/deviceManager.ftl",
        "security/manager/security/certificates/deviceManager.ftl",
        transforms_from(
"""
devmgr =
  .title = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.title") }
  .style = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.style2") }

devmgr-devlist =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.devlist.label") }

devmgr-header-details =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.details.title") }

devmgr-header-value =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.details.title2") }

devmgr-button-login =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.login.label") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.login.accesskey") }

devmgr-button-logout =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.logout.label") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.logout.accesskey") }

devmgr-button-changepw =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.changepw.label") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.changepw.accesskey") }

devmgr-button-load =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.load.label") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.load.accesskey") }

devmgr-button-unload =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.unload.label") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "devmgr.button.unload.accesskey") }

load-device =
  .title = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.title2") }

load-device-info = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.info") }

load-device-modname =
  .value = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.modname2") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.modname2.accesskey") }

load-device-modname-default =
  .value = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.modname.default") }

load-device-filename =
  .value = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.filename2") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.filename2.accesskey") }

load-device-browse =
  .label = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.browse") }
  .accesskey = { COPY("security/manager/chrome/pippki/deviceManager.dtd", "loaddevice.browse.accesskey") }

devinfo-status =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_status") }

devinfo-status-disabled =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_disabled") }

devinfo-status-not-present =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_notpresent") }

devinfo-status-uninitialized =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_uninitialized") }

devinfo-status-not-logged-in =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_notloggedin") }

devinfo-status-logged-in
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_loggedin") }

devinfo-status-ready =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_stat_ready") }

devinfo-desc =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_desc") }

devinfo-man-id =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_manID") }

devinfo-hwversion =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_hwversion") }

devinfo-fwversion =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_fwversion") }

devinfo-modname =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_modname") }

devinfo-modpath =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_modpath") }

login-failed = { COPY("security/manager/chrome/pippki/pippki.properties", "login_failed") }

devinfo-label =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_label") }

devinfo-serialnum =
  .label = { COPY("security/manager/chrome/pippki/pippki.properties", "devinfo_serialnum") }

fips-nonempty-password-required = { COPY("security/manager/chrome/pippki/pippki.properties", "fips_nonempty_password_required") }
unable-to-toggle-fips = { COPY("security/manager/chrome/pippki/pippki.properties", "unable_to_toggle_fips") }
load-pk11-module-file-picker-title = { COPY("security/manager/chrome/pippki/pippki.properties", "loadPK11ModuleFilePickerTitle") }

load-module-help-empty-module-name =
  .value = { COPY("security/manager/chrome/pippki/pippki.properties", "loadModuleHelp_emptyModuleName") }

load-module-help-root-certs-module-name =
  .value = { COPY("security/manager/chrome/pippki/pippki.properties", "loadModuleHelp_rootCertsModuleName") }

add-module-failure = { COPY("security/manager/chrome/pipnss/pipnss.properties", "AddModuleFailure") }
del-module-warning = { COPY("security/manager/chrome/pipnss/pipnss.properties", "DelModuleWarning") }
del-module-error = { COPY("security/manager/chrome/pipnss/pipnss.properties", "DelModuleError") }
""")
)

    ctx.add_transforms(
        "security/manager/security/certificates/deviceManager.ftl",
        "security/manager/security/certificates/deviceManager.ftl",
        [
            FTL.Message(
            id=FTL.Identifier("devmgr-button-enable-fips"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("label"),
                    value=COPY(
                            "security/manager/chrome/pippki/pippki.properties",
                            "enable_fips",
                        )
                    ),
                FTL.Attribute(
                    id=FTL.Identifier("accesskey"),
                    value=COPY(
                            "security/manager/chrome/pippki/deviceManager.dtd",
                            "devmgr.button.fips.accesskey",
                            )
                        )
                    ]
                ),
            FTL.Message(
            id=FTL.Identifier("devmgr-button-disable-fips"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("label"),
                    value=COPY(
                            "security/manager/chrome/pippki/pippki.properties",
                            "disable_fips",
                            )
                        ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "security/manager/chrome/pippki/deviceManager.dtd",
                            "devmgr.button.fips.accesskey",
                            )
                        )
                    ]
                ),
            ]
        )

