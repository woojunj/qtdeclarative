/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qsgnode.h"
#include "qsgrenderer_p.h"
#include "qsgnodeupdater_p.h"
#include "qsgmaterial.h"

#include "limits.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_DEBUG
static int qt_node_count = 0;

static void qt_print_node_count()
{
    qDebug("Number of leaked nodes: %i", qt_node_count);
    qt_node_count = -1;
}
#endif

/*!
    \class QSGNode
    \brief The QSGNode class is the base class for all nodes in the scene graph.

    The QSGNode class can be used as a child container. Children are added with
    the appendChildNode(), prependChildNode(), insertChildNodeBefore() and
    insertChildNodeAfter(). Ordering of nodes is important as geometry nodes
    will be rendered in the order they are added to the scene graph.
    Actually, the scene may reorder nodes freely, but the resulting visual
    order is still guaranteed.

    If nodes change every frame, the preprocess() function can be used to
    apply changes to a node for every frame its rendered. The use of preprocess()
    must be explicitly enabled by setting the QSGNode::UsePreprocess flag
    on the node.

    The virtual isSubtreeBlocked() function can be used to disable a subtree all
    together. Nodes in a blocked subtree will not be preprocessed() and not
    rendered.

    Anything related to QSGNode should happen on the scene graph rendering thread.
 */

QSGNode::QSGNode()
    : m_parent(0)
    , m_type(BasicNodeType)
    , m_subtreeGeometryCount(0)
    , m_nodeFlags(OwnedByParent)
    , m_flags(0)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_nextSibling(0)
    , m_previousSibling(0)
{
    init();
}

QSGNode::QSGNode(NodeType type)
    : m_parent(0)
    , m_type(type)
    , m_subtreeGeometryCount(type == GeometryNodeType ? 1 : 0)
    , m_nodeFlags(OwnedByParent)
    , m_flags(0)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_nextSibling(0)
    , m_previousSibling(0)
{
    init();
}

void QSGNode::init()
{
#ifndef QT_NO_DEBUG
    ++qt_node_count;
    static bool atexit_registered = false;
    if (!atexit_registered) {
        atexit(qt_print_node_count);
        atexit_registered = true;
    }
#endif
}

QSGNode::~QSGNode()
{
#ifndef QT_NO_DEBUG
    --qt_node_count;
    if (qt_node_count < 0)
        qDebug("Material destroyed after qt_print_node_count() was called.");
#endif
    destroy();
}


/*!
    \fn void QSGNode::preprocess()

    Override this function to do processing on the node before it is rendered.

    Preprocessing needs to be explicitly enabled by setting the flag
    QSGNode::UsePreprocess. The flag needs to be set before the node is added
    to the scene graph and will cause the preprocess() function to be called
    for every frame the node is rendered.

    The preprocess function is called before the update pass that propegates
    opacity and transformations through the scene graph. That means that
    functions like QSGOpacityNode::combinedOpacity() and
    QSGTransformNode::combinedMatrix() will not contain up-to-date values.
    If such values are changed during the preprocess, these changes will be
    propegated through the scene graph before it is rendered.

    \warning Beware of deleting nodes while they are being preprocessed. It is
    possible, with a small performance hit, to delete a single node during its
    own preprocess call. Deleting a subtree which has nodes that also use
    preprocessing may result in a segmentation fault. This is done for
    performance reasons.
 */




/*!
    Returns whether this node and its subtree is available for use.

    Blocked subtrees will not get their dirty states updated and they
    will not be rendered.

    The QSGOpacityNode will return a blocked subtree when accumulated opacity
    is 0, for instance.
 */

bool QSGNode::isSubtreeBlocked() const
{
    return m_subtreeGeometryCount == 0;
}

/*!
    \internal
    Detaches the node from the scene graph and deletes any children it owns.

    This function is called from QSGNode's and QSGRootNode's destructor. It
    should not be called explicitly in user code. QSGRootNode needs to call
    destroy() because destroy() calls removeChildNode() which in turn calls
    markDirty() which type-casts the node to QSGRootNode. This type-cast is not
    valid at the time QSGNode's destructor is called because the node will
    already be partially destroyed at that point.
*/

