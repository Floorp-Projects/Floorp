/* mozillasounds.cmd */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is a supplemental OS/2 script.
 *
 * The Initial Developer of the Original Code is
 *   Rich Walsh <dragtext@e-vertise.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*****************************************************************************/
/*                                                                           */
/* MozSounds.cmd enables the user to associate sounds with Mozilla events    */
/* using the WPS Sound object in the System Setup folder.  It does not set   */
/* the sounds itself - it simply adds entries to the Sound object's list of  */
/* events.  This script only needs to be run once or twice:  the first time  */
/* to enable selected sounds, and the second time to disable most of them    */
/* because they're so annoying.                                              */
/*                                                                           */
/* This script's design is coordinated with code in widget\os2\nsSound.cpp.  */
/* Please don't make significant changes to it (e.g. changing the names of   */
/* ini-file entries) without first examining nsSound.cpp.                    */
/*                                                                           */
/* Note to Translators:  everything that needs to be translated has been     */
/* grouped together and placed toward the end of the file (a heading         */
/* identifies where to start).  Please preserve the formatting and don't     */
/* change any of the numeric values.  Thanks...                              */
/*                                                                           */
/*****************************************************************************/

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
call InitVariables
call Main
exit

/*****************************************************************************/

Main:

  /* Get the location of the MMOS2 directory. */
  path = value('MMBASE',,'OS2ENVIRONMENT')
  if path = '' then do
    call SysCls
    call EnvError
    return
  end
  path = strip(path, B, ';')

  /* Confirm that mmpm.ini can be found where we expect it to be. */
  iniFile = path'\MMPM.INI'
  call SysFileTree iniFile, 'stem', 'FO'
  if stem.0 <> 1 then do
    call SysCls
    call IniError
    return
  end

  /* Make a backup of mmpm.ini if one doesn't already exist. */
  call SysFileTree iniFile'.BAK', 'stem', 'FO'
  if stem.0 = 0 then
    '@xcopy' iniFile path'\*.*.BAK /T > NUL'

  /* Events are identified by number - MMOS2 uses 0-12.  Mozilla events */
  /* start at 800.  If this conflicts with another app, the base index  */
  /* can be changed.  The new value is stored in mmpm.ini where it can  */
  /* be accessed by the Mozilla apps and this script.                   */
  baseIndex = SysIni(iniFile, 'MOZILLA_Events', 'BaseIndex')
  baseIndex = strip(baseIndex, T, X2C('0'))
  if baseIndex = 'ERROR:' | baseIndex <= '12' then
    baseIndex = defaultIndex

  /* The main loop:  display current status & respond to commands */
  do FOREVER
    call SysCls
    call GetStatus
    call Display
    pull cmd parms

    select
      when cmd = soundCmd then do
        rc = SysOpenObject('<WP_SOUND>', 2, 'TRUE')
        leave
      end

      when cmd = exitCmd then do
        leave
      end

      when cmd = disableCmd then do
        call Disable
      end

      when cmd = enableCmd then do
        call Enable
      end

      /* this command is "undocumented" & should NOT be translated */
      when cmd = 'BASEINDEX' then do
        call ChangeBaseIndex
      end

      otherwise
    end
  end

  return

/*****************************************************************************/

/* Generate a listing of Mozilla events and their status: */
/* 'enabled' if there's an entry, 'disabled' if not.      */

GetStatus:

  ctr = 1
  show = 0

  do nbr = 1 to events.0
    rc = SysIni(iniFile, 'MMPM2_AlarmSounds', baseIndex + events.nbr.ndx)
    if rc = 'ERROR:' then
      status = disabledWord
    else
      status = enabledWord

    if show = 0 then do
      out = " "nbr". " left(events.nbr.name, 15) status
      show = 1
    end
    else do
      line.ctr = left(out, 35) " " nbr". " left(events.nbr.name, 15) status
      ctr = ctr + 1
      show = 0
    end
  end

  if show = 1 then
    line.ctr = left(out, 35)

  return

/*****************************************************************************/

/* Disable an event sound by deleting its entry. */

Disable:

  parse var parms nbr parms

  if nbr = allWord then do
    nbr = '1'
    parms = '2 3 4 5 6 7'
  end

  do while nbr <> ''
    if nbr >= '1' & nbr <= '7' then do
      key = baseIndex + events.nbr.ndx
      rc = SysIni(iniFile, 'MMPM2_AlarmSounds', key, 'DELETE:')
    end
    parse var parms nbr parms
  end

return

/*****************************************************************************/

/* Enable an event sound by adding an entry whose format is:     */
/*   'fq_soundfile#event_name#volume'.                           */
/* Since this script isn't intended to set the actual soundfile, */
/* it uses the same dummy value as MMOS2: 'x:\MMOS2\SOUNDS\'     */

