/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// WindowsTestMainDlg.cpp : implementation file
//
#include "WindowsTestMainDlg.h"
#include "WindowsTest.h"
#include "ChannelDlg.h"

#include "voe_base.h"

// WindowsTestMainDlg dialog

IMPLEMENT_DYNAMIC(WindowsTestMainDlg, CDialog)

WindowsTestMainDlg::WindowsTestMainDlg(VideoEngine* videoEngine,void* voiceEngine,CWnd* pParent /*=NULL*/)
	: CDialog(WindowsTestMainDlg::IDD, pParent),
        _videoEngine(videoEngine),
        _voiceEngine((VoiceEngine*) voiceEngine),
        _testDlg1(NULL),
        _testDlg2(NULL),
        _testDlg3(NULL),
        _testDlg4(NULL),    
        _externalInWidth(0),   
        _externalInHeight(0),    
        _externalInVideoType(0),
        _captureDevicePool(videoEngine)
{
    
}

WindowsTestMainDlg::~WindowsTestMainDlg()
{        
}

void WindowsTestMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(WindowsTestMainDlg, CDialog)
        ON_BN_CLICKED(IDC_CHANNEL1, &WindowsTestMainDlg::OnBnClickedChannel1)
        ON_BN_CLICKED(IDC_CHANNEL2, &WindowsTestMainDlg::OnBnClickedChannel2)
        ON_BN_CLICKED(IDC_CHANNEL3, &WindowsTestMainDlg::OnBnClickedChannel3)
        ON_BN_CLICKED(IDC_CHANNEL4, &WindowsTestMainDlg::OnBnClickedChannel4)
END_MESSAGE_MAP()



void WindowsTestMainDlg::OnBnClickedChannel1()
{
    if(!_testDlg1)
    {
        _testDlg1=new CDXChannelDlg(_videoEngine,_captureDevicePool,_channelPool,_voiceEngine,NULL,this);
        _testDlg1->Create(CDXChannelDlg::IDD,this);
    }
    else
    {
        _testDlg1->SetActiveWindow();
    }    
}

void WindowsTestMainDlg::OnBnClickedChannel2()
{
    if(!_testDlg2)
    {
        _testDlg2=new CDXChannelDlg(_videoEngine,_captureDevicePool,_channelPool,_voiceEngine,NULL,this);
        _testDlg2->Create(CDXChannelDlg::IDD,this);

    }
    else
    {
        _testDlg2->SetActiveWindow();
    }    
}

void WindowsTestMainDlg::ChannelDialogEnded(CDXChannelDlg* context)
{
    if(context==_testDlg4)
    {
        delete _testDlg4;
        _testDlg4=NULL;
    }
    else if(context==_testDlg3)
    {
        delete _testDlg3;
        _testDlg3=NULL;
    }
    else if(context==_testDlg2)
    {
        delete _testDlg2;
        _testDlg2=NULL;
    }
    else if(context==_testDlg1)
    {
        delete _testDlg1;
        _testDlg1=NULL;
    }
    else // Slave channel
    {
        delete context;
    }

}



void WindowsTestMainDlg::OnBnClickedChannel3()
{
    if(!_testDlg3)
    {
        _testDlg3=new CDXChannelDlg(_videoEngine,_captureDevicePool,_channelPool,_voiceEngine,NULL,this);
        _testDlg3->Create(CDXChannelDlg::IDD,this);

    }
    else
    {
        _testDlg3->SetActiveWindow();
    }    
}

void WindowsTestMainDlg::OnBnClickedChannel4()
{
    if(!_testDlg4)
    {
        _testDlg4=new CDXChannelDlg(_videoEngine,_captureDevicePool,_channelPool,_voiceEngine,NULL,this);
        _testDlg4->Create(CDXChannelDlg::IDD,this);

    }
    else
    {
        _testDlg4->SetActiveWindow();
    }                
}
