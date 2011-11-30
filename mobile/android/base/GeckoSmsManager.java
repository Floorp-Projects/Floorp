/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
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

package org.mozilla.gecko;

import java.util.ArrayList;

import android.util.Log;

import android.content.Intent;
import android.content.BroadcastReceiver;
import android.content.Context;

import android.os.Bundle;

import android.telephony.SmsManager;
import android.telephony.SmsMessage;

public class GeckoSmsManager
  extends BroadcastReceiver
{
  final static int kMaxMessageSize = 160;

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals("android.provider.Telephony.SMS_RECEIVED")) {
      // TODO: Try to find the receiver number to be able to populate
      //       SmsMessage.receiver.
      // TODO: Get the id and the date from the stock app saved message.
      //       Using the stock app saved message require us to wait for it to
      //       be saved which can lead to race conditions.

      Bundle bundle = intent.getExtras();

      if (bundle == null) {
        return;
      }

      Object[] pdus = (Object[]) bundle.get("pdus");

      for (int i=0; i<pdus.length; ++i) {
        SmsMessage msg = SmsMessage.createFromPdu((byte[])pdus[i]);

        GeckoAppShell.notifySmsReceived(msg.getDisplayOriginatingAddress(),
                                        msg.getDisplayMessageBody(),
                                        System.currentTimeMillis());
      }
    }
  }

  public static int getNumberOfMessagesForText(String aText) {
    return SmsManager.getDefault().divideMessage(aText).size();
  }

  public static void send(String aNumber, String aMessage) {
    /*
     * TODO:
     * This is a basic send method that doesn't handle errors, doesn't listen to
     * sent and received messages. It's only calling the send method.
     */
    try {
      SmsManager sm = SmsManager.getDefault();

      if (aMessage.length() <= kMaxMessageSize) {
        sm.sendTextMessage(aNumber, "", aMessage, null, null);
      } else {
        ArrayList<String> parts = sm.divideMessage(aMessage);
        sm.sendMultipartTextMessage(aNumber, "", parts, null, null);
      }
    } catch (Exception e) {
      Log.i("GeckoSmsManager", "Failed to send an SMS: ", e);
    }
  }
}
