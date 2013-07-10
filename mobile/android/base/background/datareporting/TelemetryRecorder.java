/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.datareporting;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.json.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;

import android.util.Base64;

/**
 * Writes telemetry ping to file.
 *
 * Also creates and updates a SHA-256 checksum for the payload to be included in the ping
 * file.
 *
 * A saved telemetry ping file consists of JSON in the following format,
 *   {
 *     "slug": "<uuid-string>",
 *     "payload": "<escaped-json-data-string>",
 *     "checksum": "<base64-sha-256-string>"
 *   }
 *
 * This class writes first to a temporary file and then, after finishing the contents of the ping,
 * moves that to the file specified by the caller. This is to avoid uploads of partially written
 * ping files.
 *
 * The API provided by this class:
 * startPingFile() - opens stream to a tmp File in the Android cache directory and writes the slug header
 * appendPayload(String payloadContent) - appends to the payload of the ping and updates the checksum
 * finishPingFile() - writes the checksum to the tmp file and moves it to the File specified by the caller.
 *
 * In the case of errors, we try to close the stream and File.
 */
public class TelemetryRecorder {
  private final String LOG_TAG = "TelemetryRecorder";

  private final File parentDir;
  private final String filename;

  private File tmpFile;
  private File destFile;
  private File cacheDir;

  private OutputStream  outputStream;
  private MessageDigest checksum;
  private String        base64Checksum;

  /**
   * Charset to use for writing pings; default is us-ascii.
   *
   * When telemetry calculates the checksum for the ping file, it lossily
   * converts utf-16 to ascii. Therefore we have to treat characters in the
   * traces as ascii rather than say utf-8. Otherwise we will get a "wrong"
   * checksum.
   */
  private String charset = "us-ascii";

  /**
   * Override blockSize in constructor if desired.
   * Default block size is that of BufferedOutputStream.
   */
  private int blockSize = 0;

  /**
   * Constructs a TelemetryRecorder for writing a ping file. A temporary file will be written first,
   * and then moved to the destination file location specified by the caller.
   *
   * The directory for writing the temporary file is highly suggested to be the Android internal cache directory,
   * fetched by <code>context.getCacheDir()</code>
   *
   * If the destination file already exists, it will be deleted and overwritten.
   *
   * Default charset: "us-ascii"
   * Default block size: uses constructor default of 8192 bytes (see javadocs for
   *                     <code>BufferedOutputStream</code>
   * @param parentPath
   *        path of parent directory of ping file to be written
   * @param cacheDir
   *        path of cache directory for writing temporary files.
   * @param filename
   *        name of ping file to be written
   */
  public TelemetryRecorder(File parentDir, File cacheDir, String filename) {
    if (!parentDir.isDirectory()) {
      throw new IllegalArgumentException("Expecting directory, got non-directory File instead.");
    }
    this.parentDir = parentDir;
    this.filename = filename;
    this.cacheDir = cacheDir;
  }

  public TelemetryRecorder(File parentDir, File cacheDir, String filename, String charset) {
    this(parentDir, cacheDir, filename);
    this.charset = charset;
  }

  public TelemetryRecorder(File parentDir, File cacheDir, String filename, String charset, int blockSize) {
    this(parentDir, cacheDir, filename, charset);
    this.blockSize = blockSize;
  }

  /**
   * Start the temporary ping file and write the "slug" header and payload key, of the
   * format:
   *
   *     { "slug": "< filename >", "payload":
   *
   * @throws Exception
   *           Checked exceptions <code>NoSuchAlgorithmException</code>,
   *           <code>UnsupportedEncodingException</code>, or
   *           <code>IOException</code> and unchecked exception that
   *           are rethrown to caller
   */
  public void startPingFile() throws Exception {

    // Open stream to temporary file for writing.
    try {
      tmpFile = File.createTempFile(filename, "tmp", cacheDir);
    } catch (IOException e) {
      // Try to create the temporary file in the ping directory.
      tmpFile = new File(parentDir, filename + ".tmp");
      try {
        tmpFile.createNewFile();
      } catch (IOException e1) {
        cleanUpAndRethrow("Failed to create tmp file in temp directory or ping directory.", e1);
      }
    }

    try {
      if (blockSize > 0) {
        outputStream = new BufferedOutputStream(new FileOutputStream(tmpFile), blockSize);
      } else {
        outputStream = new BufferedOutputStream(new FileOutputStream(tmpFile));
      }

      // Create checksum for ping.
      checksum = MessageDigest.getInstance("SHA-256");

      // Write ping header.
      byte[] header = makePingHeader(filename);
      outputStream.write(header);
      Logger.debug(LOG_TAG, "Wrote " + header.length + " header bytes.");

    } catch (NoSuchAlgorithmException e) {
      cleanUpAndRethrow("Error creating checksum digest", e);
    } catch (UnsupportedEncodingException e) {
      cleanUpAndRethrow("Error writing header", e);
    } catch (IOException e) {
      cleanUpAndRethrow("Error writing to stream", e);
    }
  }