Enable:

  parse var parms nbr parms

  if nbr = allWord then do
    nbr = '1'
    parms = '2 3 4 5 6 7'
  end

  do while nbr <> ''
    if nbr >= '1' & nbr <= '7' then do
      key = baseIndex + events.nbr.ndx

      /* if there's an existing entry, preserve the filename */
      sndFile = SysIni(iniFile, 'MMPM2_AlarmSounds', key)
      if sndFile = 'ERROR:' | left(sndFile, 1) = '#' then
        sndFile = path'\'soundsDir'\'
      else
        parse var sndFile sndFile '#' .
      value = sndFile'#'events.nbr.name' (Mozilla)#80'X2C('0')

      rc = SysIni(iniFile, 'MMPM2_AlarmSounds', key, value)
    end
    parse var parms nbr parms
  end

return

/*****************************************************************************/

/* An "undocumented" function to change & restore the base index.     */
/* It renumbers existing entries using the new base & adds or deletes */
/* a 'MOZILLA_Events\BaseIndex' entry depending on the new value.     */

ChangeBaseIndex:

  parse var parms newIndex parms

  /* Ignore invalid values. */
  if newIndex = '' | newIndex <= '12' then
    return

  /* Do NOT translate this. */
  if newIndex = 'DEFAULT' then
    newIndex = defaultIndex

  /* If there's no change, exit after deleting the entry if it's the default */
  if newIndex = baseIndex then do
    if baseIndex = defaultIndex then
      rc SysIni(iniFile, 'MOZILLA_Events', 'BaseIndex', 'DELETE:')
    return
  end

  /* Move existing entries from the old index to the new index. */
  do nbr = 1 to events.0
    value = SysIni(iniFile, 'MMPM2_AlarmSounds', baseIndex + events.nbr.ndx)
    if value = 'ERROR:' then
      iterate

    rc = SysIni(iniFile, 'MMPM2_AlarmSounds', baseIndex + events.nbr.ndx, 'DELETE:')
    rc = SysIni(iniFile, 'MMPM2_AlarmSounds',  newIndex + events.nbr.ndx, value)
  end

  /* If the new index is the default, delete the ini entry; */
  /* otherwise, add or update the entry with the new value. */
  if newIndex = defaultIndex then
    rc = SysIni(iniFile, 'MOZILLA_Events', 'BaseIndex', 'DELETE:')
  else
    rc = SysIni(iniFile, 'MOZILLA_Events', 'BaseIndex', newIndex||X2C('0'))

  baseIndex = newIndex

  return

/*****************************************************************************/
/* All strings that need to be translated appear below                       */
/*****************************************************************************/

/* Just like it says, init variables. */

InitVariables:

  events.0 = 7
  events.1.ndx  = 0
  events.1.name = 'New Mail'
  events.2.ndx  = 1
  events.2.name = 'Alert Dialog'
  events.3.ndx  = 2
  events.3.name = 'Confirm Dialog'
  events.4.ndx  = 3
  events.4.name = 'Prompt Dialog'
  events.5.ndx  = 4
  events.5.name = 'Select Dialog'
  events.6.ndx  = 5
  events.6.name = 'Menu Execute'
  events.7.ndx  = 6
  events.7.name = 'Menu Popup'

  enabledWord   = 'enabled'
  disabledWord  = 'disabled'
  allWord       = 'ALL'
  soundsDir     = 'SOUNDS'

  enableCmd     = 'E'
  disableCmd    = 'D'
  soundCmd      = 'S'
  exitCmd       = 'X'

  defaultIndex  = '800'

  return

/*****************************************************************************/

/* Display event status & command info. */

Display:

  say
  say " MozSounds.cmd lets you use the WPS Sound object in your System Setup"
  say " folder to assign sounds to the Mozilla events listed below.  It does"
  say " NOT set the sound itself - it just adds and deletes entries in Sound."
  say " New or changed sounds take effect after the Mozilla app is restarted."
  say
  say "     Event           Status               Event           Status"
  say "     -----------     --------             -----------     --------"
  say line.1
  say line.2
  say line.3
  say line.4
  say
  say " Commands:"
  say " E - enable event sound(s)    example: 'E 1' or 'E 3 5 7' or 'E All'"
  say " D - disable event sound(s)   example: 'D 2' or 'D 1 4 6' or 'D All'"
  say " S - open the WPS Sound object"
  say " X - exit this script"
  say
  call charout ," Enter a command > "

  return

/*****************************************************************************/

/* Display an error message */

EnvError:

  say ""
  say "ERROR:  the 'MMBASE' environment variable is missing or invalid!"
  say "  Your Mozilla app won't be able to play system sounds without it."
  say "  Please add a line to config.sys like the following, then reboot."
  say "  SET MMBASE=x:\MMOS2  (where 'x' is the correct drive)"

  return

/*****************************************************************************/

/* Display an error message */

IniError:

  say ""
  say "ERROR:  file '"iniFile"' is missing or invalid!"
  say "  Your Mozilla app won't be able to play system sounds without it."
  say "  Please confirm that the 'SET MMBASE=' line in config.sys points"
  say "  at your MMOS2 directory and that it contains 'MMPM.INI'."

  return

/*****************************************************************************/

