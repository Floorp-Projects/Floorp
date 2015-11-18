/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import java.lang.System;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

import android.content.Context;
import android.media.AudioFormat;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

class  WebRtcAudioRecord {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioRecord";

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

  // Requested size of each recorded buffer provided to the client.
  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  // Average number of callbacks per second.
  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  private final long nativeAudioRecord;
  private final Context context;

  private ByteBuffer byteBuffer;

  private AudioRecord audioRecord = null;
  private AudioRecordThread audioThread = null;

  private AcousticEchoCanceler aec = null;
  private boolean useBuiltInAEC = false;

  /**
   * Audio thread which keeps calling ByteBuffer.read() waiting for audio
   * to be recorded. Feeds recorded data to the native counterpart as a
   * periodic sequence of callbacks using DataIsRecorded().
   * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
   */
  private class AudioRecordThread extends Thread {
    private volatile boolean keepAlive = true;

    public AudioRecordThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logd("AudioRecordThread" + WebRtcAudioUtils.getThreadInfo());

      try {
        audioRecord.startRecording();
      } catch (IllegalStateException e) {
          Loge("AudioRecord.startRecording failed: " + e.getMessage());
        return;
      }
      assertTrue(audioRecord.getRecordingState()
          == AudioRecord.RECORDSTATE_RECORDING);

      long lastTime = System.nanoTime();
      while (keepAlive) {
        int bytesRead = audioRecord.read(byteBuffer, byteBuffer.capacity());
        if (bytesRead == byteBuffer.capacity()) {
          nativeDataIsRecorded(bytesRead, nativeAudioRecord);
        } else {
          Loge("AudioRecord.read failed: " + bytesRead);
          if (bytesRead == AudioRecord.ERROR_INVALID_OPERATION) {
            keepAlive = false;
          }
        }
        if (DEBUG) {
          long nowTime = System.nanoTime();
          long durationInMs =
              TimeUnit.NANOSECONDS.toMillis((nowTime - lastTime));
          lastTime = nowTime;
          Logd("bytesRead[" + durationInMs + "] " + bytesRead);
        }
      }

      try {
        audioRecord.stop();
      } catch (IllegalStateException e) {
        Loge("AudioRecord.stop failed: " + e.getMessage());
      }
    }

