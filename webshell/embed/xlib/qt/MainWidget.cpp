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


#include <qapplication.h>
#include "MainWidget.h"
#include "nslog.h"

NS_IMPL_LOG(MainWidgetLog, 0)
#define PRINTF NS_LOG_PRINTF(MainWidgetLog)
#define FLUSH  NS_LOG_FLUSH(MainWidgetLog)

MainWidget::MainWidget()
{
	setMinimumSize( 500, 500 );
	m_load    = new QPushButton( "Load", this, "Load" );
	m_quit    = new QPushButton( "Quit", this, "Quit" );
	m_mozilla = new QMozillaContainer( this );
	m_edit    = new QLineEdit( this );
	m_progressBar = NULL;

	PRINTF("done creating MainWidget parts...\n");

	m_load->setGeometry( 10, 10, 50, 25 );
	m_quit->setGeometry( 70, 10, 50, 25 );
	m_mozilla->setGeometry( 0, 40, 500, 460 );
	m_edit-> setGeometry( 130, 10, width() - 140, 25 );
	PRINTF("done setting MainWidget parts geometry...\n");

	m_mozilla->setFocusPolicy( QWidget::StrongFocus );
	m_mozilla->setMouseTracking( TRUE );

	QObject::connect( m_load,    SIGNAL(clicked()), 
										this,      SLOT(handleLoad()));

	QObject::connect( m_edit,    SIGNAL(returnPressed()), 
										this,      SLOT(handleReturnPressed()));

	QObject::connect( m_mozilla, SIGNAL(urlLoadStarted()),
										this,      SLOT(handleURLLoadStarted()));

	QObject::connect( m_mozilla, SIGNAL(urlLoadProgressed( const char *, int, int )),
										this,      SLOT(handleURLLoadProgressed( const char*, int, int )));

	QObject::connect( m_mozilla, SIGNAL(urlLoadEnded()),
										this,      SLOT(handleURLLoadEnded()));
}

MainWidget::~MainWidget()
{
}

/* private slot */
void MainWidget::handleLoad()
{
	m_mozilla->loadURL( m_edit->text() );
}

/* private slot */
void MainWidget::handleReturnPressed()
{
	m_mozilla->loadURL( m_edit->text() );
}

/* private slot */
void MainWidget::handleURLLoadStarted()
{
	PRINTF("URL Load Started...\n");
	QApplication::setOverrideCursor( waitCursor );
	if ( !m_progressBar )
	{
		PRINTF("Creating Progress Bar...\n");
		m_progressBar = new QProgressBar( 100, m_mozilla );
	}
	else
		PRINTF("Using available Progress Bar...\n");

	m_progressBar->setProgress( 0 );
	m_progressBar->move( ( m_mozilla->width() - m_progressBar->width() ) / 2,
											 ( m_mozilla->height() - m_progressBar->height() ) / 2
										 );
	m_progressBar->show();
}

/* private slot */
void MainWidget::handleURLLoadProgressed( const char *url, int progress, int max )
{
	PRINTF("URL Load Progressed...\n");
	m_progressBar->setProgress( progress / max * 100 );
}

/* private slot */
void MainWidget::handleURLLoadEnded()
{
	PRINTF("URL Load Ended...\n");
	m_progressBar->hide();
	QApplication::restoreOverrideCursor();
}

QWidget* MainWidget::getQuitWidget()
{
	return m_quit;
}

void MainWidget::resizeEvent( QResizeEvent * )
{
	m_mozilla->setGeometry( 0, 40, width(), height() - 40 );
	m_edit->setGeometry( 130, 10, width() - 130, 25 );
}

