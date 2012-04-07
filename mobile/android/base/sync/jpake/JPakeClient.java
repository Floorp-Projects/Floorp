/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.util.LinkedList;
import java.util.Queue;

import org.json.simple.JSONObject;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.CryptoInfo;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.NoKeyBundleException;
import org.mozilla.gecko.sync.jpake.stage.CompleteStage;
import org.mozilla.gecko.sync.jpake.stage.ComputeFinalStage;
import org.mozilla.gecko.sync.jpake.stage.ComputeKeyVerificationStage;
import org.mozilla.gecko.sync.jpake.stage.ComputeStepOneStage;
import org.mozilla.gecko.sync.jpake.stage.ComputeStepTwoStage;
import org.mozilla.gecko.sync.jpake.stage.DecryptDataStage;
import org.mozilla.gecko.sync.jpake.stage.DeleteChannel;
import org.mozilla.gecko.sync.jpake.stage.GetChannelStage;
import org.mozilla.gecko.sync.jpake.stage.GetRequestStage;
import org.mozilla.gecko.sync.jpake.stage.JPakeStage;
import org.mozilla.gecko.sync.jpake.stage.PutRequestStage;
import org.mozilla.gecko.sync.jpake.stage.VerifyPairingStage;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.activities.SetupSyncActivity;

import ch.boye.httpclientandroidlib.entity.StringEntity;

public class JPakeClient {

  private static String       LOG_TAG                 = "JPakeClient";

  // J-PAKE constants.
  public final static int     REQUEST_TIMEOUT         = 60 * 1000;       // 1 min
  public final static int     KEYEXCHANGE_VERSION     = 3;
  public final static String  JPAKE_VERIFY_VALUE      = "0123456789ABCDEF";

  private final static String JPAKE_SIGNERID_SENDER   = "sender";
  private final static String JPAKE_SIGNERID_RECEIVER = "receiver";
  private final static int    JPAKE_LENGTH_SECRET     = 8;
  private final static int    JPAKE_LENGTH_CLIENTID   = 256;

  private final static int    MAX_TRIES               = 10;
  private final static int    MAX_TRIES_FIRST_MSG     = 300;
  private final static int    MAX_TRIES_LAST_MSG      = 300;

  // J-PAKE session values.
  public String              clientId;
  public String              secret;

  public String              myEtag;
  public String              mySignerId;
  public String              theirEtag;
  public String              theirSignerId;
  public String              jpakeServer;

  // J-PAKE state.
  public boolean             paired                  = false;
  public boolean             finished                = false;

  // J-PAKE values.
  public int                 jpakePollInterval;
  public int                 jpakeMaxTries;
  public String              channel;
  public volatile String     channelUrl;

  // J-PAKE session data.
  public KeyBundle           myKeyBundle;
  public JSONObject          jCreds;

  public ExtendedJSONObject  jOutgoing;
  public ExtendedJSONObject  jIncoming;

  public JPakeParty          jParty;
  public JPakeNumGenerator   numGen;

  public int                 pollTries = 0;

  // UI controller.
  private SetupSyncActivity controllerActivity;
  private Queue<JPakeStage>  stages;

  public JPakeClient(SetupSyncActivity activity) {
    controllerActivity = activity;
    jpakeServer = "https://setup.services.mozilla.com/";
    jpakePollInterval = 1 * 1000; // 1 second
    jpakeMaxTries = MAX_TRIES;

    if (!jpakeServer.endsWith("/")) {
      jpakeServer += "/";
    }

    setClientId();
    numGen = new JPakeNumGeneratorRandom();
  }

  /**
   * Set up Sender sequence of stages for J-PAKE. (sender of credentials)
   *
   */

  private void prepareSenderStages() {
    Queue<JPakeStage> jStages = new LinkedList<JPakeStage>();
    jStages.add(new ComputeStepOneStage());
    jStages.add(new GetRequestStage());
    jStages.add(new PutRequestStage());
    jStages.add(new ComputeStepTwoStage());
    jStages.add(new GetRequestStage());
    jStages.add(new PutRequestStage());
    jStages.add(new ComputeFinalStage());
    jStages.add(new GetRequestStage());
    jStages.add(new VerifyPairingStage()); // Calls onPaired if verified.

    stages = jStages;
  }

