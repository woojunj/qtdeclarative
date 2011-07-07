// Commit: ac5c099cc3c5b8c7eec7a49fdeb8a21037230350
/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSGBORDERIMAGE_P_P_H
#define QSGBORDERIMAGE_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qsgimagebase_p_p.h"
#include "qsgscalegrid_p_p.h"

QT_BEGIN_NAMESPACE

class QNetworkReply;
class QSGBorderImagePrivate : public QSGImageBasePrivate
{
    Q_DECLARE_PUBLIC(QSGBorderImage)

public:
    QSGBorderImagePrivate()
      : border(0), sciReply(0),
        horizontalTileMode(QSGBorderImage::Stretch),
        verticalTileMode(QSGBorderImage::Stretch),
        redirectCount(0), pixmapChanged(false)
    {
    }

    ~QSGBorderImagePrivate()
    {
    }


    QSGScaleGrid *getScaleGrid()
    {
        Q_Q(QSGBorderImage);
        if (!border) {
            border = new QSGScaleGrid(q);
            static int borderChangedSignalIdx = -1;
            static int doUpdateSlotIdx = -1;
            if (borderChangedSignalIdx < 0)
                borderChangedSignalIdx = QSGScaleGrid::staticMetaObject.indexOfSignal("borderChanged()");
            if (doUpdateSlotIdx < 0)
                doUpdateSlotIdx = QSGBorderImage::staticMetaObject.indexOfSlot("doUpdate()");
            QMetaObject::connect(border, borderChangedSignalIdx, q, doUpdateSlotIdx);
        }
        return border;
    }

    QSGScaleGrid *border;
    QUrl sciurl;
    QNetworkReply *sciReply;
    QSGBorderImage::TileMode horizontalTileMode;
    QSGBorderImage::TileMode verticalTileMode;
    int redirectCount;

    bool pixmapChanged : 1;
};

QT_END_NAMESPACE

#endif // QSGBORDERIMAGE_P_P_H
