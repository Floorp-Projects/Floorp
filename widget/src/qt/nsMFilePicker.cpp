/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Oleg Romashin <romaxa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include <QWidget>
#include "nsMFilePicker.h"


// Use Qt Tracker API to get attachment data from tracker:
#include <QtSparql/QSparqlConnection>
#include <QtSparql/QSparqlQuery>
#include <QtSparql/QSparqlResult>

#include <MApplication>
#include <MApplicationPage>
#include <MApplicationWindow>
#include <MSceneManager>

#include <QFileInfo>
#include <QPointer>

#include <SelectSingleContentItemPage.h>
#include <SelectMultipleContentItemsPage.h>

MeegoFileDialog::MeegoFileDialog(QObject* aParent)
  : QObject(aParent)
  , mMode(Mode_Unknown)
{
}

MeegoFileDialog::~MeegoFileDialog()
{
}

int
MeegoFileDialog::exec()
{
    MApplicationPage* page = NULL;
    switch(mMode) {
    case Mode_OpenFile:
        page = createOpenFilePage();
        break;
    case Mode_OpenFiles:
        page = createOpenFilesPage();
        break;
    case Mode_OpenDirectory:
        page = createOpenDirectoryPage();
        break;
    default:
        return 0;
    }

    page->setTitle(mCaption);

    QPointer<MApplicationWindow> appWindow = new MApplicationWindow(MApplication::activeWindow());
    QObject::connect(MApplication::activeWindow(), SIGNAL(switcherEntered()), this, SLOT(backButtonClicked()));
    appWindow->show();

    // a not so pretty Hack to build up a back button
    // by using a empty scene window for a pseudo history stack
    MSceneWindow* fakeWindow = new MSceneWindow();
    // tell fake window to disappear when page back was clicked.
    connect(page, SIGNAL(backButtonClicked()), fakeWindow, SLOT(disappear()));
    // let new page appear on pseudo Application Window
    appWindow->sceneManager()->appearSceneWindowNow(page);

    // get, manipulate and set history stack
    QList<MSceneWindow*> sceneWindowHistory = appWindow->sceneManager()->pageHistory();
    sceneWindowHistory.insert(0, fakeWindow);
    appWindow->sceneManager()->setPageHistory(sceneWindowHistory);

    // start own eventloop
    QEventLoop eventLoop;
    mEventLoop = &eventLoop;
    QPointer<MeegoFileDialog> guard = this;
    (void) eventLoop.exec(QEventLoop::DialogExec);
    if (guard.isNull()) {
        return 0;
    }
    if (page) {
        page->disappear();
        delete page;
    }
    if (appWindow) {
        delete appWindow;
    }
    mEventLoop = 0;
    return 0;
}

MApplicationPage*
MeegoFileDialog::createOpenFilePage()
{
    QStringList itemType("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#FileDataObject");

    SelectSingleContentItemPage* page =
        new SelectSingleContentItemPage(QString(),
                                        itemType);

    connect(page, SIGNAL(contentItemSelected(const QString&)),
            this, SLOT  (contentItemSelected(const QString&)));

    connect(page, SIGNAL(backButtonClicked()),
            this, SLOT  (backButtonClicked()));

    return page;
}

MApplicationPage*
MeegoFileDialog::createOpenFilesPage()
{
    QStringList itemType("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Folder");

    SelectMultipleContentItemsPage* page = new SelectMultipleContentItemsPage(QString(), itemType);
    connect(page, SIGNAL(contentItemsSelected(const QStringList&)),
            this, SLOT  (contentItemsSelected(const QStringList&)));

    connect(page, SIGNAL(backButtonClicked()),
            this, SLOT  (backButtonClicked()));

    return page;
}

MApplicationPage*
MeegoFileDialog::createOpenDirectoryPage()
{
    QStringList itemType("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#Folder");

    SelectSingleContentItemPage* page = new SelectSingleContentItemPage(QString(), itemType);
    connect(page, SIGNAL(contentItemSelected(const QString&)),
            this, SLOT  (contentItemSelected(const QString&)));

    connect(page, SIGNAL(backButtonClicked()),
            this, SLOT  (backButtonClicked()));

    return page;
}

