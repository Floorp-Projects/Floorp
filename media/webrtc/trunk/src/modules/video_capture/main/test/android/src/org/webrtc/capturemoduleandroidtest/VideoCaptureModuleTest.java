package org.webrtc.capturemoduleandroidtest;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import org.webrtc.capturemoduleandroidtest.R;
import org.webrtc.videoengine.ViERenderer;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.LinearLayout;

public class VideoCaptureModuleTest
    extends Activity implements OnClickListener {
  // Set to 1 if OpenGL shall be used. 0 Otherwise
  private final int _useOpenGL=1;
  private Thread _testThread;
  private SurfaceView _view=null;
  private VideoCaptureModuleTest _thisPointer;
  private VideoCaptureJavaTest _videoCaptureJavaTest;
  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.main);

    final Button buttonStartCP = (Button) findViewById(R.id.Button01);
    buttonStartCP.setOnClickListener(this);
    final Button buttonStartJava = (Button) findViewById(R.id.Button02);
    buttonStartJava.setOnClickListener(this);
    final Button buttonStartCPP = (Button) findViewById(R.id.Button03);
    buttonStartCPP.setOnClickListener(this);
    final Button buttonStopCPP = (Button) findViewById(R.id.Button04);
    buttonStopCPP.setOnClickListener(this);
  }

  private Runnable _testProc = new Runnable() {
      public void run() {
        // TODO: choose test from GUI
        // Select test here, 0 for API test, 1-> for Func tests
        RunTest(_view);
      }
    };

  @Override
  protected void onStart()
  {
    super.onStart();
  }
  @Override
  protected void onRestart()
  {
    super.onRestart();
  }
  @Override
  protected void onPause()
  {
    super.onPause();
  }
  @Override
  protected void onStop()
  {
    super.onStop();
  }

  // Function used to call test
  private native int RunTest(Object view);
  private native int RenderInit(Object view);

  private native int StartCapture();
  private native int StopCapture();

  static {
    Log.d("*WEBRTC*",
          "Loading ModuleVideoCaptureModuleAndroidTest...");
    System.loadLibrary(
        "ModuleVideoCaptureModuleAndroidTestJniAPI");
  }

  public void onClick(View v) {
    //get the handle to the layout
    LinearLayout renderLayout=(LinearLayout) findViewById(R.id.renderView);
    switch(v.getId())
    {
      case R.id.Button01:
        renderLayout.removeAllViews();
        _view=ViERenderer.CreateLocalRenderer(this);
        if(_useOpenGL==1)
        {
          _view= ViERenderer.CreateRenderer(this, true);
        }
        else
        {
          _view= new SurfaceView(this);
        }
        // add the surfaceview to the layout,
        // the surfaceview will be the same size as the layout (container)
        renderLayout.addView(_view);
        RenderInit(_view);
        _testThread = new Thread(_testProc);
        _testThread.start();
        break;
      case R.id.Button02:
        _view=ViERenderer.CreateLocalRenderer(this);
        renderLayout.removeAllViews();
        if(_videoCaptureJavaTest==null)
        {
          _videoCaptureJavaTest=new VideoCaptureJavaTest();
          _videoCaptureJavaTest.StartCapture(this);
          // add the surfaceview to the layout,
          // the surfaceview will be the same size as the layout (container)
          renderLayout.addView(_view);
        }
        else
        {
          _videoCaptureJavaTest.StopCapture();
          _videoCaptureJavaTest=null;
        }
        break;

      case R.id.Button03:
        _view=ViERenderer.CreateLocalRenderer(this);
        renderLayout.removeAllViews();
        StartCapture();
        // add the surfaceview to the layout,
        // the surfaceview will be the same size as the layout (container)
        renderLayout.addView(_view);
        break;
      case R.id.Button04:
        renderLayout.removeAllViews();
        StopCapture();
        break;
    }
  }
}