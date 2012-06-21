/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengineapp;

import android.app.Activity;
import android.content.Context;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class ViEAndroidJavaAPI {

    public ViEAndroidJavaAPI(Context context) {
        Log.d("*WEBRTCJ*", "Loading ViEAndroidJavaAPI...");
        System.loadLibrary("webrtc-video-demo-jni");

        Log.d("*WEBRTCJ*", "Calling native init...");
        if (!NativeInit(context)) {
            Log.e("*WEBRTCJ*", "Native init failed");
            throw new RuntimeException("Native init failed");
        }
        else {
            Log.d("*WEBRTCJ*", "Native init successful");
        }
        String a = "";
        a.getBytes();
    }

    // API Native
    private native boolean NativeInit(Context context);

    // Video Engine API
    // Initialization and Termination functions
    public native int GetVideoEngine();
    public native int Init(boolean enableTrace);
    public native int Terminate();


    public native int StartSend(int channel);
    public native int StopRender(int channel);
    public native int StopSend(int channel);
    public native int StartReceive(int channel);
    public native int StopReceive(int channel);
    // Channel functions
    public native int CreateChannel(int voiceChannel);
    // Receiver & Destination functions
    public native int SetLocalReceiver(int channel, int port);
    public native int SetSendDestination(int channel, int port, byte ipadr[]);
    // Codec
    public native int SetReceiveCodec(int channel, int codecNum,
            int intbitRate, int width,
            int height, int frameRate);
    public native int SetSendCodec(int channel, int codecNum,
            int intbitRate, int width,
            int height, int frameRate);
    // Rendering
    public native int AddRemoteRenderer(int channel, Object glSurface);
    public native int RemoveRemoteRenderer(int channel);
    public native int StartRender(int channel);

    // Capture
    public native int StartCamera(int channel, int cameraNum);
    public native int StopCamera(int cameraId);
    public native int GetCameraOrientation(int cameraNum);
    public native int SetRotation(int cameraId,int degrees);

    // NACK
    public native int EnableNACK(int channel, boolean enable);

    //PLI for H.264
    public native int EnablePLI(int channel, boolean enable);

    // Enable stats callback
    public native int SetCallback(int channel, IViEAndroidCallback callback);

    // Voice Engine API
    // Create and Delete functions
    public native boolean VoE_Create(Activity context);
    public native boolean VoE_Delete();

    // Initialization and Termination functions
    public native int VoE_Authenticate(String key);
    public native int VoE_Init(boolean enableTrace);
    public native int VoE_Terminate();

    // Channel functions
    public native int VoE_CreateChannel();
    public native int VoE_DeleteChannel(int channel);

    // Receiver & Destination functions
    public native int VoE_SetLocalReceiver(int channel, int port);
    public native int VoE_SetSendDestination(int channel, int port,
            String ipaddr);

    // Media functions
    public native int VoE_StartListen(int channel);
    public native int VoE_StartPlayout(int channel);
    public native int VoE_StartSend(int channel);
    public native int VoE_StopListen(int channel);
    public native int VoE_StopPlayout(int channel);
    public native int VoE_StopSend(int channel);

    // Volume
    public native int VoE_SetSpeakerVolume(int volume);

    // Hardware
    public native int VoE_SetLoudspeakerStatus(boolean enable);

    // Playout file locally
    public native int VoE_StartPlayingFileLocally(int channel,
            String fileName,
            boolean loop);
    public native int VoE_StopPlayingFileLocally(int channel);

    // Play file as microphone
    public native int VoE_StartPlayingFileAsMicrophone(int channel,
            String fileName,
            boolean loop);
    public native int VoE_StopPlayingFileAsMicrophone(int channel);

    // Codec-setting functions
    public native int VoE_NumOfCodecs();
    public native int VoE_SetSendCodec(int channel, int index);

    //VE funtions
    public native int VoE_SetECStatus(boolean enable, int mode,
            int AESmode, int AESattenuation);
    public native int VoE_SetAGCStatus(boolean enable, int mode);
    public native int VoE_SetNSStatus(boolean enable, int mode);
}
