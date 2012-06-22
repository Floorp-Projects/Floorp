/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_WINDOWSTEST_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_WINDOWSTEST_H_


#include "StdAfx.h"
#include "resource.h"		// main symbols



/////////////////////////////////////////////////////////////////////////////

//Forward declarations
namespace webrtc {
    class VoiceEngine;
    class VoEBase;
    class VideoEngine;
}
using namespace webrtc;

class CDXWindowsTestApp : public CWinApp
{
public:
	CDXWindowsTestApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDXWindowsTestApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CDXWindowsTestApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	VideoEngine*  _videoEngine;
    VoiceEngine*  _voiceEngine;
    VoEBase*       _veBase;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_WINDOWSTEST_H_