QStringList
MeegoFileDialog::selectedFileNames() const
{
    return mSelectedFileNames;
}

bool
MeegoFileDialog::hasSelectedFileNames() const
{
    return !mSelectedFileNames.isEmpty();
}

void
MeegoFileDialog::contentItemSelected(const QString& aContentItem)
{
    mSelectedFileNames.clear();

    if (aContentItem.isEmpty()) {
        finishEventLoop();
        return;
    }

    QUrl fileUrl = trackerIdToFilename(aContentItem);
    QFileInfo fileInfo(fileUrl.toLocalFile());
    if (fileInfo.isFile()) {
        mSelectedFileNames << fileInfo.canonicalFilePath();
    }

    finishEventLoop();
}

void
MeegoFileDialog::contentItemsSelected(const QStringList& aContentItems)
{
    mSelectedFileNames.clear();

    foreach (QString contentItem, aContentItems) {
        QUrl fileUrl = trackerIdToFilename(contentItem);
        QFileInfo fileInfo(fileUrl.toLocalFile());
        if (fileInfo.isFile()) {
            mSelectedFileNames << fileInfo.canonicalFilePath();
        }
    }

    finishEventLoop();
}

void
MeegoFileDialog::backButtonClicked()
{
    mSelectedFileNames.clear();
    finishEventLoop();
}

/* static */ QString
MeegoFileDialog::getOpenFileName(QWidget* parent,
                                 const QString& caption,
                                 const QString& dir,
                                 const QString& filter)
{
    QPointer<MeegoFileDialog> picker = new MeegoFileDialog(parent);

    QString result;
    picker->mMode = MeegoFileDialog::Mode_OpenFile;
    picker->mCaption = caption;
    picker->exec();

    // Need separate 'if' because picker might have been destroyed by its parent during exec()
    if (picker) {
        if (picker->hasSelectedFileNames()) {
            result = picker->selectedFileNames().first();
        }
        delete picker;
    }

    return result;
}

/* static */ QString
MeegoFileDialog::getExistingDirectory(QWidget* parent,const QString& caption, const QString& dir)
{
    QPointer<MeegoFileDialog> picker = new MeegoFileDialog(parent);

    QString result;
    picker->mMode = MeegoFileDialog::Mode_OpenDirectory;
    picker->mCaption = caption;
    picker->exec();

    // Need separate 'if' because picker might have been destroyed by its parent during exec()
    if (picker) {
        if (picker->hasSelectedFileNames()) {
            result = picker->selectedFileNames().first();
        }
        delete picker;
    }

    return result;
}

/* static */ QStringList
MeegoFileDialog::getOpenFileNames(QWidget* parent,
                                  const QString& caption,
                                  const QString& dir,
                                  const QString& filter)
{
    QPointer<MeegoFileDialog> picker = new MeegoFileDialog(parent);

    QStringList result;
    picker->mMode = MeegoFileDialog::Mode_OpenFiles;
    picker->mCaption = caption;
    picker->exec();

    // Need separate 'if' because picker might have been destroyed by its parent during exec()
    if (picker) {
        if (picker->hasSelectedFileNames()) {
            result = picker->selectedFileNames();
        }
        delete picker;
    }

    return result;
}

QUrl
MeegoFileDialog::trackerIdToFilename(const QString& trackerId)
{
    QSparqlQuery query("SELECT ?u WHERE { ?:tUri nie:url ?u . }");
    query.bindValue("tUri", QUrl(trackerId)); // That puts <trackerId> into the query
    QSparqlConnection connection("QTRACKER"); // Exact string needed to make connection to tracker
    QSparqlResult* result = connection.exec(query);

    result->waitForFinished();
    result->first();

    QUrl resultFile;
    if (result->isValid()) {
        resultFile = QUrl(result->binding(0).value().toString());
    }
    delete result;

    return resultFile;
}