void QSGNode::destroy()
{
    if (m_parent) {
        m_parent->removeChildNode(this);
        Q_ASSERT(m_parent == 0);
    }
    while (m_firstChild) {
        QSGNode *child = m_firstChild;
        removeChildNode(child);
        Q_ASSERT(child->m_parent == 0);
        if (child->flags() & OwnedByParent)
            delete child;
    }

    Q_ASSERT(m_firstChild == 0 && m_lastChild == 0);
}


/*!
    Prepends \a node to this node's the list of children.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::prependChildNode(QSGNode *node)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::prependChildNode", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::prependChildNode", "QSGNode already has a parent");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::prependChildNode", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::prependChildNode", "QSGGeometryNode is missing geometry");
    }
#endif

    if (m_firstChild)
        m_firstChild->m_previousSibling = node;
    else
        m_lastChild = node;
    node->m_nextSibling = m_firstChild;
    m_firstChild = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}

/*!
    Appends \a node to this node's list of children.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::appendChildNode(QSGNode *node)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::appendChildNode", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::appendChildNode", "QSGNode already has a parent");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::appendChildNode", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::appendChildNode", "QSGGeometryNode is missing geometry");
    }
#endif

    if (m_lastChild)
        m_lastChild->m_nextSibling = node;
    else
        m_firstChild = node;
    node->m_previousSibling = m_lastChild;
    m_lastChild = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Inserts \a node to this node's list of children before the node specified with \a before.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::insertChildNodeBefore(QSGNode *node, QSGNode *before)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::insertChildNodeBefore", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::insertChildNodeBefore", "QSGNode already has a parent");
    Q_ASSERT_X(before && before->m_parent == this, "QSGNode::insertChildNodeBefore", "The parent of \'before\' is wrong");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::insertChildNodeBefore", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::insertChildNodeBefore", "QSGGeometryNode is missing geometry");
    }
#endif

    QSGNode *previous = before->m_previousSibling;
    if (previous)
        previous->m_nextSibling = node;
    else
        m_firstChild = node;
    node->m_previousSibling = previous;
    node->m_nextSibling = before;
    before->m_previousSibling = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Inserts \a node to this node's list of children after the node specified with \a after.

    Ordering of nodes is important as geometry nodes will be rendered in the
    order they are added to the scene graph.
 */

void QSGNode::insertChildNodeAfter(QSGNode *node, QSGNode *after)
{
    //Q_ASSERT_X(!m_children.contains(node), "QSGNode::insertChildNodeAfter", "QSGNode is already a child!");
    Q_ASSERT_X(!node->m_parent, "QSGNode::insertChildNodeAfter", "QSGNode already has a parent");
    Q_ASSERT_X(after && after->m_parent == this, "QSGNode::insertChildNodeBefore", "The parent of \'before\' is wrong");

#ifndef QT_NO_DEBUG
    if (node->type() == QSGNode::GeometryNodeType) {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(node);
        Q_ASSERT_X(g->material(), "QSGNode::insertChildNodeAfter", "QSGGeometryNode is missing material");
        Q_ASSERT_X(g->geometry(), "QSGNode::insertChildNodeAfter", "QSGGeometryNode is missing geometry");
    }
#endif

    QSGNode *next = after->m_nextSibling;
    if (next)
        next->m_previousSibling = node;
    else
        m_lastChild = node;
    node->m_nextSibling = next;
    node->m_previousSibling = after;
    after->m_nextSibling = node;
    node->m_parent = this;

    node->markDirty(DirtyNodeAdded);
}



/*!
    Removes \a node from this node's list of children.
 */

void QSGNode::removeChildNode(QSGNode *node)
{
    //Q_ASSERT(m_children.contains(node));
    Q_ASSERT(node->parent() == this);

    QSGNode *previous = node->m_previousSibling;
    QSGNode *next = node->m_nextSibling;
    if (previous)
        previous->m_nextSibling = next;
    else
        m_firstChild = next;
    if (next)
        next->m_previousSibling = previous;
    else
        m_lastChild = previous;
    node->m_previousSibling = 0;
    node->m_nextSibling = 0;

    node->markDirty(DirtyNodeRemoved);
    node->m_parent = 0;
}


/*!
    Removes all child nodes from this node's list of children.
 */

void QSGNode::removeAllChildNodes()
{
    while (m_firstChild) {
        QSGNode *node = m_firstChild;
        m_firstChild = node->m_nextSibling;
        node->m_nextSibling = 0;
        if (m_firstChild)
            m_firstChild->m_previousSibling = 0;
        else
            m_lastChild = 0;
        node->markDirty(DirtyNodeRemoved);
        node->m_parent = 0;
    }
}


