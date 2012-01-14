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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chenxia Liu <liuche@mozilla.com>
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

package org.mozilla.gecko.sync.jpake;

import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.CryptoInfo;
import org.mozilla.gecko.sync.crypto.Cryptographer;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.NoKeyBundleException;
import org.mozilla.gecko.sync.net.ResourceDelegate;
import org.mozilla.gecko.sync.net.SyncResourceDelegate;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import android.util.Log;
import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.entity.StringEntity;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class JPakeClient implements JPakeRequestDelegate {
  private static String       LOG_TAG                 = "JPakeClient";

  // J-PAKE constants.
  private final static int    REQUEST_TIMEOUT         = 60 * 1000;         // 1
                                                                            // minute
  private final static int    SOCKET_TIMEOUT          = 5 * 60 * 1000;     // 5
                                                                            // minutes
  private final static int    KEYEXCHANGE_VERSION     = 3;

  private final static String JPAKE_SIGNERID_SENDER   = "sender";
  private final static String JPAKE_SIGNERID_RECEIVER = "receiver";
  private final static int    JPAKE_LENGTH_SECRET     = 8;
  private final static int    JPAKE_LENGTH_CLIENTID   = 256;
  private final static String JPAKE_VERIFY_VALUE      = "0123456789ABCDEF";

  private final static int    MAX_TRIES               = 10;
  private final static int    MAX_TRIES_FIRST_MSG     = 300;
  private final static int    MAX_TRIES_LAST_MSG      = 300;

  // UI Controller.
  private SetupSyncActivity   ssActivity;

  // J-PAKE session values.
  private String              clientId;
  private String              secret;

  private String              myEtag;
  private String              mySignerId;
  private String              theirEtag;
  private String              theirSignerId;
  private String              jpakeServer;

  // J-PAKE state.
  private boolean             pairWithPin;
  private boolean             paired                  = false;
  private boolean             finished                = false;
  private State               state                   = State.GET_CHANNEL;
  private State               stateContext;
  private String              error;
  private int                 pollTries               = 0;

  // J-PAKE values.
  private int                 jpakePollInterval;
  private int                 jpakeMaxTries;
  private String              channel;
  private String              channelUrl;

  // J-PAKE delayed-task scheduler. Used for timing.
  private Timer               timerScheduler;

  // J-PAKE session data.
  private KeyBundle           myKeyBundle;

  private ExtendedJSONObject  jOutgoing;
  private ExtendedJSONObject  jIncoming;

  private JPakeParty          jParty;
  private JPakeNumGenerator   numGen;

  public JPakeClient(SetupSyncActivity activity) {
    ssActivity = activity;

    // Set JPAKE params from prefs
    // TODO: remove hardcoding
    jpakeServer = "https://setup.services.mozilla.com/";
    jpakePollInterval = 1 * 1000; // 1 second
    jpakeMaxTries = MAX_TRIES;

    if (!jpakeServer.endsWith("/")) {
      jpakeServer += "/";
    }

    timerScheduler = new Timer();

    setClientId();
  }

  /**
   * (Receiver Only)
   *
   * Initiate pairing and receive data without providing a PIN. The PIN will be
   * generated and passed on to the controller to be displayed to the user.
   *
   * Starts JPAKE protocol.
   */
  public void receiveNoPin() {
    mySignerId = JPAKE_SIGNERID_RECEIVER;
    theirSignerId = JPAKE_SIGNERID_SENDER;
    pairWithPin = false;

    // TODO: fetch from prefs
    jpakeMaxTries = MAX_TRIES_FIRST_MSG;

    jParty = new JPakeParty(mySignerId);
    numGen = new JPakeNumGeneratorRandom();

    final JPakeClient self = this;
    runOnThread(new Runnable() {
      @Override
      public void run() {
        self.createSecret();
        self.getChannel();
      }
    });
  }

  /**
   * (Sender Only)
   *
   * Pairing using PIN provided on other device. Functionality available only
   * when a Sync account has already been set up.
   *
   * @param pin
   *          12-character string containing PIN entered by the user.
   * @param expectDelay
   *          Flag that indicates that a significant delay between pairing and
   *          sending shoudl be expected. v2 and earlier of the protocol did not
   *          allow for this, and the pairing to a v2 or earlier client will be
   *          aborted if this flag is 'true'
   */
  public void pairWithPin(String pin, boolean expectDelay) {
    mySignerId = JPAKE_SIGNERID_SENDER;
    theirSignerId = JPAKE_SIGNERID_RECEIVER;
    pairWithPin = true;

    // Extract secret and server channel.
    secret = pin.substring(0, JPAKE_LENGTH_SECRET);
    channel = pin.substring(JPAKE_LENGTH_SECRET);
    channelUrl = jpakeServer + channel;

    jParty = new JPakeParty(mySignerId);
    numGen = new JPakeNumGeneratorRandom();

    final JPakeClient self = this;
    runOnThread(new Runnable() {
      @Override
      public void run() {
        // Make initial GET request for first round data.
        self.state = State.SNDR_STEP_ZERO;
        scheduleGetRequest(jpakePollInterval);
      }
    });
  }

  private static void runOnThread(Runnable run) {
    ThreadPool.run(run);
  }

  public enum State {
    GET_CHANNEL, SNDR_STEP_ZERO, SNDR_STEP_ONE, SNDR_STEP_TWO, RCVR_STEP_ONE, RCVR_STEP_TWO, PUT, ABORT, ENCRYPT_PUT, REPORT_FAILURE, VERIFY_KEY, VERIFY_PAIRING;
  }

  /* Main functionality Steps */

  /*
   * (Receiver Only) Request channel for J-PAKE from server.
   */
  private void getChannel() {
    Log.d(LOG_TAG, "Getting channel.");
    if (finished)
      return;

    JPakeRequest channelRequest = null;
    try {
      channelRequest = new JPakeRequest(jpakeServer + "new_channel",
          makeRequestResourceDelegate());
    } catch (URISyntaxException e) {
      e.printStackTrace();
      abort(Constants.JPAKE_ERROR_CHANNEL);
      return;
    }
    channelRequest.get();
  }

  /*
   * Helper for sending a PUT request to server, with data taken from shared
   * jOutgoing JSONObject.
   */
  private void putStep() {
    Log.d(LOG_TAG, "Uploading message.");
    runOnThread(new Runnable() {
      @Override
      public void run() {
        JPakeRequest putRequest = null;
        try {
          putRequest = new JPakeRequest(channelUrl,
              makeRequestResourceDelegate());
        } catch (URISyntaxException e) {
          e.printStackTrace();
          abort(Constants.JPAKE_ERROR_CHANNEL);
          return;
        }
        try {
          putRequest.put(jsonEntity(jOutgoing.object));
        } catch (UnsupportedEncodingException e) {
          e.printStackTrace();
        }
        Log.d(LOG_TAG, "outgoing: " + jOutgoing.toJSONString());
      }
    });
  }

  /*
   * Step One of J-PAKE protocol.
   */
  private void computeStepOne() {
    Log.d(LOG_TAG, "Computing round 1.");

    JPakeCrypto.round1(jParty, numGen);

    // Set outgoing message.
    ExtendedJSONObject jOne = new ExtendedJSONObject();
    jOne.put(Constants.ZKP_KEY_GX1,
        BigIntegerHelper.toEvenLengthHex(jParty.gx1));
    jOne.put(Constants.ZKP_KEY_GX2,
        BigIntegerHelper.toEvenLengthHex(jParty.gx2));

    Zkp zkp1 = jParty.zkp1;
    Zkp zkp2 = jParty.zkp2;
    ExtendedJSONObject jZkp1 = makeJZkp(zkp1.gr, zkp1.b, mySignerId);
    ExtendedJSONObject jZkp2 = makeJZkp(zkp2.gr, zkp2.b, mySignerId);

    jOne.put(Constants.ZKP_KEY_ZKP_X1, jZkp1);
    jOne.put(Constants.ZKP_KEY_ZKP_X2, jZkp2);

    jOutgoing = new ExtendedJSONObject();
    jOutgoing.put(Constants.JSON_KEY_TYPE, mySignerId + "1");
    jOutgoing.put(Constants.JSON_KEY_PAYLOAD, jOne);
    jOutgoing.put(Constants.JSON_KEY_VERSION, KEYEXCHANGE_VERSION);
    Log.d(LOG_TAG, "Sending: " + jOutgoing.toJSONString());

    // Store context to determine next step after PUT request.
    stateContext = pairWithPin ? State.SNDR_STEP_ONE : State.RCVR_STEP_ONE;
    state = State.PUT;
    putStep();
  }

  /*
   * Step Two of J-PAKE protocol.
   *
   * Verifies message computed by other party in their Step One. Creates Step
   * Two message to be sent.
   */
  private void computeStepTwo() {
    Log.d(LOG_TAG, "Computing round 2.");

    // Check incoming message sender.
    if (!jIncoming.get(Constants.JSON_KEY_TYPE).equals(theirSignerId + "1")) {
      Log.e(LOG_TAG, "Invalid round 1 message: " + jIncoming.toJSONString());
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Check incoming message fields.
    ExtendedJSONObject iPayload = null;
    try {
      iPayload = jIncoming.getObject(Constants.JSON_KEY_PAYLOAD);
      if (iPayload == null
          || iPayload.getObject(Constants.ZKP_KEY_ZKP_X1) == null
          || !theirSignerId.equals(iPayload.getObject(Constants.ZKP_KEY_ZKP_X1)
              .get(Constants.ZKP_KEY_ID))
          || iPayload.getObject(Constants.ZKP_KEY_ZKP_X2) == null
          || !theirSignerId.equals(iPayload.getObject(Constants.ZKP_KEY_ZKP_X2)
              .get(Constants.ZKP_KEY_ID))) {
        Log.e(LOG_TAG, "Invalid round 1 message: " + jIncoming.toJSONString());
        abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
        return;
      }
    } catch (NonObjectJSONException e) {
      e.printStackTrace();
    }

    // Extract message fields.
    jParty.gx3 = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_GX1),
        16);
    jParty.gx4 = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_GX2),
        16);

    ExtendedJSONObject zkpPayload3 = null;
    ExtendedJSONObject zkpPayload4 = null;
    try {
      zkpPayload3 = iPayload.getObject(Constants.ZKP_KEY_ZKP_X1);
      zkpPayload4 = iPayload.getObject(Constants.ZKP_KEY_ZKP_X2);
      if (zkpPayload3 == null || zkpPayload4 == null) {
        Log.e(LOG_TAG, "Invalid round 1 zkpPayload message");
        abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
        return;
      }
    } catch (NonObjectJSONException e) {
      e.printStackTrace();
    }

    // Extract ZKPs.
    String zkp3_gr = (String) zkpPayload3.get(Constants.ZKP_KEY_GR);
    String zkp3_b = (String) zkpPayload3.get(Constants.ZKP_KEY_B);
    String zkp3_id = (String) zkpPayload3.get(Constants.ZKP_KEY_ID);

    String zkp4_gr = (String) zkpPayload4.get(Constants.ZKP_KEY_GR);
    String zkp4_b = (String) zkpPayload4.get(Constants.ZKP_KEY_B);
    String zkp4_id = (String) zkpPayload4.get(Constants.ZKP_KEY_ID);

    jParty.zkp3 = new Zkp(new BigInteger(zkp3_gr, 16), new BigInteger(zkp3_b,
        16), zkp3_id);
    jParty.zkp4 = new Zkp(new BigInteger(zkp4_gr, 16), new BigInteger(zkp4_b,
        16), zkp4_id);

    // Jpake round 2
    try {
      JPakeCrypto.round2(secret, jParty, numGen);
    } catch (Gx4IsOneException e) {
      Log.e(LOG_TAG, "gx4 cannot equal 1.");
      abort(Constants.JPAKE_ERROR_INTERNAL);
    } catch (IncorrectZkpException e) {
      Log.e(LOG_TAG, "ZKP mismatch");
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
    }

    // Make outgoing payload.
    Zkp zkpA = jParty.thisZkpA;
    ExtendedJSONObject oPayload = new ExtendedJSONObject();
    ExtendedJSONObject jZkpA = makeJZkp(zkpA.gr, zkpA.b, zkpA.id);
    oPayload.put(Constants.ZKP_KEY_A,
        BigIntegerHelper.toEvenLengthHex(jParty.thisA));
    oPayload.put(Constants.ZKP_KEY_ZKP_A, jZkpA);

    // Make outgoing message.
    jOutgoing = new ExtendedJSONObject();
    jOutgoing.put(Constants.JSON_KEY_TYPE, mySignerId + "2");
    jOutgoing.put(Constants.JSON_KEY_VERSION, KEYEXCHANGE_VERSION);
    jOutgoing.put(Constants.JSON_KEY_PAYLOAD, oPayload);

    // Different behavior depending on sender or receiver.
    if (pairWithPin) {
      state = State.SNDR_STEP_TWO;
      stateContext = State.PUT;
      scheduleGetRequest(jpakePollInterval);
    } else {
      stateContext = State.RCVR_STEP_TWO;
      state = State.PUT;
      putStep();
    }
  }

  /*
   * Final Step of J-PAKE protocol.
   *
   * Verifies message computed by other party in Step Two. Creates or fetches
   * encrypted message for verification of successful key exchange.
   */
  private void computeFinal() {
    Log.d(LOG_TAG, "Computing final round.");
    // Check incoming message type.
    if (!jIncoming.get(Constants.JSON_KEY_TYPE).equals(theirSignerId + "2")) {
      Log.e(LOG_TAG, "Invalid round 2 message: " + jIncoming.toJSONString());
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return;
    }

    // Check incoming message fields.
    ExtendedJSONObject iPayload = null;
    try {
      iPayload = jIncoming.getObject(Constants.JSON_KEY_PAYLOAD);
      if (iPayload == null
          || iPayload.getObject(Constants.ZKP_KEY_ZKP_A) == null
          || !theirSignerId.equals(iPayload.getObject(Constants.ZKP_KEY_ZKP_A)
              .get(Constants.ZKP_KEY_ID))) {
        Log.e(LOG_TAG, "Invalid round 2 message: " + jIncoming.toJSONString());
        abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
        return;
      }
    } catch (NonObjectJSONException e) {
      e.printStackTrace();
    }
    // Extract fields.
    jParty.otherA = new BigInteger((String) iPayload.get(Constants.ZKP_KEY_A),
        16);

    ExtendedJSONObject zkpPayload = null;
    try {
      zkpPayload = iPayload.getObject(Constants.ZKP_KEY_ZKP_A);
    } catch (NonObjectJSONException e) {
      e.printStackTrace();
    }
    // Extract ZKP.
    String gr = (String) zkpPayload.get(Constants.ZKP_KEY_GR);
    String b = (String) zkpPayload.get(Constants.ZKP_KEY_B);
    String id = (String) zkpPayload.get(Constants.ZKP_KEY_ID);

    jParty.otherZkpA = new Zkp(new BigInteger(gr, 16), new BigInteger(b, 16),
        id);

    myKeyBundle = null;
    try {
      myKeyBundle = JPakeCrypto.finalRound(secret, jParty);
    } catch (IncorrectZkpException e) {
      Log.e(LOG_TAG, "ZKP mismatch");
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      e.printStackTrace();
    }

    if (pairWithPin) { // Wait for other device to send verification of keys.
      Log.d(LOG_TAG, "get: verifyPairing");
      this.state = State.VERIFY_PAIRING;
      scheduleGetRequest(jpakePollInterval);
    } else { // Prepare and send verification of keys.
      jOutgoing = computeKeyVerification(myKeyBundle);
      stateContext = State.VERIFY_KEY;
      this.state = State.PUT;
      putStep();
    }
  }

  /*
   * (Receiver Only) Helper method to compute verification message from JPake
   * key bundle, to be sent to other device for verification.
   */
  public ExtendedJSONObject computeKeyVerification(KeyBundle keyBundle) {
    Log.d(LOG_TAG, "Encrypting key verification value.");
    // KeyBundle not null
    ExtendedJSONObject jPayload = null;
    try {
      jPayload = encryptPayload(JPAKE_VERIFY_VALUE, keyBundle);
    } catch (UnsupportedEncodingException e) {
      Log.e(LOG_TAG, "Failed to encrypt key verification value.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
    } catch (CryptoException e) {
      Log.e(LOG_TAG, "Failed to encrypt key verification value.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
    }
    Log.d(
        LOG_TAG,
        "enc key64: "
            + new String(Base64.encodeBase64(keyBundle.getEncryptionKey())));
    Log.e(LOG_TAG,
        "hmac64: " + new String(Base64.encodeBase64(keyBundle.getHMACKey())));

    ExtendedJSONObject result = new ExtendedJSONObject();
    result.put(Constants.JSON_KEY_TYPE, mySignerId + "3");
    result.put(Constants.JSON_KEY_VERSION, KEYEXCHANGE_VERSION);
    result.put(Constants.JSON_KEY_PAYLOAD, jPayload.object);
    return result;
  }

  /*
   * (Sender Only) Helper method to check the verification message sent by the
   * other device against self-derived key.
   */
  private boolean verifyPairing(ExtendedJSONObject verificationObject,
      KeyBundle keyBundle) throws CryptoException, IOException, ParseException,
      NonObjectJSONException {
    if (!verificationObject.get(Constants.JSON_KEY_TYPE).equals(
        theirSignerId + "3")) {
      Log.e(LOG_TAG,
          "Invalid round 3 message: " + verificationObject.toJSONString());
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
      return false;
    }
    ExtendedJSONObject payload = verificationObject
        .getObject(Constants.JSON_KEY_PAYLOAD);
    String theirCiphertext = (String) payload
        .get(Constants.JSON_KEY_CIPHERTEXT);
    String iv = (String) payload.get(Constants.JSON_KEY_IV);
    boolean correctPairing = verifyCiphertext(theirCiphertext, iv, keyBundle);
    return correctPairing;
  }

  /*
   * Helper function to verify an incoming ciphertext and IV against derived
   * keyBundle.
   */
  public boolean verifyCiphertext(String theirCiphertext, String iv,
      KeyBundle keyBundle) throws UnsupportedEncodingException, CryptoException {
    byte[] cleartextBytes = JPAKE_VERIFY_VALUE.getBytes("UTF-8");
    CryptoInfo info = new CryptoInfo(cleartextBytes, keyBundle);
    info.setIV(Base64.decodeBase64(iv));

    Cryptographer.encrypt(info);
    String myCiphertext = new String(Base64.encodeBase64(info.getMessage()));
    return myCiphertext.equals(theirCiphertext);
  }

  /*
   * (Sender Only)
   *
   * Called from controller, with Sync credentials to be encrypted and sent.
   */
  public void sendAndComplete(JSONObject jObj)
      throws JPakeNoActivePairingException {
    if (paired && !finished) {
      String outData = jObj.toJSONString();
      state = State.ENCRYPT_PUT;
      encryptData(myKeyBundle, outData);
      putStep();
    } else {
      Log.e(LOG_TAG, "Can't send data, no active pairing!");
      throw new JPakeNoActivePairingException();
    }
  }

  /*
   * (Sender Only)
   *
   * Encrypt payload and package into jOutgoing for sending with a PUT request.
   *
   * @param keyBundle Encryption keys derived during J-PAKE.
   * @param payload   Credentials data to be encrypted.
   */
  private void encryptData(KeyBundle keyBundle, String payload) {
    Log.d(LOG_TAG, "Encrypting data.");
    ExtendedJSONObject jPayload = null;
    try {
      jPayload = encryptPayload(payload, keyBundle);
    } catch (UnsupportedEncodingException e) {
      Log.e(LOG_TAG, "Failed to encrypt data.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
    } catch (CryptoException e) {
      Log.e(LOG_TAG, "Failed to encrypt data.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
    }
    jOutgoing = new ExtendedJSONObject();
    jOutgoing.put(Constants.JSON_KEY_TYPE, mySignerId + "3");
    jOutgoing.put(Constants.JSON_KEY_VERSION, KEYEXCHANGE_VERSION);
    jOutgoing.put(Constants.JSON_KEY_PAYLOAD, jPayload.object);
  }

  /*
   * (Receiver Only)
   *
   * Decrypt jIncoming message from other device and extract credentials to be stored.
   *
   * @param keyBundle
   */
  private void decryptData(KeyBundle keyBundle) {
    Log.d(LOG_TAG, "Verifying their key");
    if (!(theirSignerId + "3").equals((String) jIncoming
        .get(Constants.JSON_KEY_TYPE))) {
      try {
        Log.e(LOG_TAG, "Invalid round 3 data: " + jsonEntity(jIncoming.object));
      } catch (UnsupportedEncodingException e) {
        e.printStackTrace();
      }
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
    }

    // Decrypt payload and verify HMAC.
    ExtendedJSONObject iPayload = null;
    try {
      iPayload = jIncoming.getObject(Constants.JSON_KEY_PAYLOAD);
    } catch (NonObjectJSONException e1) {
      Log.e(LOG_TAG, "Invalid round 3 data.", e1);
      abort(Constants.JPAKE_ERROR_WRONGMESSAGE);
    }
    Log.d(LOG_TAG, "Decrypting data.");
    String cleartext = null;
    try {
      cleartext = new String(decryptPayload(iPayload, keyBundle), "UTF-8");
    } catch (UnsupportedEncodingException e1) {
      Log.e(LOG_TAG, "Failed to decrypt data.", e1);
      abort(Constants.JPAKE_ERROR_INTERNAL);
    } catch (CryptoException e1) {
      Log.e(LOG_TAG, "Failed to decrypt data.", e1);
      abort(Constants.JPAKE_ERROR_KEYMISMATCH);
    }
    JSONObject jCreds = null;
    try {
      jCreds = getJSONObject(cleartext);
    } catch (Exception e) {
      Log.e(LOG_TAG, "Invalid data: " + cleartext);
      abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }
    complete(jCreds);
  }

  /*
   * Complete Sync setup and return credentials to controller to be stored, if receiver.
   *
   * @param jCreds Credentials to be stored by controller. May be null if device is sender.
   */
  private void complete(JSONObject jCreds) {
    Log.d(LOG_TAG, "Exchange complete.");
    finished = true;
    ssActivity.onComplete(jCreds);
  }

  /* JpakeRequestDelegate methods */
  @Override
  public void onRequestFailure(HttpResponse res) {
    JPakeResponse response = new JPakeResponse(res);
    switch (this.state) {
    case GET_CHANNEL:
      Log.e(LOG_TAG, "getChannel failure: " + response.getStatusCode());
      abort(Constants.JPAKE_ERROR_CHANNEL);
      break;
    case VERIFY_PAIRING:
    case VERIFY_KEY:
    case SNDR_STEP_ZERO:
    case SNDR_STEP_ONE:
    case SNDR_STEP_TWO:
    case RCVR_STEP_ONE:
    case RCVR_STEP_TWO:
      int statusCode = response.getStatusCode();
      switch (statusCode) {
      case 304:
        Log.d(LOG_TAG, "Channel hasn't been updated yet. Will try again later");
        if (pollTries >= jpakeMaxTries) {
          Log.e(LOG_TAG, "Tried for " + pollTries + " times, maxTries " + jpakeMaxTries + ", aborting");
          abort(Constants.JPAKE_ERROR_TIMEOUT);
          return;
        }
        pollTries += 1;
        if (!finished) {
          scheduleGetRequest(jpakePollInterval);
        }
        return;
      case 404:
        Log.e(LOG_TAG, "No data found in channel.");
        abort(Constants.JPAKE_ERROR_NODATA);
        break;
      case 412: // "Precondition failed"
        Log.d(LOG_TAG, "Message already replaced on server by other party.");
        onRequestSuccess(res);
        break;
      default:
        Log.e(LOG_TAG, "Could not retrieve data. Server responded with HTTP "
            + statusCode);
        abort(Constants.JPAKE_ERROR_SERVER);
        return;
      }
      pollTries = 0;
      break;
    case PUT:
      Log.e(LOG_TAG, "Could not upload data. Server responded with HTTP "
          + response.getStatusCode());
      abort(Constants.JPAKE_ERROR_SERVER);
      break;
    case ABORT:
      Log.e(LOG_TAG, "Abort: request failure.");
      break;
    case REPORT_FAILURE:
      Log.e(LOG_TAG, "ReportFailure: failure. Server responded with HTTP "
          + response.getStatusCode());
      break;
    default:
      Log.e(LOG_TAG, "Unhandled request failure " + response.getStatusCode());
    }
  }

  @Override
  public void onRequestError(Exception e) {
    abort(Constants.JPAKE_ERROR_NETWORK);
  }

  @Override
  public void onRequestSuccess(HttpResponse res) {
    if (finished)
      return;
    JPakeResponse response = new JPakeResponse(res);
    Header[] etagHeaders;
    switch (this.state) {
    case GET_CHANNEL:
      Object body = null;
      try {
        body = response.jsonBody();
      } catch (IllegalStateException e1) {
        e1.printStackTrace();
      } catch (IOException e1) {
        e1.printStackTrace();
      } catch (ParseException e1) {
        abort(Constants.JPAKE_ERROR_CHANNEL);
      }
      String channel = body instanceof String ? (String) body : null;
      if (channel == null) { // should be string
        abort(Constants.JPAKE_ERROR_CHANNEL);
        return;
      }
      channelUrl = jpakeServer + channel;
      Log.d(LOG_TAG, "using channel " + channel);

      ssActivity.displayPin(secret + channel);

      // Set up next step.
      this.state = State.RCVR_STEP_ONE;
      computeStepOne();
      break;

    // Results from GET request. Continue flow depending on case.
    case RCVR_STEP_ONE:
      ssActivity.onPairingStart();
      jpakeMaxTries = MAX_TRIES;
      // fall through
    case RCVR_STEP_TWO:
    case SNDR_STEP_ZERO:
    case SNDR_STEP_ONE:
    case SNDR_STEP_TWO:
    case VERIFY_KEY:
    case VERIFY_PAIRING:
      etagHeaders = response.httpResponse().getHeaders("etag");
      if (etagHeaders == null) {
        try {
          Log.e(LOG_TAG,
              "Server did not supply ETag for message: " + response.body());
          abort(Constants.JPAKE_ERROR_SERVER);
        } catch (IllegalStateException e) {
          e.printStackTrace();
        } catch (IOException e) {
          e.printStackTrace();
        }
        return;
      }

      theirEtag = etagHeaders[0].toString();
      try {
        jIncoming = response.jsonObjectBody();
      } catch (IllegalStateException e) {
        e.printStackTrace();
      } catch (IOException e) {
        e.printStackTrace();
      } catch (ParseException e) {
        abort(Constants.JPAKE_ERROR_INVALID);
        return;
      } catch (NonObjectJSONException e) {
        abort(Constants.JPAKE_ERROR_INVALID);
        return;
      }
      Log.d(LOG_TAG, "incoming message: " + jIncoming.toJSONString());

      if (this.state == State.SNDR_STEP_ZERO) {
        computeStepOne();
      } else if (this.state == State.RCVR_STEP_ONE
          || this.state == State.SNDR_STEP_ONE) {
        computeStepTwo();
      } else if (this.state == State.SNDR_STEP_TWO) {
        stateContext = state;
        state = State.PUT;
        putStep();
      } else if (this.state == State.RCVR_STEP_TWO) {
        computeFinal();
      } else if (this.state == State.VERIFY_KEY) {
        decryptData(myKeyBundle);
      } else if (this.state == State.VERIFY_PAIRING) {
        try {
          if (verifyPairing(jIncoming, myKeyBundle)) {
            paired = true;
            ssActivity.onPaired();
          } else {
            abort(Constants.JPAKE_ERROR_KEYMISMATCH);
          }
        } catch (NonObjectJSONException e) {
          e.printStackTrace();
        } catch (CryptoException e) {
          e.printStackTrace();
        } catch (IOException e) {
          e.printStackTrace();
        } catch (ParseException e) {
          e.printStackTrace();
        }
      }
      break;

    case PUT:
      etagHeaders = response.httpResponse().getHeaders("etag");
      myEtag = response.httpResponse().getHeaders("etag")[0].getValue();

      state = stateContext;
      if (state == State.VERIFY_KEY) {
        jpakeMaxTries = MAX_TRIES_LAST_MSG;
        ssActivity.onPaired();
      }
      if (state == State.SNDR_STEP_ONE) {
        computeStepTwo();
        return; // No need to wait for response from PUT request.
      }
      if (state == State.SNDR_STEP_TWO) {
        computeFinal();
        return; // No need to wait for response from PUT request.
      }

      // Pause twice the poll interval.
      scheduleGetRequest(2 * jpakePollInterval);
      Log.i(LOG_TAG, "scheduling 2xPollInterval for " + state.name());
      break;

    case ENCRYPT_PUT:
      complete(null); // No need to pass along credentials, they are already
                      // stored on device.
      break;

    case ABORT:
      Log.e(LOG_TAG, "Key exchange successfully aborted.");
      break;
    default:
      Log.e(LOG_TAG, "Unhandled response success.");
    }
  }

  /* ResourceDelegate that handles Resource responses */
  public ResourceDelegate makeRequestResourceDelegate() {
    return new JpakeRequestResourceDelegate(this);
  }

  public class JpakeRequestResourceDelegate implements ResourceDelegate {

    private JPakeRequestDelegate requestDelegate;

    public JpakeRequestResourceDelegate(JPakeRequestDelegate delegate) {
      this.requestDelegate = delegate;
    }

    @Override
    public String getCredentials() {
      // Jpake setup has no credentials
      return null;
    }

    @Override
    public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
      request.setHeader(new BasicHeader("X-KeyExchange-Id", clientId));

      switch (state) {
      case REPORT_FAILURE:
        // optional: set report cid to delete channel

      case ABORT:
        request.setHeader(new BasicHeader("X-KeyExchange-Cid", channel));
        break;

      case PUT:
        if (myEtag == null && state == State.RCVR_STEP_ONE) {
          request.setHeader(new BasicHeader("If-None-Match", "*"));
        }
        // fall through
      case VERIFY_KEY:
      case VERIFY_PAIRING:
      case RCVR_STEP_ONE:
      case RCVR_STEP_TWO:
        if (myEtag != null) {
          request.setHeader(new BasicHeader("If-None-Match", myEtag));
        }
        break;
      }
    }

    @Override
    public void handleHttpResponse(HttpResponse response) {
      // TODO: maybe use of http requests is wasteful?
      if (isSuccess(response)) {
        this.requestDelegate.onRequestSuccess(response);
      } else {
        this.requestDelegate.onRequestFailure(response);
      }
      SyncResourceDelegate.consumeEntity(response.getEntity());
    }

    @Override
    public void handleHttpProtocolException(ClientProtocolException e) {
      Log.e(LOG_TAG, "Got HTTP protocol exception.", e);
      this.requestDelegate.onRequestError(e);
    }

    @Override
    public void handleTransportException(GeneralSecurityException e) {
      Log.e(LOG_TAG, "Got HTTP transport exception.", e);
      this.requestDelegate.onRequestError(e);
    }

    @Override
    public void handleHttpIOException(IOException e) {
      // TODO: abort or pass on.
      Log.e(LOG_TAG, "HttpIOException", e);
      switch (state) {
      case GET_CHANNEL:
        Log.e(LOG_TAG, "Failed on GetChannel.", e);
        break;

      case RCVR_STEP_ONE:
      case RCVR_STEP_TWO:
        Log.e(LOG_TAG, "Failed on GET", e);
        break;
      case PUT:
        Log.e(LOG_TAG, "Failed on PUT.", e);
        break;

      case REPORT_FAILURE:
        Log.e(LOG_TAG, "Report failed: " + error);
        break;
      default:
        Log.e(LOG_TAG, "Unhandled HTTP I/O exception.", e);
      }
      abort(Constants.JPAKE_ERROR_NETWORK);
    }

    @Override
    public int connectionTimeout() {
      return REQUEST_TIMEOUT;
    }

    @Override
    public int socketTimeout() {
      return SOCKET_TIMEOUT;
    }

    private int getStatusCode(HttpResponse response) {
      return response.getStatusLine().getStatusCode();
    }

    private boolean isSuccess(HttpResponse response) {
      return getStatusCode(response) == 200;
    }
  }

  /**
   * Abort the current pairing. The channel on the server will be deleted if the
   * abort wasn't due to a network or server error. The controller's 'onAbort()'
   * method is notified in all cases.
   *
   * @param error
   *          [can be null] Error constant indicating the reason for the abort.
   *          Defaults to user abort
   */
  public void abort(String error) {
    Log.d(LOG_TAG, "aborting...");
    finished = true;

    if (error == null) {
      error = Constants.JPAKE_ERROR_USERABORT;
    }
    Log.d(LOG_TAG, error);

    if (Constants.JPAKE_ERROR_CHANNEL.equals(error)
        || Constants.JPAKE_ERROR_NETWORK.equals(error)
        || Constants.JPAKE_ERROR_NODATA.equals(error)) {
      // No report.
    } else {
      reportFailure(error);
    }
    ssActivity.displayAbort(error);
  }

  /*
   * Make a /report post to to server
   */
  private void reportFailure(String error) {
    Log.d(LOG_TAG, "reporting error to server");
    this.error = error;
    runOnThread(new Runnable() {
      @Override
      public void run() {
        JPakeRequest report;
        try {
          report = new JPakeRequest(jpakeServer + "report",
              makeRequestResourceDelegate());
          report.post(new StringEntity(""));
        } catch (URISyntaxException e) {
          e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
          e.printStackTrace();
        }
      }
    });

  }

  /* Utility functions */

  /*
   * Generates and sets a clientId for communications with JPAKE setup server.
   */
  private void setClientId() {
    byte[] rBytes = generateRandomBytes(JPAKE_LENGTH_CLIENTID / 2);
    StringBuilder id = new StringBuilder();

    for (byte b : rBytes) {
      String hexString = Integer.toHexString(b);
      if (hexString.length() == 1) {
        hexString = "0" + hexString;
      }
      int len = hexString.length();
      id.append(hexString.substring(len - 2, len));
    }
    clientId = id.toString();
  }

  /*
   * Generates and sets a JPAKE PIN to be displayed to user.
   */
  private void createSecret() {
    // 0-9a-z without 1,l,o,0
    String key = "23456789abcdefghijkmnpqrstuvwxyz";
    int keylen = key.length();

    byte[] rBytes = generateRandomBytes(JPAKE_LENGTH_SECRET);
    StringBuilder secret = new StringBuilder();
    for (byte b : rBytes) {
      secret.append(key.charAt(Math.abs(b) * keylen / 256));
    }
    this.secret = secret.toString();
  }

  /*
   * Helper for turning a JSON object into a payload.
   *
   * @param body JSONObject body to be converted to StringEntity.
   * @return StringEntity representation of JSONObject.
   * @throws UnsupportedEncodingException
   */
  protected StringEntity jsonEntity(JSONObject body)
      throws UnsupportedEncodingException {
    StringEntity e = new StringEntity(body.toJSONString(), "UTF-8");
    e.setContentType("application/json");
    return e;
  }

  /**
   * TimerTask for use with delayed GET requests.
   *
   */
  public class GetStepTimerTask extends TimerTask {
    private JPakeRequest request;

    public GetStepTimerTask(JPakeRequest request) {
      this.request = request;
    }

    @Override
    public void run() {
      request.get();
    }
  }

  /*
   * Helper method to schedule a GET request with some delay.
   */

  private void scheduleGetRequest(int delay) {
    JPakeRequest getRequest = null;
    try {
      getRequest = new JPakeRequest(channelUrl, makeRequestResourceDelegate());
    } catch (URISyntaxException e) {
      e.printStackTrace();
      abort(Constants.JPAKE_ERROR_CHANNEL);
      return;
    }

    GetStepTimerTask getStepTimerTask = new GetStepTimerTask(getRequest);
    timerScheduler.schedule(getStepTimerTask, delay);
  }

  /*
   * Helper method to get a JSONObject from a String. Input: String containing
   * JSON. Output: Extracted JSONObject. Throws: Exception if JSON is invalid.
   */
  private JSONObject getJSONObject(String jsonString) throws Exception {
    Reader in = new StringReader(jsonString);
    try {
      return (JSONObject) new JSONParser().parse(in);
    } catch (Exception e) {
      throw e;
    }
  }

  /*
   * Helper to generate random bytes
   *
   * @param length Number of bytes to generate
   */
  private static byte[] generateRandomBytes(int length) {
    byte[] bytes = new byte[length];
    Random random = new Random(System.nanoTime());
    random.nextBytes(bytes);
    return bytes;
  }

  /*
   * Helper function to generate a JSON encoded ZKP.
   */
  private ExtendedJSONObject makeJZkp(BigInteger gr, BigInteger b, String id) {
    ExtendedJSONObject result = new ExtendedJSONObject();
    result.put(Constants.ZKP_KEY_GR, BigIntegerHelper.toEvenLengthHex(gr));
    result.put(Constants.ZKP_KEY_B, BigIntegerHelper.toEvenLengthHex(b));
    result.put(Constants.ZKP_KEY_ID, id);
    return result;
  }

  /**
   * Helper method for doing actual decryption.
   *
   * Input: JSONObject containing a valid payload (cipherText, IV, HMAC),
   * KeyBundle with keys for decryption. Output: byte[] clearText
   *
   * @throws CryptoException
   * @throws UnsupportedEncodingException
   */
  public byte[] decryptPayload(ExtendedJSONObject payload, KeyBundle keybundle)
      throws CryptoException, UnsupportedEncodingException {
    byte[] ciphertext = Utils.decodeBase64((String) payload
        .get(Constants.JSON_KEY_CIPHERTEXT));
    byte[] iv = Utils.decodeBase64((String) payload.get(Constants.JSON_KEY_IV));
    byte[] hmac = Utils.hex2Byte((String) payload.get(Constants.JSON_KEY_HMAC));
    byte[] plainbytes = Cryptographer.decrypt(new CryptoInfo(ciphertext, iv,
        hmac, keybundle));
    return plainbytes;
  }

  /**
   * Helper method for doing actual encryption.
   *
   * Input: String of JSONObject KeyBundle with keys for decryption
   *
   * Output: ExtendedJSONObject with IV, hmac, ciphertext
   *
   * @throws CryptoException
   * @throws UnsupportedEncodingException
   */
  public ExtendedJSONObject encryptPayload(String data, KeyBundle keyBundle)
      throws UnsupportedEncodingException, CryptoException {
    if (keyBundle == null) {
      throw new NoKeyBundleException();
    }
    byte[] cleartextBytes = data.getBytes("UTF-8");
    CryptoInfo info = new CryptoInfo(cleartextBytes, keyBundle);
    Cryptographer.encrypt(info);
    String message = new String(Base64.encodeBase64(info.getMessage()));
    String iv = new String(Base64.encodeBase64(info.getIV()));

    ExtendedJSONObject payload = new ExtendedJSONObject();
    payload.put(Constants.JSON_KEY_CIPHERTEXT, message);
    payload.put(Constants.JSON_KEY_IV, iv);
    if (this.state == State.ENCRYPT_PUT) {
      String hmac = Utils.byte2hex(info.getHMAC());
      payload.put(Constants.JSON_KEY_HMAC, hmac);
    }
    return payload;
  }
}
