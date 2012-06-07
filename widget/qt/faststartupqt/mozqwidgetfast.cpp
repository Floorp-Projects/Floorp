/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QtCore/QUrl>
#include "mozqwidgetfast.h"
#include "nsFastStartupQt.h"
#include "nsIFile.h"
#include "nsStringGlue.h"
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
