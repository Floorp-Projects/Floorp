package org.mozilla.gecko;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;

/** Provides transparent access to either a SurfaceView or TextureView */
public class SurfaceViewWrapper {
  private static final String LOGTAG = "SurfaceViewWrapper";

  private ListenerWrapper mListenerWrapper;
  private View mView;

  // Only one of these will be non-null at any point in time
  SurfaceView mSurfaceView;
  TextureView mTextureView;

  public SurfaceViewWrapper(final Context context) {
    // By default, use SurfaceView
    mListenerWrapper = new ListenerWrapper();
    initSurfaceView(context);
  }

  private void initSurfaceView(final Context context) {
    mSurfaceView = new SurfaceView(context);
    mSurfaceView.setBackgroundColor(Color.TRANSPARENT);
    mSurfaceView.getHolder().setFormat(PixelFormat.TRANSPARENT);
    mView = mSurfaceView;
  }

  public void useSurfaceView(final Context context) {
    if (mTextureView != null) {
      mListenerWrapper.onSurfaceTextureDestroyed(mTextureView.getSurfaceTexture());
      mTextureView = null;
    }
    mListenerWrapper.reset();
    initSurfaceView(context);
  }

  public void useTextureView(final Context context) {
    if (mSurfaceView != null) {
      mListenerWrapper.surfaceDestroyed(mSurfaceView.getHolder());
      mSurfaceView = null;
    }
    mListenerWrapper.reset();
    mTextureView = new TextureView(context);
    mTextureView.setSurfaceTextureListener(mListenerWrapper);
    mView = mTextureView;
  }

  public void setBackgroundColor(final int color) {
    if (mSurfaceView != null) {
      mSurfaceView.setBackgroundColor(color);
    } else {
      Log.e(LOGTAG, "TextureView doesn't support background color.");
    }
  }

  public void setListener(final Listener listener) {
    mListenerWrapper.mListener = listener;
    mSurfaceView.getHolder().addCallback(mListenerWrapper);
  }

  public int getWidth() {
    if (mSurfaceView != null) {
      return mSurfaceView.getHolder().getSurfaceFrame().right;
    }
    return mListenerWrapper.mWidth;
  }

  public int getHeight() {
    if (mSurfaceView != null) {
      return mSurfaceView.getHolder().getSurfaceFrame().bottom;
    }
    return mListenerWrapper.mHeight;
  }

  public Surface getSurface() {
    if (mSurfaceView != null) {
      return mSurfaceView.getHolder().getSurface();
    }

    return mListenerWrapper.mSurface;
  }

  public View getView() {
    return mView;
  }

  /**
   * Translates SurfaceTextureListener and SurfaceHolder.Callback into a common interface
   * SurfaceViewWrapper.Listener
   */
  private static class ListenerWrapper
      implements TextureView.SurfaceTextureListener, SurfaceHolder.Callback {
    private Listener mListener;

    // TextureView doesn't provide getters for these so we keep track of them here
    private Surface mSurface;
    private int mWidth;
    private int mHeight;

    public void reset() {
      mWidth = 0;
      mHeight = 0;
      mSurface = null;
    }

    // TextureView
    @Override
    public void onSurfaceTextureAvailable(
        final SurfaceTexture surface, final int width, final int height) {
      mSurface = new Surface(surface);
      mWidth = width;
      mHeight = height;
      if (mListener != null) {
        mListener.onSurfaceChanged(mSurface, width, height);
      }
    }

    @Override
    public void onSurfaceTextureSizeChanged(
        final SurfaceTexture surface, final int width, final int height) {
      mWidth = width;
      mHeight = height;
      if (mListener != null) {
        mListener.onSurfaceChanged(mSurface, mWidth, mHeight);
      }
    }

    @Override
    public boolean onSurfaceTextureDestroyed(final SurfaceTexture surface) {
      if (mListener != null) {
        mListener.onSurfaceDestroyed();
      }
      mSurface = null;
      return false;
    }

    @Override
    public void onSurfaceTextureUpdated(final SurfaceTexture surface) {
      mSurface = new Surface(surface);
      if (mListener != null) {
        mListener.onSurfaceChanged(mSurface, mWidth, mHeight);
      }
    }

    // SurfaceView
    @Override
    public void surfaceCreated(final SurfaceHolder holder) {}

    @Override
    public void surfaceChanged(
        final SurfaceHolder holder, final int format, final int width, final int height) {
      if (mListener != null) {
        mListener.onSurfaceChanged(holder.getSurface(), width, height);
      }
    }

    @Override
    public void surfaceDestroyed(final SurfaceHolder holder) {
      if (mListener != null) {
        mListener.onSurfaceDestroyed();
      }
    }
  }

  public interface Listener {
    void onSurfaceChanged(Surface surface, int width, int height);

    void onSurfaceDestroyed();
  }
}
