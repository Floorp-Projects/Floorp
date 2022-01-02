/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import java.util.concurrent.ConcurrentLinkedQueue;

public interface BaseHlsPlayer {

  public enum TrackType {
    UNDEFINED,
    AUDIO,
    VIDEO,
    TEXT,
  }

  public enum ResourceError {
    BASE(-100),
    UNKNOWN(-101),
    PLAYER(-102),
    UNSUPPORTED(-103);

    private int mNumVal;

    private ResourceError(final int numVal) {
      mNumVal = numVal;
    }

    public int code() {
      return mNumVal;
    }
  }

  public enum DemuxerError {
    BASE(-200),
    UNKNOWN(-201),
    PLAYER(-202),
    UNSUPPORTED(-203);

    private int mNumVal;

    private DemuxerError(final int numVal) {
      mNumVal = numVal;
    }

    public int code() {
      return mNumVal;
    }
  }

  public interface DemuxerCallbacks {
    void onInitialized(boolean hasAudio, boolean hasVideo);

    void onError(int errorCode);
  }

  public interface ResourceCallbacks {
    void onLoad(String mediaUrl);

    void onDataArrived();

    void onError(int errorCode);
  }

  // Used to identify player instance.
  public int getId();

  // =======================================================================
  // API for GeckoHLSResourceWrapper
  // =======================================================================
  public void init(String url, ResourceCallbacks callback);

  public boolean isLiveStream();

  // =======================================================================
  // API for GeckoHLSDemuxerWrapper
  // =======================================================================
  public void addDemuxerWrapperCallbackListener(DemuxerCallbacks callback);

  public ConcurrentLinkedQueue<GeckoHLSSample> getSamples(TrackType trackType, int number);

  public long getBufferedPosition();

  public int getNumberOfTracks(TrackType trackType);

  public GeckoVideoInfo getVideoInfo(int index);

  public GeckoAudioInfo getAudioInfo(int index);

  public boolean seek(long positionUs);

  public long getNextKeyFrameTime();

  public void suspend();

  public void resume();

  public void play();

  public void pause();

  public void release();
}