int QSGNode::childCount() const
{
    int count = 0;
    QSGNode *n = m_firstChild;
    while (n) {
        ++count;
        n = n->m_nextSibling;
    }
    return count;
}


QSGNode *QSGNode::childAtIndex(int i) const
{
    QSGNode *n = m_firstChild;
    while (i && n) {
        --i;
        n = n->m_nextSibling;
    }
    return n;
}


/*!
    Sets the flag \a f on this node if \a enabled is true;
    otherwise clears the flag.

    \sa flags()
*/

void QSGNode::setFlag(Flag f, bool enabled)
{
    if (enabled)
        m_nodeFlags |= f;
    else
        m_nodeFlags &= ~f;
}


/*!
    Sets the flags \a f on this node if \a enabled is true;
    otherwise clears the flags.

    \sa flags()
*/

void QSGNode::setFlags(Flags f, bool enabled)
{
    if (enabled)
        m_nodeFlags |= f;
    else
        m_nodeFlags &= ~f;
}



/*!
    Marks this node with the states in \a flags as dirty.

    When a node is marked dirty, it recursively mark the parent chain
    as dirty and notify all connected renderers that the has dirty states.
 */

void QSGNode::markDirty(DirtyFlags flags)
{
    m_flags |= (flags & DirtyPropagationMask);

    DirtyFlags subtreeFlags = DirtyFlags((flags & DirtyPropagationMask) << 16);

    int geometryCountDiff = 0;
    if (flags & DirtyNodeAdded)
        geometryCountDiff += m_subtreeGeometryCount;
    if (flags & DirtyNodeRemoved)
        geometryCountDiff -= m_subtreeGeometryCount;

    QSGNode *p = m_parent;
    while (p) {
        p->m_flags |= subtreeFlags;
        p->m_subtreeGeometryCount += geometryCountDiff;
        if (p->type() == RootNodeType)
            static_cast<QSGRootNode *>(p)->notifyNodeChange(this, flags);
        p = p->m_parent;
    }
}



/*!
    \class QSGBasicGeometryNode
    \brief The QSGBasicGeometryNode serves as a baseclass for geometry based nodes

    The QSGBasicGeometryNode class should not be used by itself. It is only encapsulates
    shared functionality between the QSGGeometryNode and QSGClipNode classes.
  */


/*!
    Creates a new basic geometry node.
 */
QSGBasicGeometryNode::QSGBasicGeometryNode(NodeType type)
    : QSGNode(type)
    , m_geometry(0)
    , m_matrix(0)
    , m_clip_list(0)
{
}


/*!
    Deletes this QSGBasicGeometryNode.

    If the node has the flag QSGNode::OwnsGeometry set, it will also delete the
    geometry object it is pointing to. This flag is not set by default.
  */

QSGBasicGeometryNode::~QSGBasicGeometryNode()
{
    if (flags() & OwnsGeometry)
        delete m_geometry;
}


/*!
    \fn QSGGeometry *QSGBasicGeometryNode::geometry() const

    Returns this node's geometry.

    The geometry is null by default.
 */


/*!
    Sets the geometry of this node to \a geometry.

    If the node has the flag QSGNode::OwnsGeometry set, it will also delete the
    geometry object it is pointing to. This flag is not set by default.
 */

void QSGBasicGeometryNode::setGeometry(QSGGeometry *geometry)
{
    if (flags() & OwnsGeometry)
        delete m_geometry;
    m_geometry = geometry;
    markDirty(DirtyGeometry);
}



/*!
    \class QSGGeometryNode
    \brief The QSGGeometryNode class is used for all rendered content in the scene graph.

    The QSGGeometryNode consists of geometry and material. The geometry defines the mesh,
    the vertices and their structure, to be drawn. The Material defines how the shape is
    filled.

    A geometry node must have both geometry and a normal material before it is added to
    the scene graph.

    The geometry node supports two types of materials, the opaqueMaterial and the normal
    material. The opaqueMaterial is used when the accumulated scene graph opacity at the
    time of rendering is 1. The primary usecase is to special case opaque rendering
    to avoid an extra operation in the fragment shader can have significant performance
    impact on embedded graphics chips. The opaque material is optional.

 */


