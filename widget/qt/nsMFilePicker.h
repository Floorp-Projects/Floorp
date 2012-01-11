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
 *    Stefan Hundhammer <HuHa-zilla@gmx.de>
 *    Jeremias Bosch <jeremias.bosch@gmail.com>
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

private slots:
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
