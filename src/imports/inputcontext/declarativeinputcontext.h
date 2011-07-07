/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
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

#ifndef DECLARATIVEINPUTCONTEXT_H
#define DECLARATIVEINPUTCONTEXT_H

#include <QtGui/qinputcontext.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Declarative)

class InputContextKeyFilter;
class InputContextModule;
class InputContextMouseFilter;
class InputContextMouseHandler;

class DeclarativeInputContext : public QInputContext
{
    Q_OBJECT
public:
    explicit DeclarativeInputContext(QObject *parent = 0);
    ~DeclarativeInputContext();

    bool isComposing() const;

    QString identifierName();
    QString language();

    void setFocusWidget(QWidget *widget);

    void mouseHandler(int x, QMouseEvent *event);

    bool filterMouseEvent(const QMouseEvent *event);
    bool filterKeyEvent(const QKeyEvent *event);

    bool filterEvent(const QEvent *event);

    void reset();
    void update();

    void setModule(InputContextModule *module);

    void registerMouseHandler(InputContextMouseHandler *handler);
    void registerMouseFilter(InputContextMouseFilter *filter);
    void registerKeyFilter(InputContextKeyFilter *filter);

private slots:
    void mouseHandlerDestroyed(QObject *handler);
    void mouseFilterDestroyed(QObject *filter);
    void keyFilterDestroyed(QObject *filter);

private:
    InputContextModule *m_module;
    QList<InputContextMouseHandler *> m_mouseHandlers;
    QList<InputContextMouseFilter *> m_mouseFilters;
    QList<InputContextKeyFilter *> m_keyFilters;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif
