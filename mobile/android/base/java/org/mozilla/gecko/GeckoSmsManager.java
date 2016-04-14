/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;

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
import android.os.HandlerThread;
import android.provider.Telephony;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.util.Log;

import static android.telephony.SmsMessage.MessageClass;
import static org.mozilla.gecko.SmsManager.ISmsManager;

import java.util.ArrayList;
import java.util.Formatter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

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
    mError = GeckoSmsManager.kNoError;

    int size = SubParts.values().length;
    mRemainingParts = new int[size];
    mFailing = new boolean[size];

    for (int i = 0; i < size; ++i) {
      mRemainingParts[i] = aParts;
      mFailing[i] = false;
    }
  }

  public void decreaseRemainingParts(SubParts aType) {
    --mRemainingParts[aType.ordinal()];

    if (mRemainingParts[SubParts.SENT_PART.ordinal()] >
        mRemainingParts[SubParts.DELIVERED_PART.ordinal()]) {
      Log.e("GeckoSmsManager", "Delivered more parts than we sent!?");
    }
  }

  public boolean arePartsRemaining(SubParts aType) {
    return mRemainingParts[aType.ordinal()] != 0;
  }

  public void markAsFailed(SubParts aType) {
    mFailing[aType.ordinal()] = true;
  }

  public boolean isFailing(SubParts aType) {
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

  private final ArrayList<Envelope> mEnvelopes = new ArrayList<>(1);

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

    for (int i = 0; i < size; ++i) {
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

class SmsIOThread extends HandlerThread {
  private final static SmsIOThread sInstance = new SmsIOThread();

  private Handler mHandler;

  public static SmsIOThread getInstance() {
    return sInstance;
  }

  SmsIOThread() {
    super("SmsIOThread");
  }

  @Override
  public void start() {
    super.start();
    mHandler = new Handler(getLooper());
  }

  public boolean execute(Runnable r) {
    return mHandler.post(r);
  }
}

class MessagesListManager
{
  private static final MessagesListManager sInstance = new MessagesListManager();

  public static MessagesListManager getInstance() {
    return sInstance;
  }

  private final HashMap<Integer, Cursor> mCursors = new HashMap<>();

  public void add(int id, Cursor aCursor) {
    if (mCursors.containsKey(id)) {
      Log.e("GeckoSmsManager", "Trying to overwrite cursor!");
      return;
    }

    mCursors.put(id, aCursor);
  }

  public Cursor get(int aId) {
    if (!mCursors.containsKey(aId)) {
      Log.e("GeckoSmsManager", "Cursor doesn't exist!");
      return null;
    }

    return mCursors.get(aId);
  }

  public void remove(int aId) {
    if (!mCursors.containsKey(aId)) {
      Log.e("GeckoSmsManager", "Cursor doesn't exist!");
      return;
    }

    mCursors.remove(aId);
  }

  public void clear() {
    Set<Map.Entry<Integer, Cursor>> entries = mCursors.entrySet();
    Iterator<Map.Entry<Integer, Cursor>> it = entries.iterator();
    while (it.hasNext()) {
      it.next().getValue().close();
    }

    mCursors.clear();
  }
}

public class GeckoSmsManager
  extends BroadcastReceiver
  implements ISmsManager
{
  public final static String ACTION_SMS_SENT      = "org.mozilla.gecko.SMS_SENT";
  public final static String ACTION_SMS_DELIVERED = "org.mozilla.gecko.SMS_DELIVERED";

  /*
   * Make sure that the following error codes are in sync with |ErrorType| in:
   * dom/mobilemessage/interfaces/nsIMobileMessageCallback.idl
   * The error codes are owned by the DOM.
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
  public final static int kNetworkProblemsError = 12;
  public final static int kGeneralProblemsError = 13;
  public final static int kServiceNotAvailableError      = 14;
  public final static int kMessageTooLongForNetworkError = 15;
  public final static int kServiceNotSupportedError      = 16;
  public final static int kRetryRequiredError   = 17;

  private final static int kMaxMessageSize    = 160;

  private final static Uri kSmsContentUri        = Telephony.Sms.Inbox.CONTENT_URI;
  private final static Uri kSmsSentContentUri    = Telephony.Sms.Sent.CONTENT_URI;
  private final static Uri kSmsThreadsContentUri = Telephony.Sms.Conversations.CONTENT_URI;

  private final static int kSmsTypeInbox      = Telephony.Sms.MESSAGE_TYPE_INBOX;
  private final static int kSmsTypeSentbox    = Telephony.Sms.MESSAGE_TYPE_SENT;

  /*
   * Keep the following state codes in syng with |DeliveryState| in:
   * dom/mobilemessage/Types.h
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
   * dom/mobilemessage/Types.h
   */
  private final static int kDeliveryStatusNotApplicable = 0;
  private final static int kDeliveryStatusSuccess       = 1;
  private final static int kDeliveryStatusPending       = 2;
  private final static int kDeliveryStatusError         = 3;

  /*
   * Keep the following values in sync with |MessageClass| in:
   * dom/mobilemessage/Types.h
   */
  private final static int kMessageClassNormal  = 0;
  private final static int kMessageClassClass0  = 1;
  private final static int kMessageClassClass1  = 2;
  private final static int kMessageClassClass2  = 3;
  private final static int kMessageClassClass3  = 4;

  private final static String[] kRequiredMessageRows = { "_id", "address", "body", "date", "type", "status", "read", "thread_id" };
  private final static String[] kRequiredMessageRowsForThread = { "_id", "address", "body", "read", "subject", "date" };
  private final static String[] kThreadProjection = { "thread_id" };

  // Used to generate monotonically increasing GUIDs.
  private static final AtomicInteger pendingIntentGuid = new AtomicInteger(Integer.MIN_VALUE);

  // The maximum value of a 32 bit signed integer. Used to enforce a limit on ids.
  private static final long UNSIGNED_INTEGER_MAX_VALUE = Integer.MAX_VALUE * 2L + 1L;

  public GeckoSmsManager() {
    if (SmsIOThread.getInstance().getState() == Thread.State.NEW) {
      SmsIOThread.getInstance().start();
    }
  }

  private boolean isDefaultSmsApp(Context context) {
    String myPackageName = context.getPackageName();
    return Telephony.Sms.getDefaultSmsPackage(context).equals(myPackageName);
  }

  @Override
  public void start() {
    IntentFilter smsFilter = new IntentFilter();
    smsFilter.addAction(Telephony.Sms.Intents.SMS_RECEIVED_ACTION);
    smsFilter.addAction(ACTION_SMS_SENT);
    smsFilter.addAction(ACTION_SMS_DELIVERED);

    GeckoAppShell.getContext().registerReceiver(this, smsFilter);
  }

  /**
   * Build up the SMS message body from the SmsMessage array of received SMS
   *
   * @param msgs The SmsMessage array of the received SMS
   * @return The text message body
   */
  private static String buildMessageBodyFromPdus(SmsMessage[] msgs) {
    if (msgs.length == 1) {
      // There is only one part, so grab the body directly.
      return replaceFormFeeds(msgs[0].getDisplayMessageBody());
    } else {
      // Build up the body from the parts.
      StringBuilder body = new StringBuilder();
      for (SmsMessage msg : msgs) {
        // getDisplayMessageBody() can NPE if mWrappedMessage inside is null.
        body.append(msg.getDisplayMessageBody());
      }
      return replaceFormFeeds(body.toString());
    }
  }

  // Some providers send formfeeds in their messages. Convert those formfeeds to newlines.
  private static String replaceFormFeeds(String s) {
    return s == null ? "" : s.replace('\f', '\n');
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals(Telephony.Sms.Intents.SMS_DELIVER_ACTION) ||
        intent.getAction().equals(Telephony.Sms.Intents.SMS_RECEIVED_ACTION)) {
      // If we're the default SMS, ignore SMS_RECEIVED intents since we'll handle
      // the SMS_DELIVER intent instead.
      if (isDefaultSmsApp(GeckoAppShell.getContext()) && intent.getAction().equals(Telephony.Sms.Intents.SMS_RECEIVED_ACTION)) {
        return;
      }

      // TODO: Try to find the receiver number to be able to populate
      //       SmsMessage.receiver.
      SmsMessage[] messages = Telephony.Sms.Intents.getMessagesFromIntent(intent);
      if (messages == null || messages.length == 0) {
        return;
      }

      SmsMessage sms = messages[0];
      String body = buildMessageBodyFromPdus(messages);
      long timestamp = System.currentTimeMillis();

      int id = 0;
      // We only need to save the message if we're the default SMS app
      if (intent.getAction().equals(Telephony.Sms.Intents.SMS_DELIVER_ACTION)) {
        id = saveReceivedMessage(context,
                               sms.getDisplayOriginatingAddress(),
                               body,
                               sms.getTimestampMillis(),
                               timestamp,
                               sms.getPseudoSubject());
      }

      notifySmsReceived(id,
                        sms.getDisplayOriginatingAddress(),
                        body,
                        getGeckoMessageClass(sms.getMessageClass()),
                        sms.getTimestampMillis(),
                        timestamp);

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
          if (bundle.getBoolean("shouldNotify")) {
            notifySmsSendFailed(envelope.getError(), bundle.getInt("requestId"));
          }
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

          // save message only if we're default SMS app, otherwise sendTextMessage does it for us
          int id = 0;
          if (isDefaultSmsApp(GeckoAppShell.getContext())) {
            id = saveSentMessage(number, message, timestamp);
          }

          if (bundle.getBoolean("shouldNotify")) {
            notifySmsSent(id, number, message, timestamp, bundle.getInt("requestId"));
          }

          envelope.setMessageId(id);
          envelope.setMessageTimestamp(timestamp);

          Log.i("GeckoSmsManager", "SMS sending was successful!");
        } else {
          Uri id = ContentUris.withAppendedId(kSmsContentUri, envelope.getMessageId());
          updateMessageStatus(id, Telephony.Sms.STATUS_COMPLETE);

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
    }
  }

  @Override
  public void send(String aNumber, String aMessage, int aRequestId, boolean aShouldNotify) {
    int envelopeId = Postman.kUnknownEnvelopeId;

    try {
      SmsManager sm = SmsManager.getDefault();

      Intent sentIntent = new Intent(GeckoAppShell.getContext(), GeckoSmsManager.class);
      sentIntent.setAction(ACTION_SMS_SENT);
      Intent deliveredIntent = new Intent(GeckoAppShell.getContext(), GeckoSmsManager.class);
      deliveredIntent.setAction(ACTION_SMS_DELIVERED);

      Bundle bundle = new Bundle();
      bundle.putString("number", aNumber);
      bundle.putString("message", aMessage);
      bundle.putInt("requestId", aRequestId);
      bundle.putBoolean("shouldNotify", aShouldNotify);

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
                                     pendingIntentGuid.incrementAndGet(), sentIntent,
                                     PendingIntent.FLAG_CANCEL_CURRENT);

        PendingIntent deliveredPendingIntent =
          PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                     pendingIntentGuid.incrementAndGet(), deliveredIntent,
                                     PendingIntent.FLAG_CANCEL_CURRENT);

        sm.sendTextMessage(aNumber, null, aMessage,
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

        for (int i = 0; i < parts.size(); ++i) {
          sentPendingIntents.add(
            PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                       pendingIntentGuid.incrementAndGet(), sentIntent,
                                       PendingIntent.FLAG_CANCEL_CURRENT)
          );

          deliveredPendingIntents.add(
            PendingIntent.getBroadcast(GeckoAppShell.getContext(),
                                       pendingIntentGuid.incrementAndGet(), deliveredIntent,
                                       PendingIntent.FLAG_CANCEL_CURRENT)
          );
        }

        sm.sendMultipartTextMessage(aNumber, null, parts, sentPendingIntents,
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
      values.put(Telephony.Sms.ADDRESS, aRecipient);
      values.put(Telephony.Sms.BODY, aBody);
      values.put(Telephony.Sms.DATE, aDate);
      // Always 'PENDING' because we always request status report.
      values.put(Telephony.Sms.STATUS, Telephony.Sms.STATUS_PENDING);
      values.put(Telephony.Sms.SEEN, 0);
      values.put(Telephony.Sms.READ, 0);

      ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
      Uri uri = cr.insert(kSmsSentContentUri, values);

      long id = ContentUris.parseId(uri);

      // The DOM API takes a 32bits unsigned int for the id. It's unlikely that
      // we happen to need more than that but it doesn't cost to check.
      if (id > UNSIGNED_INTEGER_MAX_VALUE) {
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

  public void updateMessageStatus(Uri aId, int aStatus) {
    ContentValues values = new ContentValues(1);
    values.put(Telephony.Sms.STATUS, aStatus);

    ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
    if (cr.update(aId, values, null, null) != 1) {
      Log.i("GeckoSmsManager", "Failed to update message status!");
    }
  }

  public int saveReceivedMessage(Context aContext, String aSender, String aBody, long aDateSent, long aDate, String aSubject) {
    ContentValues values = new ContentValues();
    values.put(Telephony.Sms.Inbox.ADDRESS, aSender);
    values.put(Telephony.Sms.Inbox.BODY, aBody);
    values.put(Telephony.Sms.Inbox.DATE_SENT, aDateSent);
    values.put(Telephony.Sms.Inbox.DATE, aDate);
    values.put(Telephony.Sms.Inbox.STATUS, Telephony.Sms.STATUS_COMPLETE);
    values.put(Telephony.Sms.Inbox.READ, 0);
    values.put(Telephony.Sms.Inbox.SEEN, 0);

    ContentResolver cr = aContext.getContentResolver();
    Uri uri = cr.insert(kSmsContentUri, values);

    long id = ContentUris.parseId(uri);

    // The DOM API takes a 32bits unsigned int for the id. It's unlikely that
    // we happen to need more than that but it doesn't cost to check.
    if (id > UNSIGNED_INTEGER_MAX_VALUE) {
      Log.i("GeckoSmsManager", "The id we received is higher than the higher allowed value.");
      return -1;
    }

    return (int)id;
  }

  @Override
  public void getMessage(int aMessageId, int aRequestId) {
    class GetMessageRunnable implements Runnable {
      private final int mMessageId;
      private final int mRequestId;

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

          boolean read = cursor.getInt(cursor.getColumnIndex("read")) != 0;

          notifyGetSms(cursor.getInt(cursor.getColumnIndex("_id")),
                       deliveryStatus,
                       receiver, sender,
                       cursor.getString(cursor.getColumnIndex("body")),
                       cursor.getLong(cursor.getColumnIndex("date")),
                       read,
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
      private final int mMessageId;
      private final int mRequestId;

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

  private void getMessageFromCursorAndNotify(Cursor aCursor, int aRequestId) throws Exception {
    int type = aCursor.getInt(aCursor.getColumnIndex("type"));
    int deliveryStatus = kDeliveryStateUnknown;
    String sender = "";
    String receiver = "";

    if (type == kSmsTypeInbox) {
      deliveryStatus = kDeliveryStatusSuccess;
      sender = aCursor.getString(aCursor.getColumnIndex("address"));
    } else if (type == kSmsTypeSentbox) {
      deliveryStatus = getGeckoDeliveryStatus(aCursor.getInt(aCursor.getColumnIndex("status")));
      receiver = aCursor.getString(aCursor.getColumnIndex("address"));
    } else {
      throw new Exception("Shouldn't ever get a message here that's not from inbox or sentbox");
    }

    boolean read = aCursor.getInt(aCursor.getColumnIndex("read")) != 0;

    notifyMessageCursorResult(aCursor.getInt(aCursor.getColumnIndex("_id")),
                              deliveryStatus,
                              receiver, sender,
                              aCursor.getString(aCursor.getColumnIndex("body")),
                              aCursor.getLong(aCursor.getColumnIndex("date")),
                              aCursor.getLong(aCursor.getColumnIndex("thread_id")),
                              read,
                              aRequestId);
  }

  @Override
  public void markMessageRead(int aMessageId, boolean aValue, boolean aSendReadReport, int aRequestId) {
    class MarkMessageReadRunnable implements Runnable {
      private final int mMessageId;
      private final boolean mValue;
      private final int mRequestId;

      MarkMessageReadRunnable(int aMessageId, boolean aValue, int aRequestId) {
        mMessageId = aMessageId;
        mValue = aValue;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        try {
          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          Uri message = ContentUris.withAppendedId(kSmsContentUri, mMessageId);

          ContentValues updatedProps = new ContentValues();
          updatedProps.put("read", mValue);

          int count = cr.update(message, updatedProps, null, null);

          notifySmsMarkedAsRead(count == 1, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to mark message as read: " + e);
          notifySmsMarkAsReadFailed(kUnknownError, mRequestId);
        }
      }
    }

    if (aSendReadReport == true) {
      Log.w("GeckoSmsManager", "Android SmsManager doesn't suport read receipts for SMS.");
    }

    if (!SmsIOThread.getInstance().execute(new MarkMessageReadRunnable(aMessageId, aValue, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add MarkMessageReadRunnable to the SmsIOThread");
      notifySmsMarkAsReadFailed(kUnknownError, aRequestId);
    }
  }

  @Override
  public void createMessageCursor(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, String aDelivery, boolean aHasRead, boolean aRead, boolean aHasThreadId, long aThreadId, boolean aReverse, int aRequestId) {
    class CreateMessageCursorRunnable implements Runnable {
      private final long     mStartDate;
      private final long     mEndDate;
      private final String[] mNumbers;
      private final int      mNumbersCount;
      private final String   mDelivery;
      private final boolean  mHasThreadId;
      private final long     mThreadId;
      private final boolean  mReverse;
      private final int      mRequestId;

      CreateMessageCursorRunnable(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, String aDelivery, boolean aHasRead, boolean aRead, boolean aHasThreadId, long aThreadId, boolean aReverse, int aRequestId) {
        mStartDate = aStartDate;
        mEndDate = aEndDate;
        mNumbers = aNumbers;
        mNumbersCount = aNumbersCount;
        mDelivery = aDelivery;
        mHasThreadId = aHasThreadId;
        mThreadId = aThreadId;
        mReverse = aReverse;
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        Cursor cursor = null;
        boolean closeCursor = true;

        try {
          StringBuilder restrictions = new StringBuilder();
          Formatter formatter = new Formatter(restrictions);

          if (mStartDate >= 0) {
            formatter.format("date >= '%d' AND ", mStartDate);
          }

          if (mEndDate >= 0) {
            formatter.format("date <= '%d' AND ", mEndDate);
          }

          if (mNumbersCount > 0) {
            formatter.format("address IN ('%s'", mNumbers[0]);

            for (int i = 1; i < mNumbersCount; ++i) {
              formatter.format(", '%s'", mNumbers[i]);
            }

            formatter.format(") AND ");
          }

          if (mDelivery == null || mDelivery.isEmpty()) {
            formatter.format("type IN ('%d', '%d') AND ", kSmsTypeSentbox, kSmsTypeInbox);
          } else if (mDelivery.equals("sent")) {
            formatter.format("type = '%d' AND ", kSmsTypeSentbox);
          } else if (mDelivery.equals("received")) {
            formatter.format("type = '%d' AND ", kSmsTypeInbox);
          } else {
            throw new Exception("Unexpected delivery state: " + mDelivery);
          }

          if (mHasThreadId) {
            formatter.format("thread_id = '%d' AND ", mThreadId);
          }

          // Final 'AND 1' is a no-op so we don't have to special case the last
          // condition.
          formatter.format("1");

          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          cursor = cr.query(kSmsContentUri,
                            kRequiredMessageRows,
                            restrictions.toString(),
                            null,
                            mReverse ? "date DESC" : "date ASC");

          if (cursor.getCount() == 0) {
            notifyCursorDone(mRequestId);
            return;
          }

          MessagesListManager.getInstance().add(mRequestId, cursor);

          cursor.moveToFirst();
          getMessageFromCursorAndNotify(cursor, mRequestId);
          closeCursor = false;
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to create a message list cursor", e);
          notifyCursorError(kUnknownError, mRequestId);
        } finally {
          if (closeCursor && cursor != null) {
            cursor.close();
          }
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new CreateMessageCursorRunnable(aStartDate, aEndDate, aNumbers, aNumbersCount, aDelivery, aHasRead, aRead, aHasThreadId, aThreadId, aReverse, aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add CreateMessageCursorRunnable to the SmsIOThread");
      notifyCursorError(kUnknownError, aRequestId);
    }
  }

  @Override
  public void getNextMessage(int aRequestId) {
    class GetNextMessageRunnable implements Runnable {
      private final int mRequestId;

      GetNextMessageRunnable(int aRequestId) {
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        Cursor cursor = null;
        boolean closeCursor = true;
        try {
          cursor = MessagesListManager.getInstance().get(mRequestId);

          if (!cursor.moveToNext()) {
            MessagesListManager.getInstance().remove(mRequestId);
            notifyCursorDone(mRequestId);
            return;
          }

          getMessageFromCursorAndNotify(cursor, mRequestId);
          closeCursor = false;
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to get the next message of a list", e);
          notifyCursorError(kUnknownError, mRequestId);
        } finally {
          if (closeCursor) {
            cursor.close();
          }
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new GetNextMessageRunnable(aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add GetNextMessageRunnable to the SmsIOThread");
      notifyCursorError(kUnknownError, aRequestId);
    }
  }

  private void getThreadFromCursorAndNotify(Cursor aCursor, int aRequestId) throws Exception {
    ContentResolver cr = GeckoAppShell.getContext().getContentResolver();

    long id = aCursor.getLong(aCursor.getColumnIndex("thread_id"));
    Cursor msgCursor = cr.query(kSmsContentUri,
                                kRequiredMessageRowsForThread,
                                "thread_id = " + id,
                                null,
                                "date DESC");

    if (msgCursor == null || msgCursor.getCount() == 0) {
      throw new Exception("Empty thread " + id);
    }

    msgCursor.moveToFirst();

    String lastMessageSubject = msgCursor.getString(msgCursor.getColumnIndex("subject"));
    String body = msgCursor.getString(msgCursor.getColumnIndex("body"));
    long timestamp = msgCursor.getLong(msgCursor.getColumnIndex("date"));

    HashSet<String> participants = new HashSet<>();
    do {
      String p = msgCursor.getString(msgCursor.getColumnIndex("address"));
      participants.add(p);
    } while (msgCursor.moveToNext());

    //TODO: handle MMS
    String lastMessageType = "sms";

    msgCursor = cr.query(kSmsContentUri,
                         kRequiredMessageRowsForThread,
                         "thread_id = " + id + " AND read = 0",
                         null,
                         null);

    if (msgCursor == null) {
      Log.e("GeckoSmsManager", "We should never get here, should have errored before");
      throw new Exception("Empty thread " + id);
    }

    long unreadCount = msgCursor.getCount();

    notifyThreadCursorResult(id, lastMessageSubject, body, unreadCount, participants.toArray(), timestamp, lastMessageType, aRequestId);
  }

  @Override
  public void createThreadCursor(int aRequestId) {
    class CreateThreadCursorRunnable implements Runnable {
      private final int mRequestId;

      CreateThreadCursorRunnable(int aRequestId) {
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        try {
          ContentResolver cr = GeckoAppShell.getContext().getContentResolver();
          Cursor cursor = cr.query(kSmsThreadsContentUri,
                                   kThreadProjection,
                                   null,
                                   null,
                                   "date DESC");
          if (cursor == null || !cursor.moveToFirst()) {
            notifyCursorDone(mRequestId);
            return;
          }

          MessagesListManager.getInstance().add(mRequestId, cursor);

          getThreadFromCursorAndNotify(cursor, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to create thread cursor: " + e);
          notifyCursorError(kUnknownError, mRequestId);
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new CreateThreadCursorRunnable(aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add CreateThreadCursorRunnable to the SmsIOThread");
      notifyCursorError(kUnknownError, aRequestId);
    }
  }

  @Override
  public void getNextThread(int aRequestId) {
    class GetNextThreadRunnable implements Runnable {
      private final int mRequestId;

      GetNextThreadRunnable(int aRequestId) {
        mRequestId = aRequestId;
      }

      @Override
      public void run() {
        try {
          Cursor cursor = MessagesListManager.getInstance().get(mRequestId);

          if (!cursor.moveToNext()) {
            MessagesListManager.getInstance().remove(mRequestId);
            notifyCursorDone(mRequestId);
            return;
          }

          getThreadFromCursorAndNotify(cursor, mRequestId);
        } catch (Exception e) {
          Log.e("GeckoSmsManager", "Error while trying to create thread cursor: " + e);
          notifyCursorError(kUnknownError, mRequestId);
        }
      }
    }

    if (!SmsIOThread.getInstance().execute(new GetNextThreadRunnable(aRequestId))) {
      Log.e("GeckoSmsManager", "Failed to add GetNextThreadRunnable to the SmsIOThread");
      notifyCursorError(kUnknownError, aRequestId);
    }
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
    if (aDeliveryStatus == Telephony.Sms.STATUS_NONE) {
      return kDeliveryStatusNotApplicable;
    }
    if (aDeliveryStatus >= Telephony.Sms.STATUS_FAILED) {
      return kDeliveryStatusError;
    }
    if (aDeliveryStatus >= Telephony.Sms.STATUS_PENDING) {
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

  static class IdTooHighException extends Exception {
    private static final long serialVersionUID = 29935575131092050L;
  }

  static class InvalidTypeException extends Exception {
    private static final long serialVersionUID = 47436856832535912L;
  }

  static class NotFoundException extends Exception {
    private static final long serialVersionUID = 1940676816633984L;
  }

  static class TooManyResultsException extends Exception {
    private static final long serialVersionUID = 51883196784325305L;
  }

  static class UnmatchingIdException extends Exception {
    private static final long serialVersionUID = 158467542575633280L;
  }

  @WrapForJNI
  public static native void notifySmsReceived(int aId, String aSender, String aBody, int aMessageClass, long aSentTimestamp, long aTimestamp);
  @WrapForJNI
  private static native void notifySmsSent(int aId, String aReceiver, String aBody, long aTimestamp, int aRequestId);
  @WrapForJNI
  private static native void notifySmsDelivery(int aId, int aDeliveryStatus, String aReceiver, String aBody, long aTimestamp);
  @WrapForJNI
  private static native void notifySmsSendFailed(int aError, int aRequestId);
  @WrapForJNI
  private static native void notifyGetSms(int aId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, boolean aRead, int aRequestId);
  @WrapForJNI
  private static native void notifyGetSmsFailed(int aError, int aRequestId);
  @WrapForJNI
  private static native void notifySmsDeleted(boolean aDeleted, int aRequestId);
  @WrapForJNI
  private static native void notifySmsDeleteFailed(int aError, int aRequestId);
  @WrapForJNI
  private static native void notifySmsMarkedAsRead(boolean aMarkedAsRead, int aRequestId);
  @WrapForJNI
  private static native void notifySmsMarkAsReadFailed(int aError, int aRequestId);
  @WrapForJNI
  private static native void notifyCursorError(int aError, int aRequestId);
  @WrapForJNI
  private static native void notifyThreadCursorResult(long aId, String aLastMessageSubject, String aBody, long aUnreadCount, Object[] aParticipants, long aTimestamp, String aLastMessageType, int aRequestId);
  @WrapForJNI
  private static native void notifyMessageCursorResult(int aMessageId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, long aThreadId, boolean aRead, int aRequestId);
  @WrapForJNI
  private static native void notifyCursorDone(int aRequestId);
}
