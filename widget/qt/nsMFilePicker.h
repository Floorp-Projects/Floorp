/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSMFILEDIALOG_H
#define NSMFILEDIALOG_H

#include <QEventLoop>
#include <QStringList>
#include <QUrl>

class MApplicationPage;
class MeegoFileDialog : public QObject
{
    Q_OBJECT
public:

    enum MeegoFileDialogMode {
        Mode_Unknown = -1,
        Mode_OpenFile = 0,
        Mode_OpenFiles = 1,
        Mode_OpenDirectory = 2
        // No Mode_SaveFile - the content picker can't handle that
    };

    MeegoFileDialog(QObject* aParent = 0);
    virtual ~MeegoFileDialog();

    QStringList selectedFileNames() const;
    bool hasSelectedFileNames() const;

    static QString getOpenFileName(QWidget* parent, const QString &caption, const QString& dir, const QString& filter);
    static QStringList getOpenFileNames(QWidget* parent, const QString& caption, const QString& dir, const QString& filter);
    static QString getExistingDirectory(QWidget* parent, const QString& caption, const QString& dir);

    int exec();

private Q_SLOTS:
    void contentItemSelected(const QString& aItem);
    void contentItemsSelected(const QStringList& aItems);
    void backButtonClicked();

private:
    QUrl trackerIdToFilename(const QString& trackerId);
    MApplicationPage* createOpenFilePage();
    MApplicationPage* createOpenFilesPage();
    MApplicationPage* createOpenDirectoryPage();
    void finishEventLoop() {
        if (!mEventLoop)
            return;
        mEventLoop->exit(0);
        mEventLoop = 0;
    }

    QEventLoop* mEventLoop;
    MeegoFileDialogMode mMode;
    QString mCaption;
    QStringList mSelectedFileNames;
};

#endif // NSMFILEDIALOG_H