/*!
    Creates a new geometry node without geometry and material.
 */

QSGGeometryNode::QSGGeometryNode()
    : QSGBasicGeometryNode(GeometryNodeType)
    , m_render_order(0)
    , m_material(0)
    , m_opaque_material(0)
    , m_opacity(1)
{
}


/*!
    Deletes this geometry node.

    The flags QSGNode::OwnsMaterial, QSGNode::OwnsOpaqueMaterial and
    QSGNode::OwnsGeometry decides weither the geometry node should also
    delete the materials and geometry. By default, these flags are disabled.
 */

QSGGeometryNode::~QSGGeometryNode()
{
    if (flags() & OwnsMaterial)
        delete m_material;
    if (flags() & OwnsOpaqueMaterial)
        delete m_opaque_material;
}



/*!
    \fn int QSGGeometryNode::renderOrder() const

    Returns the render order of this geometry node.

    \internal
 */


/*!
    Sets the render order of this node to be \a order.

    GeometryNodes are rendered in an order that visually looks like
    low order nodes are rendered prior to high order nodes. For opaque
    geometry there is little difference as z-testing will handle
    the discard, but for translucent objects, the rendering should
    normally be specified in the order of back-to-front.

    The default render order is 0.

    \internal
  */
void QSGGeometryNode::setRenderOrder(int order)
{
    m_render_order = order;
}



/*!
    Sets the material of this geometry node to \a material.

    Geometry nodes must have a material before they can be added to the
    scene graph.
 */
void QSGGeometryNode::setMaterial(QSGMaterial *material)
{
    if (flags() & OwnsMaterial)
        delete m_material;
    m_material = material;
#ifndef QT_NO_DEBUG
    if (m_material != 0 && m_opaque_material == m_material)
        qWarning("QSGGeometryNode: using same material for both opaque and translucent");
#endif
    markDirty(DirtyMaterial);
}



/*!
    Sets the opaque material of this geometry to \a material.

    The opaque material will be preferred by the renderer over the
    default material, as returned by the material() function, if
    it is not null and the geometry item has an inherited opacity of
    1.

    The opaqueness refers to scene graph opacity, the material is still
    allowed to set QSGMaterial::Blending to true and draw transparent
    pixels.
 */
void QSGGeometryNode::setOpaqueMaterial(QSGMaterial *material)
{
    if (flags() & OwnsOpaqueMaterial)
        delete m_opaque_material;
    m_opaque_material = material;
#ifndef QT_NO_DEBUG
    if (m_opaque_material != 0 && m_opaque_material == m_material)
        qWarning("QSGGeometryNode: using same material for both opaque and translucent");
#endif

    markDirty(DirtyMaterial);
}



/*!
    Returns the material which should currently be used for geometry node.

    If the inherited opacity of the node is 1 and there is an opaque material
    set on this node, it will be returned; otherwise, the default material
    will be returned.

    \warning This function requires the scene graph above this item to be
    completely free of dirty states, so it can only be called during rendering

    \internal

    \sa setMaterial, setOpaqueMaterial
 */
QSGMaterial *QSGGeometryNode::activeMaterial() const
{
    Q_ASSERT_X(dirtyFlags() == 0, "QSGGeometryNode::activeMaterial()", "function assumes that all dirty states are cleaned up");
    if (m_opaque_material && m_opacity > 0.999)
        return m_opaque_material;
    return m_material;
}


/*!
    Sets the inherited opacity of this geometry to \a opacity.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
  */
void QSGGeometryNode::setInheritedOpacity(qreal opacity)
{
    Q_ASSERT(opacity >= 0 && opacity <= 1);
    m_opacity = opacity;
}


/*!
    \class QSGClipNode
    \brief The QSGClipNode implements the clipping functionality in the scene graph.

    Clipping applies to the node's subtree and can be nested. Multiple clip nodes will be
    accumulated by intersecting all their geometries. The accumulation happens
    as part of the rendering.

    Clip nodes must have a geometry before they can be added to the scene graph.

    Clipping is usually implemented by using the stencil buffer.
 */



/*!
    Creates a new QSGClipNode without a geometry.

    The clip node must have a geometry before it can be added to the
    scene graph.
 */

QSGClipNode::QSGClipNode()
    : QSGBasicGeometryNode(ClipNodeType)
{
}



