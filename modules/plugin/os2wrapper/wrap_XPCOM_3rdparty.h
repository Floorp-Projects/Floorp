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
 * The Original Code is InnoTek Plugin Wrapper code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   InnoTek Systemberatung GmbH / Knut St. Osmundsen
 *   Peter Weilbacher <mozilla@weilbacher.org>
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

/*
 *  This is hand written classes based on XPT files.
 */

#ifndef __wrap_XPCOM_3rdparty_h__
#define __wrap_XPCOM_3rdparty_h__


/* starting interface:    FlashIObject7 */
#define FLASH_IOBJECT7_IID_STR "42b1d5a4-6c2b-11d6-8063-0005029bc257"
#define FLASH_IOBJECT7_IID  {0x42b1d5a4, 0x6c2b, 0x11d6, { 0x80, 0x63, 0x00, 0x05, 0x02, 0x9b, 0xc2, 0x57 }}

class NS_NO_VTABLE FlashIObject7 : public nsISupports
{
public:
    NS_IMETHOD Evaluate(const char *aString, FlashIObject7 ** aFlashObject) = 0;
};


/* starting interface:    FlashIScriptablePlugin7 */
#define FLASH_ISCRIPTABLEPLUGIN7_IID_STR "d458fe9c-518c-11d6-84cb-0005029bc257"
#define FLASH_ISCRIPTABLEPLUGIN7_IID  {0xd458fe9c, 0x518c, 0x11d6, { 0x84, 0xcb, 0x00, 0x05, 0x02, 0x9b, 0xc2, 0x57 }}
class NS_NO_VTABLE FlashIScriptablePlugin7 : public nsISupports
{
public:
    NS_IMETHOD IsPlaying(PRBool *aretval) = 0;
    NS_IMETHOD Play(void) = 0;
    NS_IMETHOD StopPlay(void) = 0;
    NS_IMETHOD TotalFrames(PRInt32 *aretval) = 0;
    NS_IMETHOD CurrentFrame(PRInt32 *aretval) = 0;
    NS_IMETHOD GotoFrame(PRInt32 aFrame) = 0;
    NS_IMETHOD Rewind(void) = 0;
    NS_IMETHOD Back(void) = 0;
    NS_IMETHOD Forward(void) = 0;
    NS_IMETHOD Pan(PRInt32 aX, PRInt32 aY, PRInt32 aMode) = 0; /* version 7 moved this */
    NS_IMETHOD PercentLoaded(PRInt32 *aretval) = 0;
    NS_IMETHOD FrameLoaded(PRInt32 aFrame, PRBool *aretval) = 0;
    NS_IMETHOD FlashVersion(PRInt32 *aretval) = 0;
    NS_IMETHOD Zoom(PRInt32 aPercent) = 0;
    NS_IMETHOD SetZoomRect(PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom) = 0;
    NS_IMETHOD LoadMovie(PRInt32 aLayer, PRUnichar *aURL) = 0;
    NS_IMETHOD TGotoFrame(PRUnichar *aTarget, PRInt32 aFrameNumber) = 0;
    NS_IMETHOD TGotoLabel(PRUnichar *aTarget, PRUnichar *aLabel) = 0;
    NS_IMETHOD TCurrentFrame(PRUnichar *aTarget, PRInt32 *aretval) = 0;
    NS_IMETHOD TCurrentLabel(PRUnichar *aTarget, PRUnichar **aretval) = 0;
    NS_IMETHOD TPlay(PRUnichar *aTarget) = 0;
    NS_IMETHOD TStopPlay(PRUnichar *aTarget) = 0;
    NS_IMETHOD SetVariable(PRUnichar *aVariable, PRUnichar *aValue) = 0;
    NS_IMETHOD GetVariable(PRUnichar *aVariable, PRUnichar **aretval) = 0;
    NS_IMETHOD TSetProperty(PRUnichar *aTarget, PRInt32 aProperty, PRUnichar *aValue) = 0;
    NS_IMETHOD TGetProperty(PRUnichar *aTarget, PRInt32 aProperty, PRUnichar **aretval) = 0;
    NS_IMETHOD TGetPropertyAsNumber(PRUnichar *aTarget, PRInt32 aProperty, double **aretval) = 0;
    NS_IMETHOD TCallLabel(PRUnichar *aTarget, PRUnichar *aLabel) = 0;
    NS_IMETHOD TCallFrame(PRUnichar *aTarget, PRInt32 aFrame) = 0;
    NS_IMETHOD SetWindow(FlashIObject7 *aFlashObject, PRInt32 a1) = 0;
};


/* starting interface:    nsIPlugin5 */
#define NS_IFLASH5_IID_STR  "d27cdb6e-ae6d-11cf-96b8-444553540000"
#define NS_IFLASH5_IID      {0xd27cdb6e, 0xae6d, 0x11cf, { 0x96, 0xb8, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }}
class NS_NO_VTABLE nsIFlash5 : public nsISupports
{
public:
    NS_IMETHOD IsPlaying(PRBool *aretval) = 0;
    NS_IMETHOD Play(void) = 0;
    NS_IMETHOD StopPlay(void) = 0;
    NS_IMETHOD TotalFrames(PRInt32 *aretval) = 0;
    NS_IMETHOD CurrentFrame(PRInt32 *aretval) = 0;
    NS_IMETHOD GotoFrame(PRInt32 aPosition) = 0;
    NS_IMETHOD Rewind(void) = 0;
    NS_IMETHOD Back(void) = 0;
    NS_IMETHOD Forward(void) = 0;
    NS_IMETHOD PercentLoaded(PRInt32 *aretval) = 0;
    NS_IMETHOD FrameLoaded(PRInt32 aFrame, PRBool *aretval) = 0;
    NS_IMETHOD FlashVersion(PRInt32 *aretval) = 0;
    NS_IMETHOD Pan(PRInt32 aX, PRInt32 aY, PRInt32 aMode) = 0;
    NS_IMETHOD Zoom(PRInt32 aPercent) = 0;
    NS_IMETHOD SetZoomRect(PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom) = 0;
    NS_IMETHOD LoadMovie(PRInt32 aLayer, const char *aURL) = 0;
    NS_IMETHOD TGotoFrame(const char *aTarget, PRInt32 frameNum) = 0;
    NS_IMETHOD TGotoLabel(const char *aTarget, const char *aLabel) = 0;
    NS_IMETHOD TCurrentFrame(const char *aTarget, PRInt32 *aretval) = 0;
    NS_IMETHOD TCurrentLabel(const char *aTarget, char **aretval) = 0;
    NS_IMETHOD TPlay(const char *aTarget) = 0;
    NS_IMETHOD TStopPlay(const char *aTarget) = 0;
    NS_IMETHOD SetVariable(const char *aName, const char *aValue) = 0;
    NS_IMETHOD GetVariable(const char *aName, char **aretval) = 0;
    NS_IMETHOD TSetProperty(const char *aTarget, PRInt32 aProperty, const char *value) = 0;
    NS_IMETHOD TGetProperty(const char *aTarget, PRInt32 aProperty, char **aretval) = 0;
    NS_IMETHOD TCallFrame(const char *aTarget, PRInt32 aFrame) = 0;
    NS_IMETHOD TCallLabel(const char *aTarget, const char *aLabel) = 0;
    NS_IMETHOD TGetPropertyAsNumber(const char *aTarget, PRInt32 aProperty, double *aretval) = 0;
    NS_IMETHOD TSetPropertyAsNumber(const char *aTarget, PRInt32 aProperty, double aValue) = 0;
};

#endif
