/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Milind Changire <changire@yahoo.com>
 */

#ifndef MainWidget_h__
#define MainWidget_h__

#include <qpushbutton.h>
#include <qlineedit.h>
#include <qprogressbar.h>

#include "QMozillaContainer.h"

class MainWidget : public QWidget
{
	Q_OBJECT
public:
	MainWidget();
	~MainWidget();

	QWidget* getQuitWidget();

	void resizeEvent( QResizeEvent * );

private slots:
	void handleLoad();
	void handleReturnPressed();
	void handleURLLoadStarted();
	void handleURLLoadProgressed( const char* url, int progress, int max );
	void handleURLLoadEnded();
	
private:
	QPushButton				*m_load;
	QPushButton       *m_quit;
	QMozillaContainer	*m_mozilla;
	QLineEdit					*m_edit;
	QProgressBar			*m_progressBar;
};

#endif // MainWidget_h__