/*!
    Deletes this QSGClipNode.

    If the flag QSGNode::OwnsGeometry is set, the geometry will also be
    deleted.
 */

QSGClipNode::~QSGClipNode()
{
}



/*!
    \fn bool QSGClipNode::isRectangular() const

    Returns if this clip node has a rectangular clip.
 */



/*!
    Sets whether this clip node has a rectangular clip to \a rectHint.

    This is an optimization hint which means that the renderer can
    use scissoring instead of stencil, which is significnatly faster.

    When this hint is and it is applicable, the clip region will be
    generated from clipRect() rather than geometry().
 */

void QSGClipNode::setIsRectangular(bool rectHint)
{
    m_is_rectangular = rectHint;
}



/*!
    \fn void QSGClipNode::clipRect() const

    Returns the clip rect of this node.
 */


/*!
    Sets the clip rect of this clip node to \a rect.

    When a rectangular clip is set in combination with setIsRectangular
    the renderer may in some cases use a more optimal clip method.
 */
void QSGClipNode::setClipRect(const QRectF &rect)
{
    m_clip_rect = rect;
}


/*!
    \class QSGTransformNode
    \brief The QSGTransformNode implements transformations in the scene graph

    Transformations apply the node's subtree and can be nested. Multiple transform nodes
    will be accumulated by intersecting all their matrices. The accumulation happens
    as part of the rendering.

    The transform nodes implement a 4x4 matrix which in theory supports full 3D
    transformations. However, because the renderer optimizes for 2D use-cases rather
    than 3D use-cases, rendering a scene with full 3D transformations needs to
    be done with some care.
 */

QSGTransformNode::QSGTransformNode()
    : QSGNode(TransformNodeType)
{
}



/*!
    Deletes this transform node.
 */

QSGTransformNode::~QSGTransformNode()
{
}



/*!
    \fn QMatrix4x4 QSGTransformNode::matrix() const

    Returns this transform node's matrix.
 */



/*!
    Sets this transform node's matrix to \a matrix.
 */

void QSGTransformNode::setMatrix(const QMatrix4x4 &matrix)
{
    m_matrix = matrix;
    markDirty(DirtyMatrix);
}


/*!
    Sets the combined matrix of this matrix to \a transform.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
  */
void QSGTransformNode::setCombinedMatrix(const QMatrix4x4 &matrix)
{
    m_combined_matrix = matrix;
}



/*!
    \class QSGRootNode
    \brief The QSGRootNode is the toplevel root of any scene graph.

    The root node is used to attach a scene graph to a renderer.

    \internal
 */



/*!
    \fn QSGRootNode::QSGRootNode()

    Creates a new root node.
 */

QSGRootNode::QSGRootNode()
    : QSGNode(RootNodeType)
{
}


/*!
    Deletes the root node.

    When a root node is deleted it removes itself from all of renderers
    that are referencing it.
 */

QSGRootNode::~QSGRootNode()
{
    while (!m_renderers.isEmpty())
        m_renderers.last()->setRootNode(0);
    destroy(); // Must call destroy() here because markDirty() casts this to QSGRootNode.
}



/*!
    Called to notify all renderers that \a node has been marked as dirty
    with \a flags.
 */

void QSGRootNode::notifyNodeChange(QSGNode *node, DirtyFlags flags)
{
    for (int i=0; i<m_renderers.size(); ++i) {
        m_renderers.at(i)->nodeChanged(node, flags);
    }
}



/*!
    \class QSGOpacityNode
    \brief The QSGOpacityNode is used

    Opacity apply to its subtree and can be nested. Multiple opacity nodes
    will be accumulated by multiplying their opacity. The accumulation happens
    as part of the rendering.

    When nested opacity gets below a certain threshold, the subtree might
    be marked as blocked, causing isSubtreeBlocked() to return true. This
    is done for performance reasons.

 */



/*!
    Constructs an opacity node with a default opacity of 1.

    Opacity accumulate downwards in the scene graph so a node with two
    QSGOpacityNode instances above it, both with opacity of 0.5, will have
    effective opacity of 0.25.

    The default opacity of nodes is 1.
  */
QSGOpacityNode::QSGOpacityNode()
    : QSGNode(OpacityNodeType)
    , m_opacity(1)
    , m_combined_opacity(1)
{
}



/*!
    Deletes the opacity node.
 */

