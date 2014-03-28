/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.util.Log;

import static android.telephony.SmsMessage.MessageClass;

import java.util.ArrayList;

/**
 * This class is returning unique ids for PendingIntent requestCode attribute.
 * There are only |Integer.MAX_VALUE - Integer.MIN_VALUE| unique IDs available,
 * and they wrap around.
 */
class PendingIntentUID
{
  static private int sUID = Integer.MIN_VALUE;

  static public int generate() { return sUID++; }
}

/**
 * The envelope class contains all information that are needed to keep track of
 * a sent SMS.
 */
class Envelope
{
  enum SubParts {
    SENT_PART,
    DELIVERED_PART
  }

  protected int       mId;
  protected int       mMessageId;
  protected long      mMessageTimestamp;

  /**
   * Number of sent/delivered remaining parts.
   * @note The array has much slots as SubParts items.
   */
  protected int[]     mRemainingParts;

  /**
   * Whether sending/delivering is currently failing.
   * @note The array has much slots as SubParts items.
   */
  protected boolean[] mFailing;

  /**
   * Error type (only for sent).
   */
  protected int       mError;

  public Envelope(int aId, int aParts) {
    mId = aId;
    mMessageId = -1;
    mMessageTimestamp = 0;
    mError = GeckoSmsManager.kNoError;

    int size = Envelope.SubParts.values().length;
    mRemainingParts = new int[size];
    mFailing = new boolean[size];

    for (int i=0; i<size; ++i) {
      mRemainingParts[i] = aParts;
      mFailing[i] = false;
    }
  }

  public void decreaseRemainingParts(Envelope.SubParts aType) {
    --mRemainingParts[aType.ordinal()];

    if (mRemainingParts[SubParts.SENT_PART.ordinal()] >
        mRemainingParts[SubParts.DELIVERED_PART.ordinal()]) {
      Log.e("GeckoSmsManager", "Delivered more parts than we sent!?");
    }
  }

  public boolean arePartsRemaining(Envelope.SubParts aType) {
    return mRemainingParts[aType.ordinal()] != 0;
  }

  public void markAsFailed(Envelope.SubParts aType) {
    mFailing[aType.ordinal()] = true;
  }

  public boolean isFailing(Envelope.SubParts aType) {
    return mFailing[aType.ordinal()];
  }

  public int getMessageId() {
    return mMessageId;
  }

  public void setMessageId(int aMessageId) {
    mMessageId = aMessageId;
  }

  public long getMessageTimestamp() {
    return mMessageTimestamp;
  }

  public void setMessageTimestamp(long aMessageTimestamp) {
    mMessageTimestamp = aMessageTimestamp;
  }

  public int getError() {
    return mError;
  }

  public void setError(int aError) {
    mError = aError;
  }
}

/**
 * Postman class is a singleton that manages Envelope instances.
 */
class Postman
{
  public static final int kUnknownEnvelopeId = -1;

  private static final Postman sInstance = new Postman();

  private ArrayList<Envelope> mEnvelopes = new ArrayList<Envelope>(1);

  private Postman() {}

  public static Postman getInstance() {
    return sInstance;
  }

  public int createEnvelope(int aParts) {
    /*
     * We are going to create the envelope in the first empty slot in the array
     * list. If there is no empty slot, we create a new one.
     */
    int size = mEnvelopes.size();

    for (int i=0; i<size; ++i) {
      if (mEnvelopes.get(i) == null) {
        mEnvelopes.set(i, new Envelope(i, aParts));
        return i;
      }
    }

    mEnvelopes.add(new Envelope(size, aParts));
    return size;
  }

  public Envelope getEnvelope(int aId) {
    if (aId < 0 || mEnvelopes.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to get an unknown Envelope!");
      return null;
    }

    Envelope envelope = mEnvelopes.get(aId);
    if (envelope == null) {
      Log.e("GeckoSmsManager", "Trying to get an empty Envelope!");
    }

    return envelope;
  }

  public void destroyEnvelope(int aId) {
    if (aId < 0 || mEnvelopes.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to destroy an unknown Envelope!");
      return;
    }

    if (mEnvelopes.set(aId, null) == null) {
      Log.e("GeckoSmsManager", "Trying to destroy an empty Envelope!");
    }
  }
}

class SmsIOThread extends Thread {
  private final static SmsIOThread sInstance = new SmsIOThread();

  private Handler mHandler;