  /**
   * Set up Receiver sequence of stages for J-PAKE. (receiver of credentials)
   *
   */
  private void prepareReceiverStages() {
    Queue<JPakeStage> jStages = new LinkedList<JPakeStage>();
    jStages.add(new GetChannelStage());
    jStages.add(new ComputeStepOneStage());
    jStages.add(new PutRequestStage());
    jStages.add(new GetRequestStage());
    jStages.add(new JPakeStage() {
      @Override
      public void execute(JPakeClient jpakeClient) {

        // Notify controller that pairing has started.
        jpakeClient.onPairingStart();

        // Switch back to smaller time-out.
        jpakeClient.jpakeMaxTries = JPakeClient.MAX_TRIES;
        jpakeClient.runNextStage();
      }
    });
    jStages.add(new ComputeStepTwoStage());
    jStages.add(new PutRequestStage());
    jStages.add(new GetRequestStage());
    jStages.add(new ComputeFinalStage());
    jStages.add(new ComputeKeyVerificationStage());
    jStages.add(new PutRequestStage());
    jStages.add(new JPakeStage() {

      @Override
      public void execute(JPakeClient jpakeClient) {
        jpakeMaxTries = MAX_TRIES_LAST_MSG;
        jpakeClient.runNextStage();
      }

    });
    jStages.add(new GetRequestStage());
    jStages.add(new DecryptDataStage());
    jStages.add(new CompleteStage());

    stages = jStages;
  }

  /**
   *
   * Pairing using PIN provided on other device. Functionality available only
   * when a Sync account has already been set up.
   *
   * @param pin
   *          12-character string containing PIN entered by the user.
   */
  public void pairWithPin(String pin) {
    mySignerId = JPAKE_SIGNERID_SENDER;
    theirSignerId = JPAKE_SIGNERID_RECEIVER;
    jParty = new JPakeParty(mySignerId);

    // Extract secret and server channel.
    secret = pin.substring(0, JPAKE_LENGTH_SECRET);
    channel = pin.substring(JPAKE_LENGTH_SECRET);
    channelUrl = jpakeServer + channel;

    prepareSenderStages();
    runNextStage();
  }

  /**
   *
   * Initiate pairing and receive data, without having received a PIN. The PIN
   * will be generated and passed on to the controller to be displayed to the
   * user.
   *
   * Starts J-PAKE protocol.
   */
  public void receiveNoPin() {
    mySignerId = JPAKE_SIGNERID_RECEIVER;
    theirSignerId = JPAKE_SIGNERID_SENDER;
    jParty = new JPakeParty(mySignerId);

    // TODO: fetch from prefs
    jpakeMaxTries = MAX_TRIES_FIRST_MSG;

    createSecret();
    prepareReceiverStages();
    runNextStage();
  }

  /**
   * Run next stage of J-PAKE.
   */
  public void runNextStage() {
    if (finished || stages.size() == 0) {
      Logger.debug(LOG_TAG, "All stages complete.");
      return;
    }
    JPakeStage currentStage = null;
    try{
      currentStage = stages.remove();
      Logger.debug(LOG_TAG, "starting stage " + currentStage.toString());
      currentStage.execute(this);
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Exception in stage " + currentStage, e);
      abort("Stage exception.");
    }
  }

  /**
   * Abort J-PAKE. This can propagate an error from the stages, or result from
   * UI abort (onPause, user abort)
   *
   * @param reason
   *          Reason for abort.
   */
  public void abort(String reason) {
    finished = true;
    // We do not need to clean up the channel in the following cases:
    if (Constants.JPAKE_ERROR_CHANNEL.equals(reason) ||
        Constants.JPAKE_ERROR_NETWORK.equals(reason) ||
        Constants.JPAKE_ERROR_NODATA.equals(reason) ||
        channelUrl == null) {
      // We may leak a channel if the activity aborts sync while requesting the channel.
      // The server, however, will delete the channel anyways after a certain time has passed.
      displayAbort(reason);
    } else {
      // Delete channel, then call controller's displayAbort in callback.
      new DeleteChannel().execute(this, reason);
    }
  }

  public void displayAbort(String reason) {
    controllerActivity.displayAbort(reason);
  }

  /* Static helper methods used by stages. */

  /**
   * Run on a different thread from the thread pool.
   *
   * @param run
   *            Runnable to run on separate thread.
   */
  public static void runOnThread(Runnable run) {
    ThreadPool.run(run);
  }

  /**
   *
   * @param secretString
   *          String to convert to BigInteger
   * @return BigInteger representation of secretString
   *
   * @throws UnsupportedEncodingException
   */
  public static BigInteger secretAsBigInteger(String secretString) throws UnsupportedEncodingException {
    return new BigInteger(secretString.getBytes("UTF-8"));
  }

