/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

class SmsManager
{
    static private ISmsManager sInstance;

    static public ISmsManager getInstance() {
        if (AppConstants.MOZ_WEBSMS_BACKEND) {
            if (sInstance == null) {
                sInstance = new GeckoSmsManager();
            }
        }
        return sInstance;
    }
}

interface ISmsManager
{
  public void start();
  public void stop();
  public void shutdown();

  public void send(String aNumber, String aMessage, int aRequestId);
  public void getMessage(int aMessageId, int aRequestId);
  public void deleteMessage(int aMessageId, int aRequestId);
  public void createMessageList(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, int aDeliveryState, boolean aReverse, int aRequestId);
  public void getNextMessageInList(int aListId, int aRequestId);
  public void clearMessageList(int aListId);
}