QSGOpacityNode::~QSGOpacityNode()
{
}



/*!
    \fn qreal QSGOpacityNode::opacity() const

    Returns this opacity node's opacity.
 */



/*!
    Sets the opacity of this node to \a opacity.

    Before rendering the graph, the renderer will do an update pass
    over the subtree to propegate the opacity to its children.

    The value will be bounded to the range 0 to 1.
 */

void QSGOpacityNode::setOpacity(qreal opacity)
{
    opacity = qBound<qreal>(0, opacity, 1);
    if (m_opacity == opacity)
        return;
    m_opacity = opacity;
    markDirty(DirtyOpacity);
}



/*!
    \fn qreal QSGOpacityNode::combinedOpacity() const

    Returns this node's accumulated opacity.

    This vaule is calculated during rendering and only stored
    in the opacity node temporarily.

    \internal
 */



/*!
    Sets the combined opacity of this node to \a opacity.

    This function is meant to be called by the node preprocessing
    prior to rendering the tree, so it will not mark the tree as
    dirty.

    \internal
 */

void QSGOpacityNode::setCombinedOpacity(qreal opacity)
{
    m_combined_opacity = opacity;
}



/*!
    For performance reasons, we block the subtree when the opacity
    is below a certain threshold.

    \internal
 */

bool QSGOpacityNode::isSubtreeBlocked() const
{
    return QSGNode::isSubtreeBlocked() || m_opacity < 0.001;
}


/*!
    \class QSGNodeVisitor
    \brief The QSGNodeVisitor class is a helper class for traversing the scene graph.

    \internal
 */

QSGNodeVisitor::~QSGNodeVisitor()
{

}


void QSGNodeVisitor::visitNode(QSGNode *n)
{
    switch (n->type()) {
    case QSGNode::TransformNodeType: {
        QSGTransformNode *t = static_cast<QSGTransformNode *>(n);
        enterTransformNode(t);
        visitChildren(t);
        leaveTransformNode(t);
        break; }
    case QSGNode::GeometryNodeType: {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(n);
        enterGeometryNode(g);
        visitChildren(g);
        leaveGeometryNode(g);
        break; }
    case QSGNode::ClipNodeType: {
        QSGClipNode *c = static_cast<QSGClipNode *>(n);
        enterClipNode(c);
        visitChildren(c);
        leaveClipNode(c);
        break; }
    case QSGNode::OpacityNodeType: {
        QSGOpacityNode *o = static_cast<QSGOpacityNode *>(n);
        enterOpacityNode(o);
        visitChildren(o);
        leaveOpacityNode(o);
        break; }
    default:
        visitChildren(n);
        break;
    }
}

void QSGNodeVisitor::visitChildren(QSGNode *n)
{
    for (QSGNode *c = n->firstChild(); c; c = c->nextSibling())
        visitNode(c);
}



#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const QSGGeometryNode *n)
{
    if (!n) {
        d << "QSGGeometryNode(null)";
        return d;
    }
    d << "QSGGeometryNode(" << hex << (void *) n << dec;

    const QSGGeometry *g = n->geometry();

    if (!g) {
        d << "no geometry";
    } else {

        switch (g->drawingMode()) {
        case GL_TRIANGLE_STRIP: d << "strip"; break;
        case GL_TRIANGLE_FAN: d << "fan"; break;
        case GL_TRIANGLES: d << "triangles"; break;
        default: break;
        }

         d << g->vertexCount();

         if (g->attributeCount() > 0 && g->attributes()->type == GL_FLOAT) {
             float x1 = 1e10, x2 = -1e10, y1=1e10, y2=-1e10;
             int stride = g->stride();
             for (int i = 0; i < g->vertexCount(); ++i) {
                 float x = ((float *)((char *)const_cast<QSGGeometry *>(g)->vertexData() + i * stride))[0];
                 float y = ((float *)((char *)const_cast<QSGGeometry *>(g)->vertexData() + i * stride))[1];

                 x1 = qMin(x1, x);
                 x2 = qMax(x2, x);
                 y1 = qMin(y1, y);
                 y2 = qMax(y2, y);
             }

             d << "x1=" << x1 << "y1=" << y1 << "x2=" << x2 << "y2=" << y2;
         }
    }

    d << "order=" << n->renderOrder();
    if (n->material())
        d << "effect=" << n->material() << "type=" << n->material()->type();


    d << ")";
#ifdef QML_RUNTIME_TESTING
    d << n->description;
#endif
    d << "dirty=" << hex << (int) n->dirtyFlags() << dec;
    return d;
}

