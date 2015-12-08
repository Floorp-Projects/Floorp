/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.sqlite.SQLiteException;
import android.net.Uri;
import android.os.Build;
import android.provider.Telephony;
import android.telephony.SmsManager;
import android.util.Log;

import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_DELIVERY_IND;
import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND;
import static com.google.android.mms.pdu.PduHeaders.MESSAGE_TYPE_READ_ORIG_IND;

import com.google.android.mms.MmsException;
import com.google.android.mms.pdu.DeliveryInd;
import com.google.android.mms.pdu.GenericPdu;
import com.google.android.mms.pdu.NotificationInd;
import com.google.android.mms.pdu.PduHeaders;
import com.google.android.mms.pdu.PduParser;
import com.google.android.mms.pdu.PduPersister;
import com.google.android.mms.pdu.ReadOrigInd;
import com.google.android.mms.util.SqliteWrapper;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoSmsManager;

public class MmsService extends BroadcastReceiver {
    private final static String LOGTAG = "MmsService";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(Telephony.Sms.Intents.WAP_PUSH_DELIVER_ACTION)) {
            final byte[] data = intent.getByteArrayExtra("data");
            writeInboxMessage(data);
        }
    }

    private static boolean shouldParseContentDisposition() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return SmsManager.getDefault().getCarrierConfigValues().getBoolean(SmsManager.MMS_CONFIG_SUPPORT_MMS_CONTENT_DISPOSITION, true);
        } else {
            return false;
        }
    }

    private void writeInboxMessage(byte[] pushData) {
        final Context context = GeckoAppShell.getContext();
        final GenericPdu pdu = new PduParser(pushData, shouldParseContentDisposition()).parse();
        if (pdu == null) {
            Log.e(LOGTAG, "Invalid PUSH PDU");
        }
        final PduPersister persister = PduPersister.getPduPersister(context);
        final int type = pdu.getMessageType();
        try {
            switch (type) {
                case MESSAGE_TYPE_DELIVERY_IND:
                case MESSAGE_TYPE_READ_ORIG_IND: {
                    final long threadId = getDeliveryOrReadReportThreadId(context, pdu);
                    if (threadId == -1) {
                        // The associated SendReq isn't found, therefore skip
                        // processing this PDU.
                        Log.e(LOGTAG, "Failed to find delivery or read report's thread id");
                        break;
                    }
                    final Uri uri = persister.persist(
                            pdu,
                            Telephony.Mms.Inbox.CONTENT_URI,
                            true/*createThreadId*/,
                            true/*groupMmsEnabled*/,
                            null/*preOpenedFiles*/);
                    if (uri == null) {
                        Log.e(LOGTAG, "Failed to persist delivery or read report");
                        break;
                    }
                    // Update thread ID for ReadOrigInd & DeliveryInd.
                    final ContentValues values = new ContentValues(1);
                    values.put(Telephony.Mms.THREAD_ID, threadId);
                    if (SqliteWrapper.update(
                            context,
                            context.getContentResolver(),
                            uri,
                            values,
                            null/*where*/,
                            null/*selectionArgs*/) != 1) {
                        Log.e(LOGTAG, "Failed to update delivery or read report thread id");
                    }
                    break;
                }
                case MESSAGE_TYPE_NOTIFICATION_IND: {
                    final NotificationInd nInd = (NotificationInd) pdu;
                    if (!isDuplicateNotification(context, nInd)) {
                        final Uri uri = persister.persist(
                                pdu,
                                Telephony.Mms.Inbox.CONTENT_URI,
                                true/*createThreadId*/,
                                true/*groupMmsEnabled*/,
                                null/*preOpenedFiles*/);
                        if (uri == null) {
                            Log.e(LOGTAG, "Failed to save MMS WAP push notification ind");
                        }
                    } else {
                        Log.d(LOGTAG, "Skip storing duplicate MMS WAP push notification ind: "
                                + new String(nInd.getContentLocation()));
                    }
                    break;
                }
                default:
                    Log.e(LOGTAG, "Received unrecognized WAP Push PDU.");
            }
        } catch (MmsException e) {
            Log.e(LOGTAG, "Failed to save MMS WAP push data: type=" + type, e);
        } catch (RuntimeException e) {
            Log.e(LOGTAG, "Unexpected RuntimeException in persisting MMS WAP push data", e);
        }
    }

    private static final String THREAD_ID_SELECTION =
            Telephony.Mms.MESSAGE_ID + "=? AND " + Telephony.Mms.MESSAGE_TYPE + "=?";

    private static long getDeliveryOrReadReportThreadId(Context context, GenericPdu pdu) {
        String messageId;
        if (pdu instanceof DeliveryInd) {
            messageId = new String(((DeliveryInd) pdu).getMessageId());
        } else if (pdu instanceof ReadOrigInd) {
            messageId = new String(((ReadOrigInd) pdu).getMessageId());
        } else {
            Log.e(LOGTAG, "WAP Push data is neither delivery or read report type: "
                    + pdu.getClass().getCanonicalName());
            return -1L;
        }
        Cursor cursor = null;
        try {
            cursor = SqliteWrapper.query(
                    context,
                    context.getContentResolver(),
                    Telephony.Mms.CONTENT_URI,
                    new String[]{ Telephony.Mms.THREAD_ID },
                    THREAD_ID_SELECTION,
                    new String[]{
                            DatabaseUtils.sqlEscapeString(messageId),
                            Integer.toString(PduHeaders.MESSAGE_TYPE_SEND_REQ)
                    },
                    null/*sortOrder*/);
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.getLong(0);
            }
        } catch (SQLiteException e) {
            Log.e(LOGTAG, "Failed to query delivery or read report thread id", e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return -1L;
    }

    private static final String LOCATION_SELECTION =
            Telephony.Mms.MESSAGE_TYPE + "=? AND " + Telephony.Mms.CONTENT_LOCATION + " =?";

    private static boolean isDuplicateNotification(Context context, NotificationInd nInd) {
        final byte[] rawLocation = nInd.getContentLocation();
        if (rawLocation != null) {
            String location = new String(rawLocation);
            String[] selectionArgs = new String[] { location };
            Cursor cursor = null;
            try {
                cursor = SqliteWrapper.query(
                        context,
                        context.getContentResolver(),
                        Telephony.Mms.CONTENT_URI,
                        new String[]{Telephony.Mms._ID},
                        LOCATION_SELECTION,
                        new String[]{
                                Integer.toString(PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND),
                                new String(rawLocation)
                        },
                        null/*sortOrder*/);
                if (cursor != null && cursor.getCount() > 0) {
                    // We already received the same notification before.
                    return true;
                }
            } catch (SQLiteException e) {
                Log.e(LOGTAG, "failed to query existing notification ind", e);
            } finally {
                if (cursor != null) {
                    cursor.close();
                }
            }
        }
        return false;
    }
}
