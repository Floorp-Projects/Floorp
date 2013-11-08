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

import android.app.AlertDialog;
import android.app.TabActivity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.OrientationEventListener;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TabHost;
import android.widget.TabHost.TabSpec;
import android.widget.TextView;

import org.webrtc.videoengine.ViERenderer;

import java.io.File;
import java.io.IOException;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;

/** {@} */
public class WebRTCDemo extends TabActivity implements IViEAndroidCallback,
                                                View.OnClickListener,
                                                OnItemSelectedListener {
    private ViEAndroidJavaAPI vieAndroidAPI = null;

    // remote renderer
    private SurfaceView remoteSurfaceView = null;

    // local renderer and camera
    private SurfaceView svLocal = null;

    // channel number
    private int channel = -1;
    private int cameraId;
    private int voiceChannel = -1;

    // flags
    private boolean viERunning = false;
    private boolean voERunning = false;

    // debug
    private boolean enableTrace = true;

    // Constant
    private static final String TAG = "WEBRTC";
    private static final int RECEIVE_CODEC_FRAMERATE = 15;
    private static final int SEND_CODEC_FRAMERATE = 15;
    private static final int INIT_BITRATE = 500;
    private static final String LOOPBACK_IP = "127.0.0.1";
    // Zero means don't automatically start/stop calls.
    private static final long AUTO_CALL_RESTART_DELAY_MS = 0;

    private Handler handler = new Handler();
    private Runnable startOrStopCallback = new Runnable() {
            public void run() {
                startOrStop();
            }
        };

    private int volumeLevel = 204;

    private TabHost mTabHost = null;

    private TabSpec mTabSpecConfig;
    private TabSpec mTabSpecVideo;

    private LinearLayout mLlRemoteSurface = null;
    private LinearLayout mLlLocalSurface = null;

    private Button btStartStopCall;
    private Button btSwitchCamera;

    // Global Settings
    private CheckBox cbVideoSend;
    private boolean enableVideoSend = true;
    private CheckBox cbVideoReceive;
    private boolean enableVideoReceive = true;
    private boolean enableVideo = true;
    private CheckBox cbVoice;
    private boolean enableVoice = true;
    private EditText etRemoteIp;
    private String remoteIp = "";
    private CheckBox cbLoopback;
    private boolean loopbackMode = true;
    private CheckBox cbStats;
    private boolean isStatsOn = true;
    public enum RenderType {
        OPENGL,
        SURFACE,
        MEDIACODEC
    }
    RenderType renderType = RenderType.OPENGL;

    // Video settings
    private Spinner spCodecType;
    private int codecType = 0;
    private Spinner spCodecSize;
    private int codecSizeWidth = 0;
    private int codecSizeHeight = 0;
    private TextView etVRxPort;
    private int receivePortVideo = 11111;
    private TextView etVTxPort;
    private int destinationPortVideo = 11111;
    private CheckBox cbEnableNack;
    private boolean enableNack = true;
    private CheckBox cbEnableVideoRTPDump;

    // Audio settings
    private Spinner spVoiceCodecType;
    private int voiceCodecType = 0;
    private TextView etARxPort;
    private int receivePortVoice = 11113;
    private TextView etATxPort;
    private int destinationPortVoice = 11113;
    private CheckBox cbEnableSpeaker;
    private CheckBox cbEnableAGC;
    private boolean enableAGC = false;
    private CheckBox cbEnableAECM;
    private boolean enableAECM = false;
    private CheckBox cbEnableNS;
    private boolean enableNS = false;
    private CheckBox cbEnableDebugAPM;
    private CheckBox cbEnableVoiceRTPDump;

    // Stats variables
    private int frameRateI;
    private int bitRateI;
    private int packetLoss;
    private int frameRateO;
    private int bitRateO;
    private int numCalls = 0;

    private int widthI;
    private int heightI;

    // Variable for storing variables
    private String webrtcName = "/webrtc";
    private String webrtcDebugDir = null;

    private WakeLock wakeLock;

    private boolean usingFrontCamera = true;
    // The orientations (in degrees) of each of the cameras CCW-relative to the
    // device, indexed by CameraInfo.CAMERA_FACING_{BACK,FRONT}, and -1
    // for unrepresented |facing| values (i.e. single-camera device).
    private int[] cameraOrientations = new int[] { -1, -1 };

    private String[] mVideoCodecsStrings = null;
    private String[] mVideoCodecsSizeStrings = { "176x144", "320x240",
                                                 "352x288", "640x480" };
    private String[] mVoiceCodecsStrings = null;

    private OrientationEventListener orientationListener;
    int currentDeviceOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;

    private StatsView statsView = null;

    private BroadcastReceiver receiver;

    // Rounds rotation to the nearest 90 degree rotation.
    private static int roundRotation(int rotation) {
        return (int)(Math.round((double)rotation / 90) * 90) % 360;
    }

    // Populate |cameraOrientations| with the first cameras that have each of
    // the facing values.
    private void populateCameraOrientations() {
        CameraInfo info = new CameraInfo();
        for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
            Camera.getCameraInfo(i, info);
            if (cameraOrientations[info.facing] != -1) {
                continue;
            }
            cameraOrientations[info.facing] = info.orientation;
        }
    }

    // Return the |CameraInfo.facing| value appropriate for |usingFrontCamera|.
    private static int facingOf(boolean usingFrontCamera) {
        return usingFrontCamera ? CameraInfo.CAMERA_FACING_FRONT
                : CameraInfo.CAMERA_FACING_BACK;
    }

    // This function ensures that egress streams always send real world up
    // streams.
    // Note: There are two components of the camera rotation. The rotation of
    // the capturer relative to the device. I.e. up for the camera might not be
    // device up. When rotating the device the camera is also rotated.
    // The former is called orientation and the second is called rotation here.
    public void compensateCameraRotation() {
        int cameraOrientation = cameraOrientations[facingOf(usingFrontCamera)];
        // The device orientation is the device's rotation relative to its
        // natural position.
        int cameraRotation = roundRotation(currentDeviceOrientation);

        int totalCameraRotation = 0;
        if (usingFrontCamera) {
            // The front camera rotates in the opposite direction of the
            // device.
            int inverseCameraRotation = (360 - cameraRotation) % 360;
            totalCameraRotation =
                (inverseCameraRotation + cameraOrientation) % 360;
        } else {
            totalCameraRotation =
                (cameraRotation + cameraOrientation) % 360;
        }
        vieAndroidAPI.SetRotation(cameraId, totalCameraRotation);
    }

    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");

        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        populateCameraOrientations();

        PowerManager pm = (PowerManager) this.getSystemService(
            Context.POWER_SERVICE);
        wakeLock = pm.newWakeLock(
            PowerManager.SCREEN_DIM_WAKE_LOCK, TAG);

        setContentView(R.layout.tabhost);

        IntentFilter receiverFilter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);

        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.getAction().compareTo(Intent.ACTION_HEADSET_PLUG)
                        == 0) {
                    int state = intent.getIntExtra("state", 0);
                    Log.v(TAG, "Intent.ACTION_HEADSET_PLUG state: " + state +
                         " microphone: " + intent.getIntExtra("microphone", 0));
                    if (voERunning) {
                        routeAudio(state == 0 && cbEnableSpeaker.isChecked());
                    }
                }
            }
        };
        registerReceiver(receiver, receiverFilter);

        mTabHost = getTabHost();

        // Main tab
        mTabSpecVideo = mTabHost.newTabSpec("tab_video");
        mTabSpecVideo.setIndicator("Main");
        mTabSpecVideo.setContent(R.id.tab_video);
        mTabHost.addTab(mTabSpecVideo);

        // Shared config tab
        mTabHost = getTabHost();
        mTabSpecConfig = mTabHost.newTabSpec("tab_config");
        mTabSpecConfig.setIndicator("Settings");
        mTabSpecConfig.setContent(R.id.tab_config);
        mTabHost.addTab(mTabSpecConfig);

        TabSpec mTabv;
        mTabv = mTabHost.newTabSpec("tab_vconfig");
        mTabv.setIndicator("Video");
        mTabv.setContent(R.id.tab_vconfig);
        mTabHost.addTab(mTabv);
        TabSpec mTaba;
        mTaba = mTabHost.newTabSpec("tab_aconfig");
        mTaba.setIndicator("Audio");
        mTaba.setContent(R.id.tab_aconfig);
        mTabHost.addTab(mTaba);

        int childCount = mTabHost.getTabWidget().getChildCount();
        for (int i = 0; i < childCount; i++) {
            mTabHost.getTabWidget().getChildAt(i).getLayoutParams().height = 50;
        }
        orientationListener =
                new OrientationEventListener(this, SensorManager.SENSOR_DELAY_UI) {
                    public void onOrientationChanged (int orientation) {
                        if (orientation != ORIENTATION_UNKNOWN) {
                            currentDeviceOrientation = orientation;
                            compensateCameraRotation();
                        }
                    }
                };
        orientationListener.enable ();

        // Create a folder named webrtc in /scard for debugging
        webrtcDebugDir = Environment.getExternalStorageDirectory().toString() +
                webrtcName;
        File webrtcDir = new File(webrtcDebugDir);
        if (!webrtcDir.exists() && webrtcDir.mkdir() == false) {
            Log.v(TAG, "Failed to create " + webrtcDebugDir);
        } else if (!webrtcDir.isDirectory()) {
            Log.v(TAG, webrtcDebugDir + " exists but not a folder");
            webrtcDebugDir = null;
        }

        startMain();

        if (AUTO_CALL_RESTART_DELAY_MS > 0)
            startOrStop();
    }

    // Called before the activity is destroyed.
    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        handler.removeCallbacks(startOrStopCallback);
        unregisterReceiver(receiver);
        super.onDestroy();
    }

    private class StatsView extends View{
        public StatsView(Context context){
            super(context);
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            // Only draw Stats in Main tab.
            if(mTabHost.getCurrentTabTag() == "tab_video") {
                Paint loadPaint = new Paint();
                loadPaint.setAntiAlias(true);
                loadPaint.setTextSize(16);
                loadPaint.setARGB(255, 255, 255, 255);

                canvas.drawText("#calls " + numCalls, 4, 222, loadPaint);

                String loadText;
                loadText = "> " + frameRateI + " fps/" +
                           bitRateI/1024 + " kbps/ " + packetLoss;
                canvas.drawText(loadText, 4, 242, loadPaint);
                loadText = "< " + frameRateO + " fps/ " +
                           bitRateO/1024 + " kbps";
                canvas.drawText(loadText, 4, 262, loadPaint);
                loadText = "Incoming resolution " + widthI + "x" + heightI;
                canvas.drawText(loadText, 4, 282, loadPaint);
            }
            updateDisplay();
        }

        void updateDisplay() {
            invalidate();
        }
    }

    private String getLocalIpAddress() {
        String localIPs = "";
        try {
            for (Enumeration<NetworkInterface> en = NetworkInterface
                         .getNetworkInterfaces(); en.hasMoreElements();) {
                NetworkInterface intf = en.nextElement();
                for (Enumeration<InetAddress> enumIpAddr =
                             intf.getInetAddresses();
                     enumIpAddr.hasMoreElements(); ) {
                    InetAddress inetAddress = enumIpAddr.nextElement();
                    if (!inetAddress.isLoopbackAddress()) {
                        localIPs +=
                                inetAddress.getHostAddress().toString() + " ";
                        // Set the remote ip address the same as
                        // the local ip address of the last netif
                        remoteIp = inetAddress.getHostAddress().toString();
                    }
                }
            }
        } catch (SocketException ex) {
            Log.e(TAG, ex.toString());
        }
        return localIPs;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (viERunning) {
                stopAll();
                startMain();
            }
            finish();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void stopAll() {
        Log.d(TAG, "stopAll");

        if (vieAndroidAPI != null) {

            if (voERunning) {
                voERunning = false;
                stopVoiceEngine();
            }

            if (viERunning) {
                viERunning = false;
                vieAndroidAPI.StopRender(channel);
                vieAndroidAPI.StopReceive(channel);
                vieAndroidAPI.StopSend(channel);
                vieAndroidAPI.RemoveRemoteRenderer(channel);
                vieAndroidAPI.ViE_DeleteChannel(channel);
                channel = -1;
                vieAndroidAPI.StopCamera(cameraId);
                vieAndroidAPI.Terminate();
                mLlRemoteSurface.removeView(remoteSurfaceView);
                mLlLocalSurface.removeView(svLocal);
                remoteSurfaceView = null;
                svLocal = null;
            }
        }
    }

    /** {@ArrayAdapter} */
    public class SpinnerAdapter extends ArrayAdapter<String> {
        private String[] mCodecString = null;
        public SpinnerAdapter(Context context, int textViewResourceId, String[] objects) {
            super(context, textViewResourceId, objects);
            mCodecString = objects;
        }

        @Override public View getDropDownView(int position, View convertView, ViewGroup parent) {
            return getCustomView(position, convertView, parent);
        }

        @Override public View getView(int position, View convertView, ViewGroup parent) {
            return getCustomView(position, convertView, parent);
        }

        public View getCustomView(int position, View convertView, ViewGroup parent) {
            LayoutInflater inflater = getLayoutInflater();
            View row = inflater.inflate(R.layout.row, parent, false);
            TextView label = (TextView) row.findViewById(R.id.spinner_row);
            label.setText(mCodecString[position]);
            return row;
        }
    }

    private void startMain() {
        mTabHost.setCurrentTab(0);

        mLlRemoteSurface = (LinearLayout) findViewById(R.id.llRemoteView);
        mLlLocalSurface = (LinearLayout) findViewById(R.id.llLocalView);

        if (null == vieAndroidAPI) {
            vieAndroidAPI = new ViEAndroidJavaAPI(this);
        }
        if (0 > setupVoE() || 0 > vieAndroidAPI.GetVideoEngine() ||
                0 > vieAndroidAPI.Init(enableTrace)) {
            // Show dialog
            AlertDialog alertDialog = new AlertDialog.Builder(this).create();
            alertDialog.setTitle("WebRTC Error");
            alertDialog.setMessage("Can not init video engine.");
            alertDialog.setButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        return;
                    } });
            alertDialog.show();
        }

        btSwitchCamera = (Button) findViewById(R.id.btSwitchCamera);
        if (cameraOrientations[0] != -1 && cameraOrientations[1] != -1) {
            btSwitchCamera.setOnClickListener(this);
        } else {
            btSwitchCamera.setEnabled(false);
        }
        btStartStopCall = (Button) findViewById(R.id.btStartStopCall);
        btStartStopCall.setOnClickListener(this);
        findViewById(R.id.btExit).setOnClickListener(this);

        // cleaning
        remoteSurfaceView = null;
        svLocal = null;

        // Video codec
        mVideoCodecsStrings = vieAndroidAPI.GetCodecs();
        spCodecType = (Spinner) findViewById(R.id.spCodecType);
        spCodecType.setOnItemSelectedListener(this);
        spCodecType.setAdapter(new SpinnerAdapter(this,
                        R.layout.row,
                        mVideoCodecsStrings));
        spCodecType.setSelection(0);

        // Video Codec size
        spCodecSize = (Spinner) findViewById(R.id.spCodecSize);
        spCodecSize.setOnItemSelectedListener(this);
        spCodecSize.setAdapter(new SpinnerAdapter(this,
                        R.layout.row,
                        mVideoCodecsSizeStrings));
        spCodecSize.setSelection(mVideoCodecsSizeStrings.length - 1);

        // Voice codec
        mVoiceCodecsStrings = vieAndroidAPI.VoE_GetCodecs();
        spVoiceCodecType = (Spinner) findViewById(R.id.spVoiceCodecType);
        spVoiceCodecType.setOnItemSelectedListener(this);
        spVoiceCodecType.setAdapter(new SpinnerAdapter(this,
                        R.layout.row,
                        mVoiceCodecsStrings));
        spVoiceCodecType.setSelection(0);
        // Find ISAC and use it
        for (int i = 0; i < mVoiceCodecsStrings.length; ++i) {
            if (mVoiceCodecsStrings[i].contains("ISAC")) {
                spVoiceCodecType.setSelection(i);
                break;
            }
        }

        RadioGroup radioGroup = (RadioGroup) findViewById(R.id.radio_group1);
        radioGroup.clearCheck();
        if (renderType == RenderType.OPENGL) {
            radioGroup.check(R.id.radio_opengl);
        } else if (renderType == RenderType.SURFACE) {
            radioGroup.check(R.id.radio_surface);
        } else if (renderType == RenderType.MEDIACODEC) {
            radioGroup.check(R.id.radio_mediacodec);
        }

        etRemoteIp = (EditText) findViewById(R.id.etRemoteIp);
        etRemoteIp.setText(remoteIp);

        cbLoopback = (CheckBox) findViewById(R.id.cbLoopback);
        cbLoopback.setChecked(loopbackMode);

        cbStats = (CheckBox) findViewById(R.id.cbStats);
        cbStats.setChecked(isStatsOn);

        cbVoice = (CheckBox) findViewById(R.id.cbVoice);
        cbVoice.setChecked(enableVoice);

        cbVideoSend = (CheckBox) findViewById(R.id.cbVideoSend);
        cbVideoSend.setChecked(enableVideoSend);
        cbVideoReceive = (CheckBox) findViewById(R.id.cbVideoReceive);
        cbVideoReceive.setChecked(enableVideoReceive);

        etVTxPort = (EditText) findViewById(R.id.etVTxPort);
        etVTxPort.setText(Integer.toString(destinationPortVideo));

        etVRxPort = (EditText) findViewById(R.id.etVRxPort);
        etVRxPort.setText(Integer.toString(receivePortVideo));

        etATxPort = (EditText) findViewById(R.id.etATxPort);
        etATxPort.setText(Integer.toString(destinationPortVoice));

        etARxPort = (EditText) findViewById(R.id.etARxPort);
        etARxPort.setText(Integer.toString(receivePortVoice));

        cbEnableNack = (CheckBox) findViewById(R.id.cbNack);
        cbEnableNack.setChecked(enableNack);

        cbEnableSpeaker = (CheckBox) findViewById(R.id.cbSpeaker);
        cbEnableAGC = (CheckBox) findViewById(R.id.cbAutoGainControl);
        cbEnableAGC.setChecked(enableAGC);
        cbEnableAECM = (CheckBox) findViewById(R.id.cbAECM);
        cbEnableAECM.setChecked(enableAECM);
        cbEnableNS = (CheckBox) findViewById(R.id.cbNoiseSuppression);
        cbEnableNS.setChecked(enableNS);

        cbEnableDebugAPM = (CheckBox) findViewById(R.id.cbDebugRecording);
        cbEnableDebugAPM.setChecked(false);  // Disable APM debugging by default

        cbEnableVideoRTPDump = (CheckBox) findViewById(R.id.cbVideoRTPDump);
        cbEnableVideoRTPDump.setChecked(false);  // Disable Video RTP Dump

        cbEnableVoiceRTPDump = (CheckBox) findViewById(R.id.cbVoiceRTPDump);
        cbEnableVoiceRTPDump.setChecked(false);  // Disable Voice RTP Dump

        etRemoteIp.setOnClickListener(this);
        cbLoopback.setOnClickListener(this);
        cbStats.setOnClickListener(this);
        cbEnableNack.setOnClickListener(this);
        cbEnableSpeaker.setOnClickListener(this);
        cbEnableAECM.setOnClickListener(this);
        cbEnableAGC.setOnClickListener(this);
        cbEnableNS.setOnClickListener(this);
        cbEnableDebugAPM.setOnClickListener(this);
        cbEnableVideoRTPDump.setOnClickListener(this);
        cbEnableVoiceRTPDump.setOnClickListener(this);

        if (loopbackMode) {
            remoteIp = LOOPBACK_IP;
            etRemoteIp.setText(remoteIp);
        } else {
            getLocalIpAddress();
            etRemoteIp.setText(remoteIp);
        }

        // Read settings to refresh each configuration
        readSettings();
    }

    private String getRemoteIPString() {
        return etRemoteIp.getText().toString();
    }

    private void startCall() {
        int ret = 0;

        if (enableVoice) {
            startVoiceEngine();
        }

        if (enableVideo) {
            if (enableVideoSend) {
                // camera and preview surface
                svLocal = ViERenderer.CreateLocalRenderer(this);
            }

            channel = vieAndroidAPI.CreateChannel(voiceChannel);
            ret = vieAndroidAPI.SetLocalReceiver(channel,
                                                 receivePortVideo);
            ret = vieAndroidAPI.SetSendDestination(channel,
                                                   destinationPortVideo,
                                                   getRemoteIPString());

            if (enableVideoReceive) {
                if (renderType == RenderType.OPENGL) {
                    Log.v(TAG, "Create OpenGL Render");
                    remoteSurfaceView = ViERenderer.CreateRenderer(this, true);
                } else if (renderType == RenderType.SURFACE) {
                    Log.v(TAG, "Create SurfaceView Render");
                    remoteSurfaceView = ViERenderer.CreateRenderer(this, false);
                } else if (renderType == RenderType.MEDIACODEC) {
                    Log.v(TAG, "Create MediaCodec Decoder/Renderer");
                    remoteSurfaceView = new SurfaceView(this);
                }

                if (mLlRemoteSurface != null) {
                    mLlRemoteSurface.addView(remoteSurfaceView);
                }

                if (renderType == RenderType.MEDIACODEC) {
                    ret = vieAndroidAPI.SetExternalMediaCodecDecoderRenderer(
                            channel, remoteSurfaceView);
                } else {
                    ret = vieAndroidAPI.AddRemoteRenderer(channel, remoteSurfaceView);
                }

                ret = vieAndroidAPI.SetReceiveCodec(channel,
                        codecType,
                        INIT_BITRATE,
                        codecSizeWidth,
                        codecSizeHeight,
                        RECEIVE_CODEC_FRAMERATE);
                ret = vieAndroidAPI.StartRender(channel);
                ret = vieAndroidAPI.StartReceive(channel);
            }

            if (enableVideoSend) {
                ret = vieAndroidAPI.SetSendCodec(channel, codecType, INIT_BITRATE,
                        codecSizeWidth, codecSizeHeight, SEND_CODEC_FRAMERATE);
                int camId = vieAndroidAPI.StartCamera(channel, usingFrontCamera ? 1 : 0);

                if (camId >= 0) {
                    cameraId = camId;
                    compensateCameraRotation();
                } else {
                    ret = camId;
                }
                ret = vieAndroidAPI.StartSend(channel);
            }

            // TODO(leozwang): Add more options besides PLI, currently use pli
            // as the default. Also check return value.
            ret = vieAndroidAPI.EnablePLI(channel, true);
            ret = vieAndroidAPI.EnableNACK(channel, enableNack);
            ret = vieAndroidAPI.SetCallback(channel, this);

            if (enableVideoSend) {
                if (mLlLocalSurface != null) {
                    mLlLocalSurface.addView(svLocal);
                }
            }

            isStatsOn = cbStats.isChecked();
            if (isStatsOn) {
                addStatusView();
            } else {
                removeStatusView();
            }

            viERunning = true;
        }
    }

    private void stopVoiceEngine() {
        // Stop send
        if (0 != vieAndroidAPI.VoE_StopSend(voiceChannel)) {
            Log.d(TAG, "VoE stop send failed");
        }

        // Stop listen
        if (0 != vieAndroidAPI.VoE_StopListen(voiceChannel)) {
            Log.d(TAG, "VoE stop listen failed");
        }

        // Stop playout
        if (0 != vieAndroidAPI.VoE_StopPlayout(voiceChannel)) {
            Log.d(TAG, "VoE stop playout failed");
        }

        if (0 != vieAndroidAPI.VoE_DeleteChannel(voiceChannel)) {
            Log.d(TAG, "VoE delete channel failed");
        }
        voiceChannel = -1;

        // Terminate
        if (0 != vieAndroidAPI.VoE_Terminate()) {
            Log.d(TAG, "VoE terminate failed");
        }
    }

    private int setupVoE() {
        // Create VoiceEngine
        // Error logging is done in native API wrapper
        vieAndroidAPI.VoE_Create(getApplicationContext());

        // Initialize
        if (0 != vieAndroidAPI.VoE_Init(enableTrace)) {
            Log.d(TAG, "VoE init failed");
            return -1;
        }

        // Suggest to use the voice call audio stream for hardware volume controls
        setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);
        return 0;
    }

    private int startVoiceEngine() {
        // Create channel
        voiceChannel = vieAndroidAPI.VoE_CreateChannel();
        if (0 > voiceChannel) {
            Log.d(TAG, "VoE create channel failed");
            return -1;
        }

        // Set local receiver
        if (0 != vieAndroidAPI.VoE_SetLocalReceiver(voiceChannel,
                        receivePortVoice)) {
            Log.d(TAG, "VoE set local receiver failed");
        }

        if (0 != vieAndroidAPI.VoE_StartListen(voiceChannel)) {
            Log.d(TAG, "VoE start listen failed");
        }

        // Route audio
        routeAudio(cbEnableSpeaker.isChecked());

        // set volume to default value
        if (0 != vieAndroidAPI.VoE_SetSpeakerVolume(volumeLevel)) {
            Log.d(TAG, "VoE set speaker volume failed");
        }

        // Start playout
        if (0 != vieAndroidAPI.VoE_StartPlayout(voiceChannel)) {
            Log.d(TAG, "VoE start playout failed");
        }

        if (0 != vieAndroidAPI.VoE_SetSendDestination(voiceChannel,
                                                      destinationPortVoice,
                                                      getRemoteIPString())) {
            Log.d(TAG, "VoE set send  destination failed");
        }

        if (0 != vieAndroidAPI.VoE_SetSendCodec(voiceChannel, voiceCodecType)) {
            Log.d(TAG, "VoE set send codec failed");
        }

        if (0 != vieAndroidAPI.VoE_SetECStatus(enableAECM)) {
            Log.d(TAG, "VoE set EC Status failed");
        }

        if (0 != vieAndroidAPI.VoE_SetAGCStatus(enableAGC)) {
            Log.d(TAG, "VoE set AGC Status failed");
        }

        if (0 != vieAndroidAPI.VoE_SetNSStatus(enableNS)) {
            Log.d(TAG, "VoE set NS Status failed");
        }

        if (0 != vieAndroidAPI.VoE_StartSend(voiceChannel)) {
            Log.d(TAG, "VoE start send failed");
        }

        voERunning = true;
        return 0;
    }

    private void routeAudio(boolean enableSpeaker) {
        if (0 != vieAndroidAPI.VoE_SetLoudspeakerStatus(enableSpeaker)) {
            Log.d(TAG, "VoE set louspeaker status failed");
        }
    }

    private void startOrStop() {
        readSettings();
        if (viERunning || voERunning) {
            stopAll();
            startMain();
            wakeLock.release(); // release the wake lock
            btStartStopCall.setText(R.string.startCall);
        } else if (enableVoice || enableVideo){
            ++numCalls;
            startCall();
            wakeLock.acquire(); // screen stay on during the call
            btStartStopCall.setText(R.string.stopCall);
        }
        if (AUTO_CALL_RESTART_DELAY_MS > 0) {
            handler.postDelayed(startOrStopCallback, AUTO_CALL_RESTART_DELAY_MS);
        }
    }

    public void onClick(View arg0) {
        switch (arg0.getId()) {
            case R.id.btSwitchCamera:
                if (usingFrontCamera) {
                    btSwitchCamera.setText(R.string.frontCamera);
                } else {
                    btSwitchCamera.setText(R.string.backCamera);
                }
                usingFrontCamera = !usingFrontCamera;

                if (viERunning) {
                    vieAndroidAPI.StopCamera(cameraId);
                    mLlLocalSurface.removeView(svLocal);

                    vieAndroidAPI.StartCamera(channel, usingFrontCamera ? 1 : 0);
                    mLlLocalSurface.addView(svLocal);
                    compensateCameraRotation();
                }
                break;
            case R.id.btStartStopCall:
              startOrStop();
              break;
            case R.id.btExit:
                stopAll();
                finish();
                break;
            case R.id.cbLoopback:
                loopbackMode  = cbLoopback.isChecked();
                if (loopbackMode) {
                    remoteIp = LOOPBACK_IP;
                    etRemoteIp.setText(LOOPBACK_IP);
                } else {
                    getLocalIpAddress();
                    etRemoteIp.setText(remoteIp);
                }
                break;
            case R.id.etRemoteIp:
                remoteIp = etRemoteIp.getText().toString();
                break;
            case R.id.cbStats:
                isStatsOn = cbStats.isChecked();
                if (isStatsOn) {
                    addStatusView();
                } else {
                    removeStatusView();
                }
                break;
            case R.id.radio_surface:
                renderType = RenderType.SURFACE;
                break;
            case R.id.radio_opengl:
                renderType = RenderType.OPENGL;
                break;
            case R.id.radio_mediacodec:
                renderType = RenderType.MEDIACODEC;
                break;
            case R.id.cbNack:
                enableNack  = cbEnableNack.isChecked();
                if (viERunning) {
                    vieAndroidAPI.EnableNACK(channel, enableNack);
                }
                break;
            case R.id.cbSpeaker:
                if (voERunning) {
                    routeAudio(cbEnableSpeaker.isChecked());
                }
                break;
            case R.id.cbDebugRecording:
                if (voERunning && webrtcDebugDir != null) {
                    if (cbEnableDebugAPM.isChecked()) {
                        vieAndroidAPI.VoE_StartDebugRecording(
                            webrtcDebugDir + String.format("/apm_%d.dat",
                                    System.currentTimeMillis()));
                    } else {
                        vieAndroidAPI.VoE_StopDebugRecording();
                    }
                }
                break;
            case R.id.cbVoiceRTPDump:
                if (voERunning && webrtcDebugDir != null) {
                    if (cbEnableVoiceRTPDump.isChecked()) {
                        vieAndroidAPI.VoE_StartIncomingRTPDump(channel,
                                webrtcDebugDir + String.format("/voe_%d.rtp",
                                        System.currentTimeMillis()));
                    } else {
                        vieAndroidAPI.VoE_StopIncomingRTPDump(channel);
                    }
                }
                break;
            case R.id.cbVideoRTPDump:
                if (viERunning && webrtcDebugDir != null) {
                    if (cbEnableVideoRTPDump.isChecked()) {
                        vieAndroidAPI.StartIncomingRTPDump(channel,
                                webrtcDebugDir + String.format("/vie_%d.rtp",
                                        System.currentTimeMillis()));
                    } else {
                        vieAndroidAPI.StopIncomingRTPDump(channel);
                    }
                }
                break;
            case R.id.cbAutoGainControl:
                enableAGC = cbEnableAGC.isChecked();
                if (voERunning) {
                    vieAndroidAPI.VoE_SetAGCStatus(enableAGC);
                }
                break;
            case R.id.cbNoiseSuppression:
                enableNS = cbEnableNS.isChecked();
                if (voERunning) {
                    vieAndroidAPI.VoE_SetNSStatus(enableNS);
                }
                break;
            case R.id.cbAECM:
                enableAECM = cbEnableAECM.isChecked();
                if (voERunning) {
                    vieAndroidAPI.VoE_SetECStatus(enableAECM);
                }
                break;
        }
    }

    private void readSettings() {
        codecType = spCodecType.getSelectedItemPosition();
        voiceCodecType = spVoiceCodecType.getSelectedItemPosition();

        String sCodecSize = spCodecSize.getSelectedItem().toString();
        String[] aCodecSize = sCodecSize.split("x");
        codecSizeWidth = Integer.parseInt(aCodecSize[0]);
        codecSizeHeight = Integer.parseInt(aCodecSize[1]);

        loopbackMode  = cbLoopback.isChecked();
        enableVoice  = cbVoice.isChecked();
        enableVideoSend = cbVideoSend.isChecked();
        enableVideoReceive = cbVideoReceive.isChecked();
        enableVideo = enableVideoSend || enableVideoReceive;

        destinationPortVideo =
                Integer.parseInt(etVTxPort.getText().toString());
        receivePortVideo =
                Integer.parseInt(etVRxPort.getText().toString());
        destinationPortVoice =
                Integer.parseInt(etATxPort.getText().toString());
        receivePortVoice =
                Integer.parseInt(etARxPort.getText().toString());

        enableNack  = cbEnableNack.isChecked();
        enableAGC  = cbEnableAGC.isChecked();
        enableAECM  = cbEnableAECM.isChecked();
        enableNS  = cbEnableNS.isChecked();
    }

    public void onItemSelected(AdapterView<?> adapterView, View view,
            int position, long id) {
        if ((adapterView == spCodecType || adapterView == spCodecSize) &&
                viERunning) {
            readSettings();
            // change the codectype
            if (enableVideoReceive) {
                if (0 != vieAndroidAPI.SetReceiveCodec(channel, codecType,
                                INIT_BITRATE, codecSizeWidth,
                                codecSizeHeight,
                                RECEIVE_CODEC_FRAMERATE)) {
                    Log.d(TAG, "ViE set receive codec failed");
                }
            }
            if (enableVideoSend) {
                if (0 != vieAndroidAPI.SetSendCodec(channel, codecType,
                                INIT_BITRATE, codecSizeWidth, codecSizeHeight,
                                SEND_CODEC_FRAMERATE)) {
                    Log.d(TAG, "ViE set send codec failed");
                }
            }
        } else if ((adapterView == spVoiceCodecType) && voERunning) {
            // change voice engine codec
            readSettings();
            if (0 != vieAndroidAPI.VoE_SetSendCodec(voiceChannel, voiceCodecType)) {
                Log.d(TAG, "VoE set send codec failed");
            }
        }
    }

    public void onNothingSelected(AdapterView<?> arg0) {
        Log.d(TAG, "No setting selected");
    }

    public int updateStats(int inFrameRateI, int inBitRateI,
            int inPacketLoss, int inFrameRateO, int inBitRateO) {
        frameRateI = inFrameRateI;
        bitRateI = inBitRateI;
        packetLoss = inPacketLoss;
        frameRateO = inFrameRateO;
        bitRateO = inBitRateO;
        return 0;
    }

    public int newIncomingResolution(int width, int height) {
        widthI = width;
        heightI = height;
        return 0;
    }

    private void addStatusView() {
        if (statsView != null) {
            return;
        }
        statsView = new StatsView(this);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
            PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.RIGHT | Gravity.TOP;
        params.setTitle("Load Average");
        mTabHost.addView(statsView, params);
        statsView.setBackgroundColor(0);
    }

    private void removeStatusView() {
        mTabHost.removeView(statsView);
        statsView = null;
    }

}
