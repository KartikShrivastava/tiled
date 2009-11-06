/*
 * Tiled Map Editor (Qt)
 * Copyright 2009 Tiled (Qt) developers (see AUTHORS file)
 *
 * This file is part of Tiled (Qt).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stampbrush.h"

#include "brushitem.h"
#include "map.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"
#include "tilelayer.h"

#include <QGraphicsSceneMouseEvent>

using namespace Tiled;
using namespace Tiled::Internal;

StampBrush::StampBrush(QObject *parent)
    : AbstractTileTool(tr("Stamp Brush"),
                       QIcon(QLatin1String(
                               ":images/22x22/stock-tool-clone.png")),
                       parent)
    , mMapDocument(0)
    , mStamp(0)
    , mPainting(false)
    , mCapturing(false)
    , mStampX(0), mStampY(0)
{
}

StampBrush::~StampBrush()
{
    delete mStamp;
}

void StampBrush::enable(MapScene *scene)
{
    AbstractTileTool::enable(scene);
    setMapDocument(mapScene()->mapDocument());
    brushItem()->setTileLayer(mStamp);
}

void StampBrush::tilePositionChanged(const QPoint &)
{
    updatePosition();

    if (mPainting) {
        doPaint();
    } else if (mCapturing) {
        brushItem()->setTileSize(capturedArea().size());
    }
}

void StampBrush::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if (brushItem()->isVisible()) {
        if (mouseEvent->button() == Qt::LeftButton)
            beginPaint();
        else if (mouseEvent->button() == Qt::RightButton)
            beginCapture();
        mouseEvent->accept();
    }
}

void StampBrush::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if (mPainting && mouseEvent->button() == Qt::LeftButton) {
        endPaint();
        mouseEvent->accept();
    } else if (mCapturing && mouseEvent->button() == Qt::RightButton) {
        endCapture();
        mouseEvent->accept();
    }
}

void StampBrush::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    mMapDocument = mapDocument;
    brushItem()->setMapDocument(mMapDocument);

    // Reset the brush, since it probably became invalid
    brushItem()->setTileSize(1, 1);
    setStamp(0);
}

void StampBrush::setStamp(TileLayer *stamp)
{
    if (mStamp == stamp)
        return;

    brushItem()->setTileLayer(stamp);
    delete mStamp;
    mStamp = stamp;

    updatePosition();
}

void StampBrush::beginPaint()
{
    if (mPainting || mCapturing)
        return;

    mPainting = true;
    doPaint();
}

void StampBrush::endPaint()
{
    mPainting = false;
}

void StampBrush::beginCapture()
{
    if (mPainting || mCapturing)
        return;

    mCaptureStart = tilePosition();
    mCapturing = true;

    setStamp(0);
    brushItem()->setTileSize(1, 1);
}

void StampBrush::endCapture()
{
    if (!mCapturing)
        return;

    mCapturing = false;

    TileLayer *tileLayer = currentTileLayer();
    Q_ASSERT(tileLayer);

    // Intersect with the layer and translate to layer coordinates
    QRect captured = capturedArea();
    captured.intersect(QRect(tileLayer->x(), tileLayer->y(),
                             tileLayer->width(), tileLayer->height()));

    if (captured.isValid()) {
        captured.translate(-tileLayer->x(), -tileLayer->y());
        setStamp(tileLayer->copy(captured));
    } else {
        updatePosition();
    }
}

QRect StampBrush::capturedArea() const
{
    QRect captured = QRect(mCaptureStart, tilePosition()).normalized();
    if (captured.width() == 0)
        captured.adjust(-1, 0, 1, 0);
    if (captured.height() == 0)
        captured.adjust(0, -1, 0, 1);
    return captured;
}

void StampBrush::doPaint()
{
    if (!mStamp)
        return;

    // This method shouldn't be called when current layer is not a tile layer
    TileLayer *tileLayer = currentTileLayer();
    Q_ASSERT(tileLayer);

    if (!tileLayer->bounds().intersects(QRect(mStampX, mStampY,
                                              mStamp->width(),
                                              mStamp->height())))
        return;

    PaintTileLayer *paint = new PaintTileLayer(mMapDocument, tileLayer,
                                               mStampX, mStampY, mStamp);
    mMapDocument->undoStack()->push(paint);
}

/**
 * Updates the position of the brush item.
 */
void StampBrush::updatePosition()
{
    const QPoint tilePos = tilePosition();
    QPoint newPos;

    if (mCapturing) {
        newPos = QPoint(qMin(tilePos.x(), mCaptureStart.x()),
                        qMin(tilePos.y(), mCaptureStart.y()));
    } else if (mStamp) {
        mStampX = tilePos.x() - mStamp->width() / 2;
        mStampY = tilePos.y() - mStamp->height() / 2;
        newPos = QPoint(mStampX, mStampY);
    } else {
        newPos = tilePos;
    }

    brushItem()->setTilePos(newPos);
}
