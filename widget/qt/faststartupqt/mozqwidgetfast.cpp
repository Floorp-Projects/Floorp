/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include <QtCore/QUrl>
#include "mozqwidgetfast.h"
#include "nsFastStartupQt.h"
#include "nsILocalFile.h"
#include "BinaryPath.h"

#define TOOLBAR_SPLASH "toolbar_splash.png"
#define FAVICON_SPLASH "favicon32.png"
#define DRAWABLE_PATH "res/drawable/"

MozQWidgetFast::MozQWidgetFast(nsWindow* aReceiver, QGraphicsItem* aParent)
{
  setParentItem(aParent);
  char exePath[MAXPATHLEN];
  QStringList arguments = qApp->arguments();
  nsresult rv =
    mozilla::BinaryPath::Get(arguments.at(0).toLocal8Bit().constData(),
                             exePath);
  if (NS_FAILED(rv)) {
    printf("Cannot read default path\n");
    return;
  }
  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash ||
      (lastSlash - exePath > int(MAXPATHLEN - sizeof(XPCOM_DLL) - 1))) {
     return;
  }
  strcpy(++lastSlash, "/");
  QString resourcePath(QString((const char*)&exePath) + DRAWABLE_PATH);
  mToolbar.load(resourcePath + TOOLBAR_SPLASH);
  mIcon.load(resourcePath + FAVICON_SPLASH);
  for (int i = 1; i < arguments.size(); i++) {
    QUrl url = QUrl::fromUserInput(arguments.at(i));
    if (url.isValid()) {
      mUrl = url.toString();
    }
  }
}

void MozQWidgetFast::paint(QPainter* aPainter,
                           const QStyleOptionGraphicsItem*,
                           QWidget*)
{
  // toolbar height
  int toolbarHeight = 80;
  // Offset of favicon starting from left toolbar edge
  int faviconOffset = 25;
  // favicon size
  int faviconSize = 32;
  // width of left and right TOOLBAR_SPLASH part
  // |------------------------------|
  // |LeftPart|tile...part|RightPart|
  float toolbarPartWidth = 77;
  // width of TOOLBAR_SPLASH part after toolbarPartWidth,
  // that can be used for tiled toolbar area
  int tileWidth = 2;
  // Paint left toolbar part
  aPainter->drawPixmap(QRect(0, 0, toolbarPartWidth, toolbarHeight),
                       mToolbar, QRect(0, 0, toolbarPartWidth, toolbarHeight));

  // Paint Tile pixmap of middle toolbar part
  QPixmap tile(tileWidth, toolbarHeight);
  QPainter p(&tile);
  p.drawPixmap(QRect(0, 0, tileWidth, toolbarHeight), mToolbar,
               QRect(toolbarPartWidth, 0, tileWidth, toolbarHeight));
  aPainter->drawTiledPixmap(QRect(toolbarPartWidth, 0, rect().width() - toolbarPartWidth * 2,
                                  toolbarHeight),
                            tile);
  // Paint Favicon
  aPainter->drawPixmap(QRect(faviconOffset, faviconOffset,
                             faviconSize, faviconSize),
                       mIcon);
  if (!mUrl.isEmpty()) {
    // Height or URL string (font height)
    float urlHeight = 24.0f;
    // Start point of URL string, relative to window 0,0
    int urlOffsetX = 80;
    int urlOffsetY = 48;
    QFont font = aPainter->font();
    font.setPixelSize(urlHeight);
    font.setFamily(QString("Nokia Sans"));
    font.setKerning(true);
    aPainter->setFont(font);
    aPainter->setRenderHint(QPainter::TextAntialiasing, true);
    aPainter->drawText(urlOffsetX, urlOffsetY,
                       aPainter->fontMetrics().elidedText(mUrl, Qt::ElideRight, rect().width() - urlOffsetX * 2));
  }

  // Paint Right toolbar part
  aPainter->drawPixmap(QRect(rect().width() - toolbarPartWidth,
                             0, toolbarPartWidth,
                             toolbarHeight),
                       mToolbar,
                       QRect(mToolbar.width() - toolbarPartWidth, 0,
                             toolbarPartWidth, toolbarHeight));

  nsFastStartup::GetSingleton()->painted();
}