    public void joinThread() {
      keepAlive = false;
      while (isAlive()) {
        try {
          join();
        } catch (InterruptedException e) {
          // Ignore.
        }
      }
    }
  }

  WebRtcAudioRecord(Context context, long nativeAudioRecord) {
    Logd("ctor" + WebRtcAudioUtils.getThreadInfo());
    this.context = context;
    this.nativeAudioRecord = nativeAudioRecord;
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
  }

  public static boolean BuiltInAECIsAvailable() {
    // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
    if (!WebRtcAudioUtils.runningOnJellyBeanOrHigher()) {
      return false;
    }
    // TODO(henrika): add black-list based on device name. We could also
    // use uuid to exclude devices but that would require a session ID from
    // an existing AudioRecord object.
    return AcousticEchoCanceler.isAvailable();
  }

  private boolean EnableBuiltInAEC(boolean enable) {
    Logd("EnableBuiltInAEC(" + enable + ')');
    // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
    if (!WebRtcAudioUtils.runningOnJellyBeanOrHigher()) {
      return false;
    }
    // Store the AEC state.
    useBuiltInAEC = enable;
    // Set AEC state if AEC has already been created.
    if (aec != null) {
      int ret = aec.setEnabled(enable);
      if (ret != AudioEffect.SUCCESS) {
        Loge("AcousticEchoCanceler.setEnabled failed");
        return false;
      }
      Logd("AcousticEchoCanceler.getEnabled: " + aec.getEnabled());
    }
    return true;
  }

  private int InitRecording(int sampleRate, int channels) {
    Logd("InitRecording(sampleRate=" + sampleRate + ", channels=" +
        channels + ")");
    final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
    final int framesPerBuffer = sampleRate / BUFFERS_PER_SECOND;
    byteBuffer = byteBuffer.allocateDirect(bytesPerFrame * framesPerBuffer);
    Logd("byteBuffer.capacity: " + byteBuffer.capacity());
    // Rather than passing the ByteBuffer with every callback (requiring
    // the potentially expensive GetDirectBufferAddress) we simply have the
    // the native class cache the address to the memory once.
    nativeCacheDirectBufferAddress(byteBuffer, nativeAudioRecord);

    // Get the minimum buffer size required for the successful creation of
    // an AudioRecord object, in byte units.
    // Note that this size doesn't guarantee a smooth recording under load.
    // TODO(henrika): Do we need to make this larger to avoid underruns?
    int minBufferSize = AudioRecord.getMinBufferSize(
          sampleRate,
          AudioFormat.CHANNEL_IN_MONO,
          AudioFormat.ENCODING_PCM_16BIT);
    Logd("AudioRecord.getMinBufferSize: " + minBufferSize);

    if (aec != null) {
      aec.release();
      aec = null;
    }
    assertTrue(audioRecord == null);

    int bufferSizeInBytes = Math.max(byteBuffer.capacity(), minBufferSize);
    Logd("bufferSizeInBytes: " + bufferSizeInBytes);
    try {
      audioRecord = new AudioRecord(AudioSource.VOICE_COMMUNICATION,
                                    sampleRate,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT,
                                    bufferSizeInBytes);

    } catch (IllegalArgumentException e) {
      Logd(e.getMessage());
      return -1;
    }
    assertTrue(audioRecord.getState() == AudioRecord.STATE_INITIALIZED);

    Logd("AudioRecord " +
          "session ID: " + audioRecord.getAudioSessionId() + ", " +
          "audio format: " + audioRecord.getAudioFormat() + ", " +
          "channels: " + audioRecord.getChannelCount() + ", " +
          "sample rate: " + audioRecord.getSampleRate());
    Logd("AcousticEchoCanceler.isAvailable: " + BuiltInAECIsAvailable());
    if (!BuiltInAECIsAvailable()) {
      return framesPerBuffer;
    }

    aec = AcousticEchoCanceler.create(audioRecord.getAudioSessionId());
    if (aec == null) {
      Loge("AcousticEchoCanceler.create failed");
      return -1;
    }
    int ret = aec.setEnabled(useBuiltInAEC);
    if (ret != AudioEffect.SUCCESS) {
      Loge("AcousticEchoCanceler.setEnabled failed");
      return -1;
    }
    Descriptor descriptor = aec.getDescriptor();
    Logd("AcousticEchoCanceler " +
          "name: " + descriptor.name + ", " +
          "implementor: " + descriptor.implementor + ", " +
          "uuid: " + descriptor.uuid);
    Logd("AcousticEchoCanceler.getEnabled: " + aec.getEnabled());
    return framesPerBuffer;
  }

  private boolean StartRecording() {
    Logd("StartRecording");
    assertTrue(audioRecord != null);
    assertTrue(audioThread == null);
    audioThread = new AudioRecordThread("AudioRecordJavaThread");
    audioThread.start();
    return true;
  }

  private boolean StopRecording() {
    Logd("StopRecording");
    assertTrue(audioThread != null);
    audioThread.joinThread();
    audioThread = null;
    if (aec != null) {
      aec.release();
      aec = null;
    }
    audioRecord.release();
    audioRecord = null;
    return true;
  }

  /** Helper method which throws an exception  when an assertion has failed. */
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private static void Logd(String msg) {
    Log.d(TAG, msg);
  }

  private static void Loge(String msg) {
    Log.e(TAG, msg);
  }

  private native void nativeCacheDirectBufferAddress(
      ByteBuffer byteBuffer, long nativeAudioRecord);

  private native void nativeDataIsRecorded(int bytes, long nativeAudioRecord);
}