QDebug operator<<(QDebug d, const QSGClipNode *n)
{
    if (!n) {
        d << "QSGClipNode(null)";
        return d;
    }
    d << "QSGClipNode(" << hex << (void *) n << dec;

    if (n->childCount())
        d << "children=" << n->childCount();

    d << "is rect?" << (n->isRectangular() ? "yes" : "no");

    d << ")";
#ifdef QML_RUNTIME_TESTING
    d << n->description;
#endif
    d << "dirty=" << hex << (int) n->dirtyFlags() << dec << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
    return d;
}

QDebug operator<<(QDebug d, const QSGTransformNode *n)
{
    if (!n) {
        d << "QSGTransformNode(null)";
        return d;
    }
    const QMatrix4x4 m = n->matrix();
    d << "QSGTransformNode(";
    d << hex << (void *) n << dec;
    if (m.isIdentity())
        d << "identity";
    else if (m.determinant() == 1 && m(0, 0) == 1 && m(1, 1) == 1 && m(2, 2) == 1)
        d << "translate" << m(0, 3) << m(1, 3) << m(2, 3);
    else
        d << "det=" << n->matrix().determinant();
#ifdef QML_RUNTIME_TESTING
    d << n->description;
#endif
    d << "dirty=" << hex << (int) n->dirtyFlags() << dec << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
    d << ")";
    return d;
}

QDebug operator<<(QDebug d, const QSGOpacityNode *n)
{
    if (!n) {
        d << "QSGOpacityNode(null)";
        return d;
    }
    d << "QSGOpacityNode(";
    d << hex << (void *) n << dec;
    d << "opacity=" << n->opacity()
      << "combined=" << n->combinedOpacity()
      << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QML_RUNTIME_TESTING
    d << n->description;
#endif
    d << "dirty=" << hex << (int) n->dirtyFlags() << dec;
    d << ")";
    return d;
}


QDebug operator<<(QDebug d, const QSGRootNode *n)
{
    if (!n) {
        d << "QSGRootNode(null)";
        return d;
    }
    d << "QSGRootNode" << hex << (void *) n << "dirty=" << (int) n->dirtyFlags() << dec
      << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QML_RUNTIME_TESTING
    d << n->description;
#endif
    d << ")";
    return d;
}



QDebug operator<<(QDebug d, const QSGNode *n)
{
    if (!n) {
        d << "QSGNode(null)";
        return d;
    }
    switch (n->type()) {
    case QSGNode::GeometryNodeType:
        d << static_cast<const QSGGeometryNode *>(n);
        break;
    case QSGNode::TransformNodeType:
        d << static_cast<const QSGTransformNode *>(n);
        break;
    case QSGNode::ClipNodeType:
        d << static_cast<const QSGClipNode *>(n);
        break;
    case QSGNode::RootNodeType:
        d << static_cast<const QSGRootNode *>(n);
        break;
    case QSGNode::OpacityNodeType:
        d << static_cast<const QSGOpacityNode *>(n);
        break;
    default:
        d << "QSGNode(" << hex << (void *) n << dec
          << "dirty=" << hex << (int) n->dirtyFlags() << dec
          << (n->isSubtreeBlocked() ? "*BLOCKED*" : "");
#ifdef QML_RUNTIME_TESTING
        d << n->description;
#endif
        d << ")";
        break;
    }
    return d;
}


/*!
    \class QSGNodeDumper
    \brief The QSGNodeDumper class provides a way of dumping a scene grahp to the console.

    This class is solely for debugging purposes.

    \internal
 */

void QSGNodeDumper::dump(QSGNode *n)
{
    QSGNodeDumper dump;
    dump.visitNode(n);
}

void QSGNodeDumper::visitNode(QSGNode *n)
{
    if (n->isSubtreeBlocked())
        return;
    qDebug() << QString(m_indent * 2, QLatin1Char(' ')) << n;
    QSGNodeVisitor::visitNode(n);
}

void QSGNodeDumper::visitChildren(QSGNode *n)
{
    ++m_indent;
    QSGNodeVisitor::visitChildren(n);
    --m_indent;
}

#endif

QT_END_NAMESPACE
