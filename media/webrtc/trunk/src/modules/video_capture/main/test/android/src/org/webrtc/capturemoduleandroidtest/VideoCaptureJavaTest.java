package org.webrtc.capturemoduleandroidtest;

import java.util.List;

import android.content.Context;
import android.util.Log;

import org.webrtc.videoengine.CaptureCapabilityAndroid;
import org.webrtc.videoengine.VideoCaptureAndroid;
import org.webrtc.videoengine.VideoCaptureDeviceInfoAndroid;

public class VideoCaptureJavaTest {
  void DoTest(Context context)
  {
    VideoCaptureDeviceInfoAndroid videoCaptureDeviceInfo =
        VideoCaptureDeviceInfoAndroid.CreateVideoCaptureDeviceInfoAndroid(
            5,context);
    for(int i = 0; i < videoCaptureDeviceInfo.NumberOfDevices(); i++) {
      String deviceUniqueId=videoCaptureDeviceInfo.GetDeviceUniqueName(i);
      VideoCaptureAndroid videoCapture =
          videoCaptureDeviceInfo.AllocateCamera(i,0,deviceUniqueId);

      CaptureCapabilityAndroid capArray[] =
          videoCaptureDeviceInfo.GetCapabilityArray(deviceUniqueId);
      for(CaptureCapabilityAndroid cap: capArray) {
        Log.d("*WEBRTC*", "Capability widht" + cap.width +
              " height " +cap.height+ " frameRate " +cap.maxFPS);
        int result=videoCapture.StartCapture(cap.width,
                                             cap.height,
                                             cap.maxFPS);
        try{
          Thread.sleep(2000);//sleep for 2000 ms
        }
        catch(InterruptedException ie){
          //If this thread was interrupted by another thread
        }
        result+=videoCapture.StopCapture();
        Log.d("*WEBRTC*", "Start stop result " + result);
      }
      VideoCaptureAndroid.DeleteVideoCaptureAndroid(videoCapture);
      videoCapture=null;
    }
    Log.d("*WEBRTC*", "Test complete");
  }

  VideoCaptureDeviceInfoAndroid _videoCaptureDeviceInfo;
  VideoCaptureAndroid _videoCapture;
  void StartCapture(Context context) {
    _videoCaptureDeviceInfo =
        VideoCaptureDeviceInfoAndroid.CreateVideoCaptureDeviceInfoAndroid(
            5,context);
    String deviceUniqueId=_videoCaptureDeviceInfo.GetDeviceUniqueName(0);
    _videoCapture=_videoCaptureDeviceInfo.AllocateCamera(5,0,deviceUniqueId);
    _videoCapture.StartCapture(176,144,15);
  }
  void StopCapture() {
    _videoCapture.StopCapture();
    VideoCaptureAndroid.DeleteVideoCaptureAndroid(_videoCapture);
 }

}