  /**
   * Helper method for doing actual encryption.
   *
   * Input: String of JSONObject KeyBundle with keys for encryption
   *
   * Output: ExtendedJSONObject with IV, ciphertext, hmac (if sender)
   *
   * @throws CryptoException
   * @throws UnsupportedEncodingException
   */
  public static ExtendedJSONObject encryptPayload(String data, KeyBundle keyBundle, boolean makeHmac)
      throws UnsupportedEncodingException, CryptoException {
    if (keyBundle == null) {
      throw new NoKeyBundleException();
    }

    byte[] cleartextBytes = data.getBytes("UTF-8");
    CryptoInfo encrypted = CryptoInfo.encrypt(cleartextBytes, keyBundle);

    ExtendedJSONObject payload = new ExtendedJSONObject();

    String message64 = new String(Base64.encodeBase64(encrypted.getMessage()));
    String iv64 = new String(Base64.encodeBase64(encrypted.getIV()));

    payload.put(Constants.JSON_KEY_CIPHERTEXT, message64);
    payload.put(Constants.JSON_KEY_IV, iv64);
    if (makeHmac) {
      String hmacHex = Utils.byte2hex(encrypted.getHMAC());
      payload.put(Constants.JSON_KEY_HMAC, hmacHex);
    }
    return payload;
  }

  /*
   * Helper for turning a JSON object into a payload.
   *
   * @param body JSONObject body to be converted to StringEntity.
   * @return StringEntity representation of JSONObject.
   *
   * @throws UnsupportedEncodingException
   */
  public static StringEntity jsonEntity(JSONObject body)
      throws UnsupportedEncodingException {
    StringEntity entity = new StringEntity(body.toJSONString(), "UTF-8");
    entity.setContentType("application/json");
    return entity;
  }

  /*
   * Controller methods.
   */
  public void makeAndDisplayPin(String channel) {
    controllerActivity.displayPin(secret + channel);
  }

  public void onPairingStart() {
    Logger.debug(LOG_TAG, "Pairing started.");
    controllerActivity.onPairingStart();
  }

  public void onPaired() {
    Logger.debug(LOG_TAG, "Pairing completed. Starting credential exchange.");
    controllerActivity.onPaired();
  }

  public void complete(JSONObject credentials) {
    controllerActivity.onComplete(credentials);
  }

  /*
   * Called from controller, with Sync credentials to be encrypted and sent.
   */
  public void sendAndComplete(JSONObject jObj)
      throws JPakeNoActivePairingException {
    if (!paired || finished) {
      Logger.error(LOG_TAG, "Can't send data, no active pairing!");
      throw new JPakeNoActivePairingException();
    }
    stages.clear();
    stages.add(new PutRequestStage());
    stages.add(new CompleteStage());

    // Encrypt data to send and set as jOutgoing.
    String outData = jObj.toJSONString();
    encryptData(myKeyBundle, outData);

    // Start stages for sending credentials.
    runNextStage();
  }

  /* Setup helper functions */

  /*
   * Generates and sets a clientId for communications with JPAKE setup server.
   */
  private void setClientId() {
    byte[] rBytes = Utils.generateRandomBytes(JPAKE_LENGTH_CLIENTID / 2);
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

    byte[] rBytes = Utils.generateRandomBytes(JPAKE_LENGTH_SECRET);
    StringBuilder secret = new StringBuilder();
    for (byte b : rBytes) {
      secret.append(key.charAt(Math.abs(b) * keylen / 256));
    }
    this.secret = secret.toString();
  }

  /*
   *
   * Encrypt payload and package into jOutgoing for sending with a PUT request.
   *
   * @param keyBundle Encryption keys derived during J-PAKE.
   *
   * @param payload Credentials data to be encrypted.
   */
  private void encryptData(KeyBundle keyBundle, String payload) {
    Logger.debug(LOG_TAG, "Encrypting data.");
    ExtendedJSONObject jPayload = null;
    try {
      jPayload = encryptPayload(payload, keyBundle, true);
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "Failed to encrypt data.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    } catch (CryptoException e) {
      Logger.error(LOG_TAG, "Failed to encrypt data.", e);
      abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }
    jOutgoing = new ExtendedJSONObject();
    jOutgoing.put(Constants.JSON_KEY_TYPE, mySignerId + "3");
    jOutgoing.put(Constants.JSON_KEY_VERSION, KEYEXCHANGE_VERSION);
    jOutgoing.put(Constants.JSON_KEY_PAYLOAD, jPayload.object);
  }
}
