/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "WindowsTest.h"
#include "ChannelDlg.h"
#include "WindowsTestMainDlg.h"
#include "engine_configurations.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Check memory leaks id running debug
#if (defined(_DEBUG) && defined(_WIN32))
//    #include "vld.h"
#endif
/////////////////////////////////////////////////////////////////////////////
// CDXWindowsTestApp

BEGIN_MESSAGE_MAP(CDXWindowsTestApp, CWinApp)
	//{{AFX_MSG_MAP(CDXWindowsTestApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDXWindowsTestApp construction

CDXWindowsTestApp::CDXWindowsTestApp()
{
    
}

/////////////////////////////////////////////////////////////////////////////
// The one and only object

CDXWindowsTestApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CDXWindowsTestApp initialization

BOOL CDXWindowsTestApp::InitInstance()
{
    int result=0;
    #ifndef NO_VOICE_ENGINE
        _voiceEngine = VoiceEngine::Create();
        _veBase = VoEBase::GetInterface(_voiceEngine);
         result+=_veBase->Init();
     #else
        _voiceEngine=NULL;
    #endif

    _videoEngine = VideoEngine::Create();

    _videoEngine->SetTraceFilter(webrtc::kTraceDefault);//webrtc::kTraceDebug | webrtc::kTraceError | webrtc::kTraceApiCall | webrtc::kTraceWarning | webrtc::kTraceCritical | webrtc::kTraceStateInfo | webrtc::kTraceInfo | webrtc::kTraceStream);
    _videoEngine->SetTraceFile("trace.txt");
    
    ViEBase* vieBase=ViEBase::GetInterface(_videoEngine);
    result+=vieBase->Init();
    if(result!=0)
    {
        ::MessageBox (NULL, (LPCTSTR)("failed to init VideoEngine"), TEXT("Error Message"),  MB_OK | MB_ICONINFORMATION);                
    }
    
    {
      WindowsTestMainDlg dlg(_videoEngine,_voiceEngine);

      m_pMainWnd = &dlg;
      dlg.DoModal();
    }
    
    vieBase->Release();

    if(!VideoEngine::Delete(_videoEngine))
    {
        char errorMsg[255];
        sprintf(errorMsg,"All VideoEngine interfaces are not released properly!");
        ::MessageBox (NULL, (LPCTSTR)errorMsg, TEXT("Error Message"),  MB_OK | MB_ICONINFORMATION);
    }

  #ifndef NO_VOICE_ENGINE
    
    _veBase->Terminate();
    if(_veBase->Release()!=0)        
    {
        // ensure that no interface is still referenced
        char errorMsg[256];
        sprintf(errorMsg,"All VoiceEngine interfaces are not released properly!");
        ::MessageBox (NULL, (LPCTSTR)errorMsg, TEXT("Error Message"),  MB_OK | MB_ICONINFORMATION);
    }

    if (false == VoiceEngine::Delete(_voiceEngine))
    {
        char errorMsg[256];
        sprintf(errorMsg,"VoiceEngine::Delete() failed!");
        ::MessageBox (NULL, (LPCTSTR)errorMsg, TEXT("Error Message"),  MB_OK | MB_ICONINFORMATION);
    }
   #endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
