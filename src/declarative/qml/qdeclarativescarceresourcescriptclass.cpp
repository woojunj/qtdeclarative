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

#include "private/qdeclarativescarceresourcescriptclass_p.h"

#include "private/qdeclarativeengine_p.h"
#include "private/qdeclarativecontext_p.h"
#include "private/qdeclarativedata_p.h"
#include "private/qdeclarativetypenamescriptclass_p.h"
#include "private/qdeclarativelistscriptclass_p.h"
#include "private/qdeclarativebinding_p.h"
#include "private/qdeclarativeguard_p.h"
#include "private/qdeclarativevmemetaobject_p.h"

#include <QtCore/qtimer.h>
#include <QtCore/qvarlengtharray.h>
#include <QtScript/qscriptcontextinfo.h>

Q_DECLARE_METATYPE(QScriptValue);

QT_BEGIN_NAMESPACE

QDeclarativeScarceResourceScriptClass::QDeclarativeScarceResourceScriptClass(QDeclarativeEngine *bindEngine)
    : QScriptDeclarativeClass(QDeclarativeEnginePrivate::getScriptEngine(bindEngine)), engine(bindEngine)
{
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);

    // Properties of this type can be explicitly preserved by clients,
    // which prevents the scarce resource from being automatically
    // released after the binding has been evaluated.
    m_preserve = scriptEngine->newFunction(preserve);
    m_preserveId = createPersistentIdentifier(QLatin1String("preserve"));

    // Similarly, they can be explicitly destroyed by clients,
    // which releases the scarce resource.
    m_destroy = scriptEngine->newFunction(destroy);
    m_destroyId = createPersistentIdentifier(QLatin1String("destroy"));
}

QDeclarativeScarceResourceScriptClass::~QDeclarativeScarceResourceScriptClass()
{
}

/*
  Returns a JavaScript object whose instance data is a new scarce resource data.
  The scarce resource is added to the doubly-linked-list of scarce resources in the engine
  so that the scarce resource can be released after evaluation completes.
 */
QScriptValue QDeclarativeScarceResourceScriptClass::newScarceResource(const QVariant &v)
{
    // create the scarce resource
    ScarceResourceData *srd = new ScarceResourceData(v);

    // insert into the linked list
    QDeclarativeEnginePrivate *enginePrivate = QDeclarativeEnginePrivate::get(engine);
    srd->insertInto(&enginePrivate->scarceResources);
    Q_ASSERT(enginePrivate->scarceResourcesRefCount > 0);

    // return the javascript object with the scarce resource instance data
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);
    return QScriptDeclarativeClass::newObject(scriptEngine, this, srd); // JSC takes ownership of srd.
}

QVariant QDeclarativeScarceResourceScriptClass::toVariant(Object *object, bool *ok)
{
    ScarceResourceData *obj = static_cast<ScarceResourceData*>(object);
    if (ok) *ok = true;
    return obj->resource;
}

QVariant QDeclarativeScarceResourceScriptClass::toVariant(const QScriptValue &value)
{
    Q_ASSERT(scriptClass(value) == this);

    return toVariant(object(value), 0);
}

// The destroy() and preserve() function properties are readable.
QScriptClass::QueryFlags
QDeclarativeScarceResourceScriptClass::queryProperty(Object *object, const Identifier &name,
                                                     QScriptClass::QueryFlags flags)
{
    Q_UNUSED(object)
    Q_UNUSED(flags)

    if (name == m_destroyId.identifier || name == m_preserveId.identifier)
        return (QScriptClass::HandlesReadAccess);
    return 0;
}

// Return the (function) values which may be evaluated by clients.
QDeclarativeScarceResourceScriptClass::Value
QDeclarativeScarceResourceScriptClass::property(Object *object, const Identifier &name)
{
    Q_UNUSED(object)

    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);

    // functions
    if (name == m_preserveId.identifier)
        return Value(scriptEngine, m_preserve);
    else if (name == m_destroyId.identifier)
        return Value(scriptEngine, m_destroy);

    return Value();
}

/*
   This method is called when the user explicitly calls the "preserve" method of a scarce resource in JavaScript
   within the specified evaluation context \a context of the script engine \a engine.
   Calling this function signifies that the user explicitly wants to preserve the resource rather than let it
   be automatically released once evaluation of the expression is complete.
   This function removes the internal scarce resource from the declarative engine's linked list of scarce resources
   to release after evaluation of the expression completes.  This means that the resource will only be truly
   released when the JavaScript engine's garbage collector is run.

   Example:
   \qml
   function getIcon(model) {
       var icon = model.avatar; // a pixmap property
       icon.preserve();         // explicitly preserves the resource
       return icon;             // a valid variant will be returned
   }
   \endqml
 */
QScriptValue QDeclarativeScarceResourceScriptClass::preserve(QScriptContext *context, QScriptEngine *engine)
{
    QDeclarativeEnginePrivate *p = QDeclarativeEnginePrivate::get(engine);
    QScriptValue that = context->thisObject();

    if (scriptClass(that) != p->scarceResourceClass)
        return engine->undefinedValue();

    // The client wishes to preserve the resource in this SRD.
    ScarceResourceData *data = static_cast<ScarceResourceData *>(p->scarceResourceClass->object(that));
    if (!data)
        return engine->undefinedValue();

    // remove node from list, without releasing the resource.
    data->removeNode();

    return engine->undefinedValue();
}

/*
   This method is called when the user explicitly calls the "destroy" method of a scarce resource in JavaScript
   within the specified evaluation context \a context of the script engine \a engine.
   Calling this function signifies that the user explicitly wants to release the resource.
   This function sets the internal scarce resource variant to the invalid variant, in order to release the original resource,
   and then removes the resource from the declarative engine's linked-list of scarce resources to
   to release after evaluation of the expression completes, as it has already been released.

   Example:
   \qml
   function getIcon(model) {
       var icon = model.avatar; // a pixmap property
       icon.destroy();          // explicitly releases the resource
       return icon;             // an invalid variant will be returned
   }
   \endqml
 */
QScriptValue QDeclarativeScarceResourceScriptClass::destroy(QScriptContext *context, QScriptEngine *engine)
{
    QDeclarativeEnginePrivate *p = QDeclarativeEnginePrivate::get(engine);
    QScriptValue that = context->thisObject();

    if (scriptClass(that) != p->scarceResourceClass)
        return engine->undefinedValue();

    // the client wishes to release the resource in this SRD.
    ScarceResourceData *data = static_cast<ScarceResourceData *>(p->scarceResourceClass->object(that));
    if (!data)
        return engine->undefinedValue();

    // release the resource and remove the node from the list.
    data->releaseResource();

    return engine->undefinedValue();
}

QT_END_NAMESPACE
