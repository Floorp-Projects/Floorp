/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public class SmsManager {
    private static final ISmsManager sInstance;
    static {
        if (AppConstants.MOZ_WEBSMS_BACKEND) {
            sInstance = new GeckoSmsManager();
        } else {
            sInstance = null;
        }
    }

    public static ISmsManager getInstance() {
        return sInstance;
    }

    public static boolean isEnabled() {
        return AppConstants.MOZ_WEBSMS_BACKEND;
    }

    public interface ISmsManager {
        public void start();
        public void stop();
        public void shutdown();

        public void send(String aNumber, String aMessage, int aRequestId);
        public void getMessage(int aMessageId, int aRequestId);
        public void deleteMessage(int aMessageId, int aRequestId);
        public void createMessageList(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, String aDelivery, boolean aHasRead, boolean aRead, long aThreadId, boolean aReverse, int aRequestId);
        public void getNextMessageInList(int aListId, int aRequestId);
        public void clearMessageList(int aListId);
    }
}