  public static SmsIOThread getInstance() {
    return sInstance;
  }

  public boolean execute(Runnable r) {
    return mHandler.post(r);
  }

  @Override
  public void run() {
    Looper.prepare();

    mHandler = new Handler();

    Looper.loop();
  }
}

class MessagesListManager
{
  private static final MessagesListManager sInstance = new MessagesListManager();

  public static MessagesListManager getInstance() {
    return sInstance;
  }

  private ArrayList<Cursor> mCursors = new ArrayList<Cursor>(0);

  public int add(Cursor aCursor) {
    int size = mCursors.size();

    for (int i=0; i<size; ++i) {
      if (mCursors.get(i) == null) {
        mCursors.set(i, aCursor);
        return i;
      }
    }

    mCursors.add(aCursor);
    return size;
  }

  public Cursor get(int aId) {
    if (aId < 0 || mCursors.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to get an unknown list!");
      return null;
    }

    Cursor cursor = mCursors.get(aId);
    if (cursor == null) {
      Log.e("GeckoSmsManager", "Trying to get an empty list!");
    }

    return cursor;
  }

  public void remove(int aId) {
    if (aId < 0 || mCursors.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to destroy an unknown list!");
      return;
    }

    Cursor cursor = mCursors.set(aId, null);
    if (cursor == null) {
      Log.e("GeckoSmsManager", "Trying to destroy an empty list!");
      return;
    }

    cursor.close();
  }

  public void clear() {
    for (int i=0; i<mCursors.size(); ++i) {
      Cursor c = mCursors.get(i);
      if (c != null) {
        c.close();
      }
    }

    mCursors.clear();
  }
}

