============
Install Ping
============

The install pings contain some data about the system and the installation process, sent whenever the installer exits [#earlyexit]_.

---------
Stub Ping
---------

The :doc:`Stub Installer </browser/installer/windows/installer/StubInstaller>` sends a ping just before it exits, in function SendPing of `stub.nsi <https://searchfox.org/mozilla-central/source/browser/installer/windows/nsis/stub.nsi>`_. This is sent as an HTTP GET request to DSMO (download-stats.mozilla.org).

Ingestion is handled in `gcp-ingestion <https://mozilla.github.io/gcp-ingestion/>`_ at class StubUri within `ParseUri <https://github.com/mozilla/gcp-ingestion/blob/master/ingestion-beam/src/main/java/com/mozilla/telemetry/decoder/ParseUri.java>`_. Several of the fields are codes which are broken out into multiple boolean columns in the database table.

-----------------
Full Install Ping
-----------------

The :doc:`Full Installer </browser/installer/windows/installer/FullInstaller>` sends a ping just before it exits, in function SendPing of `installer.nsi <https://searchfox.org/mozilla-central/source/browser/installer/windows/nsis/installer.nsi>`_. This is an HTTP POST request with a JSON document, sent to the standard Telemetry endpoint (incoming.telemetry.mozilla.org).

To avoid double counting, the full installer does not send a ping when it is launched from the stub installer, so pings where ``installer_type = "full"`` correspond to installs that did not use the stub.

--------------------------
Querying the install pings
--------------------------

The pings are recorded in the ``firefox_installer.install`` table, accessible in `Redash <https://sql.telemetry.mozilla.org>`_ [#redashlogin]_ using the default "Telemetry (BigQuery)" data source.

Some of the columns are marked [DEPRECATED] because they involve features that were removed, mostly when the stub installer was `streamlined <https://bugzilla.mozilla.org/show_bug.cgi?id=1328445>`_ in Firefox 55. These columns were not removed to keep compatibility and so we could continue to use the old data, but they should no longer be used.

The columns are annotated with "(stub)", "(full)", or "(both)" to indicate which types of installer provide meaningful values.

See also the `JSON schema <https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/templates/firefox-installer/install/install.1.schema.json>`_.

submission_timestamp (both)
  Time the ping was received

installer_type (both)
  Which type of installer generated this ping (full or stub)

installer_version (full)
  Version of the installer itself [#stubversion]_

build_channel (both)
  Channel the installer was built with the branding for ("release", "beta", "nightly", or "default")

update_channel (both)
  Value of MOZ_UPDATE_CHANNEL for the installer build; should generally be the same as build_channel

version, build_id (both)
  Version number and Build ID of the installed product, from ``application.ini``. This is **not** the version of the installer itself.
 
  stub: 0 if the installation failed

  full: ``""`` if not found [#versionfailure]_

locale (both)
  Locale of the installer and of the installed product, in AB_CD format

from_msi (full)
  True if the install was launched from an MSI wrapper.

_64bit_build (both)
  True if a 64-bit build was selected for installation.
  
  stub: This means the OS is 64-bit, the RAM requirement was met, and no third-party software that blocks 64-bit installations was found
  
  full: Hardcoded based on the architecture to be installed

_64bit_os (both)
  True if the version of Windows on the machine was 64-bit.

os_version (both)
  Version number of Windows in ``major.minor.build`` format [#win10build]_

service_pack (stub)
  Latest Windows service pack installed on the machine.

server_os (both)
  True if the installed OS is a server version of Windows.

admin_user (both)
  True if the installer was run by a user with administrator privileges (and the UAC prompt was accepted). Specifically, this reports whether :abbr:`HKLM (HKEY_LOCAL_MACHINE)` was writeable.

default_path (both)
  True if the default installation path was not changed.

  stub: [DEPRECATED] [#stubdefaultpath]_

set_default (both)
  True if the option to set the new installation as the default browser was left selected.

  stub: [DEPRECATED] [#stubsetdefault]_

new_default (both)
  True if the new installation is now the default browser (registered to handle the http protocol).
 
  full: Checks the association using ``AppAssocReg::QueryCurrentDefault`` and :abbr:`HKCU (HKEY_CURRENT_USER)`.

  stub: [DEPRECATED] [#stubnewdefault]_

old_default (both)
  True if firefox.exe in a different directory is now the default browser, mutually exclusive with new_default. The details are the same as new_default.
 
had_old_install (both)
  True if at least one existing installation of Firefox was found on the system prior to this installation.
  
  full: Checks for the installation directory given in the ``Software\Mozilla\${BrandFullName}`` registry keys, either :abbr:`HKLM` or :abbr:`HKCU`

  stub: Checks for the top level profile directory ``%LOCALAPPDATA%\Mozilla\Firefox``

old_version, old_build_id (stub)
  Version number and Build ID (from ``application.ini``) of a previous installation of Firefox in the install directory, 0 if not found

bytes_downloaded (stub)
  Size of the full installer data that was transferred before the download ended (whether it failed, was cancelled, or completed normally)

download_size (stub)
  Expected size of the full installer download according to the HTTP response headers

download_retries (stub)
  Number of times the full installer download was retried or resumed. 10 retries is the maximum.

download_time (stub)
  Number of seconds spent downloading the full installer [#downloadphase]_

download_latency (stub)
  Seconds between sending the full installer download request and receiving the first response data

download_ip (stub)
  IP address of the server the full installer was download from (can be either IPv4 or IPv6)

manual_download (stub)
  True if the user clicked on the button that opens the manual download page. The prompt to do that is shown after the installation fails or is cancelled.

intro_time (both)
  Seconds the user spent on the intro screen.

  stub: [DEPRECATED] The streamlined stub no longer has this screen, so this should always be 0.

options_time (both)
  Seconds the user spent on the options screen.

  stub: [DEPRECATED] The streamlined stub no longer has this screen, so this should always be 0.

preinstall_time (stub)
  Seconds spent verifying the downloaded full installer and preparing to run it

install_time (both)
  full: Seconds taken by the installation phase.

  stub: Seconds taken by the full installer.

finish_time (both)
  full: Seconds the user spent on the finish page.

  stub: Seconds spent waiting for the installed application to launch.

succeeded (both)
  True if a new installation was successfully created. False if that didn't happen for any reason, including when the user closed the installer window.

disk_space_error (stub)
  [DEPRECATED] True if the installation failed because the drive we're trying to install to does not have enough space. The streamlined stub no longer sends a ping in this case, because the installation drive can no longer be selected.

no_write_access (stub)
  [DEPRECATED] True if the installation failed because the user doesn't have permission to write to the path we're trying to install to. The streamlined stub no longer sends a ping in this case, because the installation directory can no longer be selected.

user_cancelled (both)
  True if the installation failed because the user cancelled it or closed the window.

out_of_retries (stub)
  True if the installation failed because the download had to be retried too many times (currently 10)

file_error (stub)
  True if the installation failed because the downloaded file couldn't be read from

sig_not_trusted (stub)
  True if the installation failed because the signature on the downloaded file wasn't valid or wasn't signed by a trusted authority

sig_unexpected (stub)
  True if the installation failed because the signature on the downloaded file didn't have the expected subject and issuer names

install_timeout (stub)
  True if the installation failed because running the full installer timed out. Currently that means it ran for more than 150 seconds for a new installation, or 165 seconds for a paveover installation.

new_launched (both)
  True if the installation succeeded and tried to launch the newly installed application.

old_running (stub)
  [DEPRECATED] True if the installation succeeded and we weren't able to launch the newly installed application because a copy of Firefox was already running. This should always be false since the check for a running copy was `removed <https://bugzilla.mozilla.org/show_bug.cgi?id=1601806>`_ in Firefox 74.

attribution (both)
  Any attribution data that was included with the installer

profile_cleanup_prompt (stub)
  0: neither profile cleanup prompt was shown

  1: the "reinstall" version of the profile cleanup prompt was shown (no existing installation was found, but the user did have an old Firefox profile)

  2: the "paveover" version of the profile cleanup prompt was shown (an installation of Firefox was already present, but it's an older version)

profile_cleanup_requested (stub)
  True if either profile cleanup prompt was shown and the user accepted the prompt

funnelcake (stub)
  `Funnelcake <https://wiki.mozilla.org/Funnelcake>`_ ID

ping_version (stub)
  Version of the stub ping, currently 8

silent (full)
  True if the install was silent (see :ref:`Full Installer Configuration`)

---------
Footnotes
---------

.. [#earlyexit] No ping is sent if the installer exits early because initial system requirements checks fail.

.. [#redashlogin] A Mozilla LDAP login is required to access Redash.

.. [#stubversion] The version of the installer would be useful for the stub, but it is not currently sent as part of the stub ping.

.. [#versionfailure] If the installation failed or was cancelled, the full installer will still report the version number of whatever was in the installation directory, or ``""`` on if it couldn't be read.

.. [#win10build] Previous versions of Windows have used a very small set of build numbers through their entire lifecycle. However, Windows 10 gets a new build number with every major update (about every 6 months), and many more builds have been released on its insider channels. So, to prevent a huge amount of noise, queries using this field should generally filter out the build number and only use the major and minor version numbers to differentiate Windows versions, unless the build number is specifically needed.

.. [#stubdefaultpath] ``default_path`` should always be true in the stub, since we no longer support changing the path, but see `bug 1351697 <https://bugzilla.mozilla.org/show_bug.cgi?id=1351697>`_.

.. [#stubsetdefault] We no longer attempt to change the default browser setting in the streamlined stub, so set_default should always be false.

.. [#stubnewdefault] We no longer attempt to change the default browser setting in the streamlined stub, so new_default should usually be false, but the stub still checks the association at ``Software\Classes\http\shell\open\command`` in :abbr:`HKLM` or :abbr:`HKCU`. 

.. [#downloadphase] ``download_time`` was previously called ``download_phase_time``, this includes retries during the download phase. There was a different ``download_time`` field that specifically measured only the time of the last download, this is still submitted but it is ignored during ingestion.