  private byte[] makePingHeader(String slug)
      throws UnsupportedEncodingException {
    return ("{\"slug\":" + JSONObject.quote(slug) + "," + "\"payload\":\"")
        .getBytes(charset);
  }

  /**
   * Append payloadContent to ping file and update the checksum.
   *
   * @param payloadContent
   *          String content to be written
   * @return number of bytes written, or -1 if writing failed
   * @throws Exception
   *           Checked exceptions <code>UnsupportedEncodingException</code> or
   *           <code>IOException</code> and unchecked exception that
   *           are rethrown to caller
   */
  public int appendPayload(String payloadContent) throws Exception {
    if (payloadContent == null) {
      cleanUpAndRethrow("Payload is null", new Exception());
      return -1;
    }

    try {
      byte[] payloadBytes = payloadContent.getBytes(charset);
      // If we run into an error, we'll throw and abort, so checksum won't be stale.
      checksum.update(payloadBytes);

      byte[] quotedPayloadBytes = JSONObject.quote(payloadContent).getBytes(charset);

      // First and last bytes are quotes inserted by JSONObject.quote; discard
      // them.
      int numBytes = quotedPayloadBytes.length - 2;
      outputStream.write(quotedPayloadBytes, 1, numBytes);
      return numBytes;

    } catch (UnsupportedEncodingException e) {
      cleanUpAndRethrow("Error encoding payload", e);
      return -1;
    } catch (IOException e) {
      cleanUpAndRethrow("Error writing to stream", e);
      return -1;
    }
  }

  /**
   * Add the checksum of the payload to the ping file and close the stream.
   *
   * @throws Exception
   *          Checked exceptions <code>UnsupportedEncodingException</code> or
   *          <code>IOException</code> and unchecked exception that
   *          are rethrown to caller
   */
  public void finishPingFile() throws Exception {
    try {
      byte[] footer = makePingFooter(checksum);
      outputStream.write(footer);
      // We're done writing, so force the stream to flush the buffer.
      outputStream.flush();
      Logger.debug(LOG_TAG, "Wrote " + footer.length + " footer bytes.");
    } catch (UnsupportedEncodingException e) {
      cleanUpAndRethrow("Checksum encoding exception", e);
    } catch (IOException e) {
      cleanUpAndRethrow("Error writing footer to stream", e);
    } finally {
      try {
        outputStream.close();
      } catch (IOException e) {
        // Failed to close, nothing we can do except discard the reference to the stream.
        outputStream = null;
      }
    }

    // Move temp file to destination specified by caller.
    try {
      File destFile = new File(parentDir, filename);
      // Delete file if it exists - docs state that rename may fail if the File already exists.
      if (destFile.exists()) {
        destFile.delete();
      }
      boolean result = tmpFile.renameTo(destFile);
      if (!result) {
        throw new IOException("Could not move tmp file to destination.");
      }
    } finally {
      cleanUp();
    }
  }

  private byte[] makePingFooter(MessageDigest checksum)
      throws UnsupportedEncodingException {
    base64Checksum = Base64.encodeToString(checksum.digest(), Base64.NO_WRAP);
    return ("\",\"checksum\":" + JSONObject.quote(base64Checksum) + "}")
        .getBytes(charset);
  }

  /**
   * Get final digested Base64 checksum.
   *
   * @return String checksum if it has been calculated, null otherwise.
   */
  protected String getFinalChecksum() {
    return base64Checksum;
  }

  public String getCharset() {
    return this.charset;
  }

  /**
   * Clean up checksum and delete the temporary file.
   */
  private void cleanUp() {
    // Discard checksum.
    checksum.reset();

    // Clean up files.
    if (tmpFile != null && tmpFile.exists()) {
      tmpFile.delete();
    }
    tmpFile = null;
  }

  /**
   * Log message and error and clean up, then rethrow exception to caller.
   *
   * @param message
   *          Error message
   * @param e
   *          Exception
   *
   * @throws Exception
   *           Exception to be rethrown to caller
   */
  private void cleanUpAndRethrow(String message, Exception e) throws Exception {
    Logger.error(LOG_TAG, message, e);
    cleanUp();

    if (outputStream != null) {
      try {
        outputStream.close();
      } catch (IOException exception) {
        // Failed to close stream; nothing we can do, and we're aborting anyways.
      }
    }

    if (destFile != null && destFile.exists()) {
      destFile.delete();
    }
    // Rethrow the exception.
    throw e;
  }
}
