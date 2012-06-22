/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once
#include "StdAfx.h"
#include "WindowsTestResource.h"

#include "ChannelDlg.h"
#include "CaptureDevicePool.h"
#include "ChannelPool.h"

//Forward declarations
namespace webrtc {
    class VideoEngine;
    class VoiceEngine;
}
using namespace webrtc;
class CDXCaptureDlg;


class WindowsTestMainDlg : public CDialog, private CDXChannelDlgObserver
{
	DECLARE_DYNAMIC(WindowsTestMainDlg)

public:
	WindowsTestMainDlg(VideoEngine* videoEngine,void* voiceEngine=NULL,CWnd* pParent = NULL);   // standard constructor
	virtual ~WindowsTestMainDlg();

// Dialog Data
	enum { IDD = IDD_WINDOWSTEST_MAIN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
     afx_msg void OnBnClickedChannel1();
     afx_msg void OnBnClickedChannel2();
     afx_msg void OnBnClickedChannel3();
     afx_msg void OnBnClickedChannel4();


     VideoEngine* _videoEngine;
    VoiceEngine*		_voiceEngine;
    VoEBase* _veBase;

    CDXChannelDlg* _testDlg1;
    CDXChannelDlg* _testDlg2;
    CDXChannelDlg* _testDlg3;
    CDXChannelDlg* _testDlg4;

    int _externalInWidth;   
    int _externalInHeight;
    int _externalInVideoType;

    CaptureDevicePool _captureDevicePool;
    ChannelPool       _channelPool;


private:
    virtual void ChannelDialogEnded(CDXChannelDlg* context);

public:

};