public class GeckoSmsManager
  extends BroadcastReceiver
  implements ISmsManager
{
  public final static String ACTION_SMS_RECEIVED  = "android.provider.Telephony.SMS_RECEIVED";
  public final static String ACTION_SMS_SENT      = "org.mozilla.gecko.SMS_SENT";
  public final static String ACTION_SMS_DELIVERED = "org.mozilla.gecko.SMS_DELIVERED";

  /*
   * Make sure that the following error codes are in sync with |ErrorType| in:
   * dom/mobilemessage/src/Types.h
   * The error code are owned by the DOM.
   */
  public final static int kNoError               = 0;
  public final static int kNoSignalError         = 1;
  public final static int kNotFoundError         = 2;
  public final static int kUnknownError          = 3;
  public final static int kInternalError         = 4;
  public final static int kNoSimCardError        = 5;
  public final static int kRadioDisabledError    = 6;
  public final static int kInvalidAddressError   = 7;
  public final static int kFdnCheckError         = 8;
  public final static int kNonActiveSimCardError = 9;
  public final static int kStorageFullError      = 10;
  public final static int kSimNotMatchedError    = 11;

  private final static int kMaxMessageSize    = 160;

  private final static Uri kSmsContentUri     = Uri.parse("content://sms");
  private final static Uri kSmsSentContentUri = Uri.parse("content://sms/sent");

  private final static int kSmsTypeInbox      = 1;
  private final static int kSmsTypeSentbox    = 2;

  /*
   * Keep the following state codes in syng with |DeliveryState| in:
   * dom/mobilemessage/src/Types.h
   */
  private final static int kDeliveryStateSent          = 0;
  private final static int kDeliveryStateReceived      = 1;
  private final static int kDeliveryStateSending       = 2;
  private final static int kDeliveryStateError         = 3;
  private final static int kDeliveryStateUnknown       = 4;
  private final static int kDeliveryStateNotDownloaded = 5;
  private final static int kDeliveryStateEndGuard      = 6;

  /*
   * Keep the following status codes in sync with |DeliveryStatus| in:
   * dom/mobilemessage/src/Types.h
   */
  private final static int kDeliveryStatusNotApplicable = 0;
  private final static int kDeliveryStatusSuccess       = 1;
  private final static int kDeliveryStatusPending       = 2;
  private final static int kDeliveryStatusError         = 3;

  /*
   * android.provider.Telephony.Sms.STATUS_*. Duplicated because they're not
   * part of Android public API.
   */
  private final static int kInternalDeliveryStatusNone     = -1;
  private final static int kInternalDeliveryStatusComplete = 0;
  private final static int kInternalDeliveryStatusPending  = 32;
  private final static int kInternalDeliveryStatusFailed   = 64;

  /*
   * Keep the following values in sync with |MessageClass| in:
   * dom/mobilemessage/src/Types.h
   */
  private final static int kMessageClassNormal  = 0;
  private final static int kMessageClassClass0  = 1;
  private final static int kMessageClassClass1  = 2;
  private final static int kMessageClassClass2  = 3;
  private final static int kMessageClassClass3  = 4;

  private final static String[] kRequiredMessageRows = new String[] { "_id", "address", "body", "date", "type", "status" };

  public GeckoSmsManager() {
    SmsIOThread.getInstance().start();
  }

  @Override
  public void start() {
    IntentFilter smsFilter = new IntentFilter();
    smsFilter.addAction(GeckoSmsManager.ACTION_SMS_RECEIVED);
    smsFilter.addAction(GeckoSmsManager.ACTION_SMS_SENT);
    smsFilter.addAction(GeckoSmsManager.ACTION_SMS_DELIVERED);

    GeckoAppShell.getContext().registerReceiver(this, smsFilter);
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals(ACTION_SMS_RECEIVED)) {
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

        notifySmsReceived(msg.getDisplayOriginatingAddress(),
                          msg.getDisplayMessageBody(),
                          getGeckoMessageClass(msg.getMessageClass()),
                          System.currentTimeMillis());
      }

      return;
    }

    if (intent.getAction().equals(ACTION_SMS_SENT) ||
        intent.getAction().equals(ACTION_SMS_DELIVERED)) {
      Bundle bundle = intent.getExtras();

      if (bundle == null || !bundle.containsKey("envelopeId") ||
          !bundle.containsKey("number") || !bundle.containsKey("message") ||
          !bundle.containsKey("requestId")) {
        Log.e("GeckoSmsManager", "Got an invalid ACTION_SMS_SENT/ACTION_SMS_DELIVERED!");
        return;
      }

      int envelopeId = bundle.getInt("envelopeId");
      Postman postman = Postman.getInstance();

      Envelope envelope = postman.getEnvelope(envelopeId);
      if (envelope == null) {
        Log.e("GeckoSmsManager", "Got an invalid envelope id (or Envelope has been destroyed)!");
        return;
      }

      Envelope.SubParts part = intent.getAction().equals(ACTION_SMS_SENT)
                                 ? Envelope.SubParts.SENT_PART
                                 : Envelope.SubParts.DELIVERED_PART;
      envelope.decreaseRemainingParts(part);


      if (getResultCode() != Activity.RESULT_OK) {
        switch (getResultCode()) {
          case SmsManager.RESULT_ERROR_NULL_PDU:
            envelope.setError(kInternalError);
            break;
          case SmsManager.RESULT_ERROR_NO_SERVICE:
          case SmsManager.RESULT_ERROR_RADIO_OFF:
            envelope.setError(kNoSignalError);
            break;
          case SmsManager.RESULT_ERROR_GENERIC_FAILURE:
          default:
            envelope.setError(kUnknownError);
            break;
        }
        envelope.markAsFailed(part);
        Log.i("GeckoSmsManager", "SMS part sending failed!");
      }

      if (envelope.arePartsRemaining(part)) {
        return;
      }

      if (envelope.isFailing(part)) {
        if (part == Envelope.SubParts.SENT_PART) {
          notifySmsSendFailed(envelope.getError(), bundle.getInt("requestId"));
          Log.i("GeckoSmsManager", "SMS sending failed!");
        } else {
          notifySmsDelivery(envelope.getMessageId(),
                            kDeliveryStatusError,
                            bundle.getString("number"),
                            bundle.getString("message"),
                            envelope.getMessageTimestamp());
          Log.i("GeckoSmsManager", "SMS delivery failed!");
        }
      } else {
        if (part == Envelope.SubParts.SENT_PART) {
          String number = bundle.getString("number");
          String message = bundle.getString("message");
          long timestamp = System.currentTimeMillis();

          int id = saveSentMessage(number, message, timestamp);

          notifySmsSent(id, number, message, timestamp,
                        bundle.getInt("requestId"));

          envelope.setMessageId(id);
          envelope.setMessageTimestamp(timestamp);

          Log.i("GeckoSmsManager", "SMS sending was successfull!");
        } else {
          notifySmsDelivery(envelope.getMessageId(),
                            kDeliveryStatusSuccess,
                            bundle.getString("number"),
                            bundle.getString("message"),
                            envelope.getMessageTimestamp());
          Log.i("GeckoSmsManager", "SMS successfully delivered!");
        }
      }

      // Destroy the envelope object only if the SMS has been sent and delivered.
      if (!envelope.arePartsRemaining(Envelope.SubParts.SENT_PART) &&
          !envelope.arePartsRemaining(Envelope.SubParts.DELIVERED_PART)) {
        postman.destroyEnvelope(envelopeId);
      }

      return;
    }
  }

  @Override
  public void send(String aNumber, String aMessage, int aRequestId) {
    int envelopeId = Postman.kUnknownEnvelopeId;

    try {
      SmsManager sm = SmsManager.getDefault();

      Intent sentIntent = new Intent(ACTION_SMS_SENT);
      Intent deliveredIntent = new Intent(ACTION_SMS_DELIVERED);

      Bundle bundle = new Bundle();
      bundle.putString("number", aNumber);
      bundle.putString("message", aMessage);
      bundle.putInt("requestId", aRequestId);

      if (aMessage.length() <= kMaxMessageSize) {
        envelopeId = Postman.getInstance().createEnvelope(1);
        bundle.putInt("envelopeId", envelopeId);

        sentIntent.putExtras(bundle);
        deliveredIntent.putExtras(bundle);

        /*
         * There are a few things to know about getBroadcast and pending intents:
         * - the pending intents are in a shared pool maintained by the system;
         * - each pending intent is identified by a token;
         * - when a new pending intent is created, if it has the same token as
         *   another intent in the pool, one of them has to be removed.
         *
         * To prevent having a hard time because of this situation, we give a
         * unique id to all pending intents we are creating. This unique id is
         * generated by GetPendingIntentUID().
         */
        PendingIntent sentPendingIntent =
          PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                     PendingIntentUID.generate(), sentIntent,
                                     PendingIntent.FLAG_CANCEL_CURRENT);

        PendingIntent deliveredPendingIntent =
          PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                     PendingIntentUID.generate(), deliveredIntent,
                                     PendingIntent.FLAG_CANCEL_CURRENT);

        sm.sendTextMessage(aNumber, "", aMessage,
                           sentPendingIntent, deliveredPendingIntent);
      } else {
        ArrayList<String> parts = sm.divideMessage(aMessage);
        envelopeId = Postman.getInstance().createEnvelope(parts.size());
        bundle.putInt("envelopeId", envelopeId);

        sentIntent.putExtras(bundle);
        deliveredIntent.putExtras(bundle);

        ArrayList<PendingIntent> sentPendingIntents =
          new ArrayList<PendingIntent>(parts.size());
        ArrayList<PendingIntent> deliveredPendingIntents =
          new ArrayList<PendingIntent>(parts.size());

        for (int i=0; i<parts.size(); ++i) {
          sentPendingIntents.add(
            PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                       PendingIntentUID.generate(), sentIntent,
                                       PendingIntent.FLAG_CANCEL_CURRENT)
          );

          deliveredPendingIntents.add(
            PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                       PendingIntentUID.generate(), deliveredIntent,
                                       PendingIntent.FLAG_CANCEL_CURRENT)
          );
        }

        sm.sendMultipartTextMessage(aNumber, "", parts, sentPendingIntents,
                                    deliveredPendingIntents);
      }
    } catch (Exception e) {
      Log.e("GeckoSmsManager", "Failed to send an SMS: ", e);

      if (envelopeId != Postman.kUnknownEnvelopeId) {
        Postman.getInstance().destroyEnvelope(envelopeId);
      }

      notifySmsSendFailed(kUnknownError, aRequestId);
    }
  }

  public int saveSentMessage(String aRecipient, String aBody, long aDate) {
    try {
      ContentValues values = new ContentValues();
      values.put("address", aRecipient);
      values.put("body", aBody);
      values.put("date", aDate);
      // Always 'PENDING' because we always request status report.
      values.put("status", kInternalDeliveryStatusPending);

      ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
      Uri uri = cr.insert(kSmsSentContentUri, values);

      long id = ContentUris.parseId(uri);

      // The DOM API takes a 32bits unsigned int for the id. It's unlikely that
      // we happen to need more than that but it doesn't cost to check.
      if (id > Integer.MAX_VALUE) {
        throw new IdTooHighException();
      }

      return (int)id;
    } catch (IdTooHighException e) {
      Log.e("GeckoSmsManager", "The id we received is higher than the higher allowed value.");
      return -1;
    } catch (Exception e) {
      Log.e("GeckoSmsManager", "Something went wrong when trying to write a sent message", e);
      return -1;
    }
  }

  @Override
  public void getMessage(int aMessageId, int aRequestId) {
    class GetMessageRunnable implements Runnable {
      private int mMessageId;
      private int mRequestId;

      GetMessageRunnable(int aMessageId, int aRequestId) {
        mMessageId = aMessageId;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        Cursor cursor = null;

        try {
          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          Uri message = ContentUris.withAppendedId(kSmsContentUri, mMessageId);

          cursor = cr.query(message, kRequiredMessageRows, null, null, null);
          if (cursor == null || cursor.getCount() == 0) {
            throw new NotFoundException();
          }

          if (cursor.getCount() != 1) {
            throw new TooManyResultsException();
          }

          cursor.moveToFirst();

          if (cursor.getInt(cursor.getColumnIndex("_id")) != mMessageId) {
            throw new UnmatchingIdException();
          }

          int type = cursor.getInt(cursor.getColumnIndex("type"));
          int deliveryStatus;
          String sender = "";
          String receiver = "";

          if (type == kSmsTypeInbox) {
            deliveryStatus = kDeliveryStatusSuccess;
            sender = cursor.getString(cursor.getColumnIndex("address"));
          } else if (type == kSmsTypeSentbox) {
            deliveryStatus = getGeckoDeliveryStatus(cursor.getInt(cursor.getColumnIndex("status")));
            receiver = cursor.getString(cursor.getColumnIndex("address"));
          } else {
            throw new InvalidTypeException();
          }

          notifyGetSms(cursor.getInt(cursor.getColumnIndex("_id")),
                       deliveryStatus,
                       receiver, sender,
                       cursor.getString(cursor.getColumnIndex("body")),
                       cursor.getLong(cursor.getColumnIndex("date")),
                       mRequestId);
        } catch (NotFoundException e) {
          Log.i("GeckoSmsManager", "Message id " + mMessageId + " not found");
          notifyGetSmsFailed(kNotFoundError, mRequestId);
        } catch (UnmatchingIdException e) {
          Log.e("GeckoSmsManager", "Requested message id (" + mMessageId +
                                   ") is different from the one we got.");
          notifyGetSmsFailed(kUnknownError, mRequestId);
        } catch (TooManyResultsException e) {
          Log.e("GeckoSmsManager", "Get too many results for id " + mMessageId);
          notifyGetSmsFailed(kUnknownError, mRequestId);
        } catch (InvalidTypeException e) {
          Log.i("GeckoSmsManager", "Message has an invalid type, we ignore it.");
          notifyGetSmsFailed(kNotFoundError, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to get message", e);
          notifyGetSmsFailed(kUnknownError, mRequestId);
        } finally {
          if (cursor != null) {
            cursor.close();
          }
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new GetMessageRunnable(aMessageId, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add GetMessageRunnable to the SmsIOThread");
      notifyGetSmsFailed(kUnknownError, aRequestId);
    }
  }

  @Override
  public void deleteMessage(int aMessageId, int aRequestId) {
    class DeleteMessageRunnable implements Runnable {
      private int mMessageId;
      private int mRequestId;

      DeleteMessageRunnable(int aMessageId, int aRequestId) {
        mMessageId = aMessageId;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        try {
          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          Uri message = ContentUris.withAppendedId(kSmsContentUri, mMessageId);

          int count = cr.delete(message, null, null);

          if (count > 1) {
            throw new TooManyResultsException();
          }

          notifySmsDeleted(count == 1, mRequestId);
        } catch (TooManyResultsException e) {
          Log.e("GeckoSmsManager", "Delete more than one message?", e);
          notifySmsDeleteFailed(kUnknownError, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to delete a message", e);
          notifySmsDeleteFailed(kUnknownError, mRequestId);
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new DeleteMessageRunnable(aMessageId, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add GetMessageRunnable to the SmsIOThread");
      notifySmsDeleteFailed(kUnknownError, aRequestId);
    }
  }

  @Override
  public void createMessageList(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, int aDeliveryState, boolean aReverse, int aRequestId) {
    class CreateMessageListRunnable implements Runnable {
      private long     mStartDate;
      private long     mEndDate;
      private String[] mNumbers;
      private int      mNumbersCount;
      private int      mDeliveryState;
      private boolean  mReverse;
      private int      mRequestId;

      CreateMessageListRunnable(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, int aDeliveryState, boolean aReverse, int aRequestId) {
        mStartDate = aStartDate;
        mEndDate = aEndDate;
        mNumbers = aNumbers;
        mNumbersCount = aNumbersCount;
        mDeliveryState = aDeliveryState;
        mReverse = aReverse;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        Cursor cursor = null;
        boolean closeCursor = true;

        try {
          // TODO: should use the |selectionArgs| argument in |ContentResolver.query()|.
          ArrayList<String> restrictions = new ArrayList<String>();

          if (mStartDate != 0) {
            restrictions.add("date >= " + mStartDate);
          }

          if (mEndDate != 0) {
            restrictions.add("date <= " + mEndDate);
          }

          if (mNumbersCount > 0) {
            String numberRestriction = "address IN ('" + mNumbers[0] + "'";

            for (int i=1; i<mNumbersCount; ++i) {
              numberRestriction += ", '" + mNumbers[i] + "'";
            }
            numberRestriction += ")";

            restrictions.add(numberRestriction);
          }

          if (mDeliveryState == kDeliveryStateUnknown) {
            restrictions.add("type IN ('" + kSmsTypeSentbox + "', '" + kSmsTypeInbox + "')");
          } else if (mDeliveryState == kDeliveryStateSent) {
            restrictions.add("type = " + kSmsTypeSentbox);
          } else if (mDeliveryState == kDeliveryStateReceived) {
            restrictions.add("type = " + kSmsTypeInbox);
          } else {
            throw new UnexpectedDeliveryStateException();
          }

          String restrictionText = restrictions.size() > 0 ? restrictions.get(0) : "";

          for (int i=1; i<restrictions.size(); ++i) {
            restrictionText += " AND " + restrictions.get(i);
          }

          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          cursor = cr.query(kSmsContentUri, kRequiredMessageRows, restrictionText, null,
                            mReverse ? "date DESC" : "date ASC");

          if (cursor.getCount() == 0) {
            notifyNoMessageInList(mRequestId);
            return;
          }

          cursor.moveToFirst();

          int type = cursor.getInt(cursor.getColumnIndex("type"));
          int deliveryStatus;
          String sender = "";
          String receiver = "";

          if (type == kSmsTypeInbox) {
            deliveryStatus = kDeliveryStatusSuccess;
            sender = cursor.getString(cursor.getColumnIndex("address"));
          } else if (type == kSmsTypeSentbox) {
            deliveryStatus = getGeckoDeliveryStatus(cursor.getInt(cursor.getColumnIndex("status")));
            receiver = cursor.getString(cursor.getColumnIndex("address"));
          } else {
            throw new UnexpectedDeliveryStateException();
          }

          int listId = MessagesListManager.getInstance().add(cursor);
          closeCursor = false;
          notifyListCreated(listId,
                            cursor.getInt(cursor.getColumnIndex("_id")),
                            deliveryStatus,
                            receiver, sender,
                            cursor.getString(cursor.getColumnIndex("body")),
                            cursor.getLong(cursor.getColumnIndex("date")),
                            mRequestId);
        } catch (UnexpectedDeliveryStateException e) {
          Log.e("GeckoSmsManager", "Unexcepted delivery state type", e);
          notifyReadingMessageListFailed(kUnknownError, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to create a message list cursor", e);
          notifyReadingMessageListFailed(kUnknownError, mRequestId);
        } finally {
          // Close the cursor if MessagesListManager isn't taking care of it.
          // We could also just check if it is in the MessagesListManager list but
          // that would be less efficient.
          if (cursor != null && closeCursor) {
            cursor.close();
          }
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new CreateMessageListRunnable(aStartDate, aEndDate, aNumbers, aNumbersCount, aDeliveryState, aReverse, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add CreateMessageListRunnable to the SmsIOThread");
      notifyReadingMessageListFailed(kUnknownError, aRequestId);
    }
  }

  @Override
  public void getNextMessageInList(int aListId, int aRequestId) {
    class GetNextMessageInListRunnable implements Runnable {
      private int mListId;
      private int mRequestId;

      GetNextMessageInListRunnable(int aListId, int aRequestId) {
        mListId = aListId;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        try {
          Cursor cursor = MessagesListManager.getInstance().get(mListId);

          if (!cursor.moveToNext()) {
            MessagesListManager.getInstance().remove(mListId);
            notifyNoMessageInList(mRequestId);
            return;
          }

          int type = cursor.getInt(cursor.getColumnIndex("type"));
          int deliveryStatus;
          String sender = "";
          String receiver = "";

          if (type == kSmsTypeInbox) {
            deliveryStatus = kDeliveryStatusSuccess;
            sender = cursor.getString(cursor.getColumnIndex("address"));
          } else if (type == kSmsTypeSentbox) {
            deliveryStatus = getGeckoDeliveryStatus(cursor.getInt(cursor.getColumnIndex("status")));
            receiver = cursor.getString(cursor.getColumnIndex("address"));
          } else {
            throw new UnexpectedDeliveryStateException();
          }

          int listId = MessagesListManager.getInstance().add(cursor);
          notifyGotNextMessage(cursor.getInt(cursor.getColumnIndex("_id")),
                               deliveryStatus,
                               receiver, sender,
                               cursor.getString(cursor.getColumnIndex("body")),
                               cursor.getLong(cursor.getColumnIndex("date")),
                               mRequestId);
        } catch (UnexpectedDeliveryStateException e) {
          Log.e("GeckoSmsManager", "Unexcepted delivery state type", e);
          notifyReadingMessageListFailed(kUnknownError, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to get the next message of a list", e);
          notifyReadingMessageListFailed(kUnknownError, mRequestId);
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new GetNextMessageInListRunnable(aListId, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add GetNextMessageInListRunnable to the SmsIOThread");
      notifyReadingMessageListFailed(kUnknownError, aRequestId);
    }
  }

  @Override
  public void clearMessageList(int aListId) {
    MessagesListManager.getInstance().remove(aListId);
  }

  @Override
  public void stop() {
    GeckoAppShell.getContext().unregisterReceiver(this);
  }

  @Override
  public void shutdown() {
    SmsIOThread.getInstance().interrupt();
    MessagesListManager.getInstance().clear();
  }

  private int getGeckoDeliveryStatus(int aDeliveryStatus) {
    if (aDeliveryStatus == kInternalDeliveryStatusNone) {
      return kDeliveryStatusNotApplicable;
    }
    if (aDeliveryStatus >= kInternalDeliveryStatusFailed) {
      return kDeliveryStatusError;
    }
    if (aDeliveryStatus >= kInternalDeliveryStatusPending) {
      return kDeliveryStatusPending;
    }
    return kDeliveryStatusSuccess;
  }

  private int getGeckoMessageClass(MessageClass aMessageClass) {
    switch (aMessageClass) {
      case CLASS_0:
        return kMessageClassClass0;
      case CLASS_1:
        return kMessageClassClass1;
      case CLASS_2:
        return kMessageClassClass2;
      case CLASS_3:
        return kMessageClassClass3;
      default:
        return kMessageClassNormal;
    }
  }

  class IdTooHighException extends Exception {
    private static final long serialVersionUID = 29935575131092050L;
  }

  class InvalidTypeException extends Exception {
    private static final long serialVersionUID = 47436856832535912L;
  }

  class NotFoundException extends Exception {
    private static final long serialVersionUID = 1940676816633984L;
  }

  class TooManyResultsException extends Exception {
    private static final long serialVersionUID = 51883196784325305L;
  }

  class UnexpectedDeliveryStateException extends Exception {
    private static final long serialVersionUID = 494122763684005716L;
  }

  class UnmatchingIdException extends Exception {
    private static final long serialVersionUID = 158467542575633280L;
  }

  private static native void notifySmsReceived(String aSender, String aBody, int aMessageClass, long aTimestamp);
  private static native void notifySmsSent(int aId, String aReceiver, String aBody, long aTimestamp, int aRequestId);
  private static native void notifySmsDelivery(int aId, int aDeliveryStatus, String aReceiver, String aBody, long aTimestamp);
  private static native void notifySmsSendFailed(int aError, int aRequestId);
  private static native void notifyGetSms(int aId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
  private static native void notifyGetSmsFailed(int aError, int aRequestId);
  private static native void notifySmsDeleted(boolean aDeleted, int aRequestId);
  private static native void notifySmsDeleteFailed(int aError, int aRequestId);
  private static native void notifyNoMessageInList(int aRequestId);
  private static native void notifyListCreated(int aListId, int aMessageId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
  private static native void notifyGotNextMessage(int aMessageId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
  private static native void notifyReadingMessageListFailed(int aError, int aRequestId);
}
