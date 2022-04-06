/*
 * painttilelayer.cpp
 * Copyright 2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "painttilelayer.h"

#include "map.h"
#include "mapdocument.h"
#include "tilelayer.h"
#include "tilepainter.h"

#include <QCoreApplication>

using namespace Tiled;

PaintTileLayer::PaintTileLayer(MapDocument *mapDocument, QUndoCommand *parent)
    : QUndoCommand(parent)
    , mMapDocument(mapDocument)
    , mMergeable(false)
{
    setText(QCoreApplication::translate("Undo Commands", "Paint"));
}

PaintTileLayer::PaintTileLayer(MapDocument *mapDocument,
                               TileLayer *target,
                               int x,
                               int y,
                               const TileLayer *source,
                               QUndoCommand *parent)
    : PaintTileLayer(mapDocument, parent)
{
    paint(target, x, y,
          std::unique_ptr<TileLayer>(source->clone()),
          source->region().translated(QPoint(x, y) - source->position()));
}

PaintTileLayer::PaintTileLayer(MapDocument *mapDocument,
                               TileLayer *target,
                               int x,
                               int y,
                               const TileLayer *source,
                               const QRegion &paintRegion,
                               QUndoCommand *parent)
    : PaintTileLayer(mapDocument, parent)
{
    paint(target, x, y,
          std::unique_ptr<TileLayer>(source->clone()),
          paintRegion);
}

PaintTileLayer::PaintTileLayer(MapDocument *mapDocument,
                               TileLayer *target,
                               int x,
                               int y,
                               std::unique_ptr<TileLayer> source,
                               const QRegion &paintRegion,
                               QUndoCommand *parent)
    : PaintTileLayer(mapDocument, parent)
{
    paint(target, x, y, std::move(source), paintRegion);
}

PaintTileLayer::~PaintTileLayer()
{
}

void PaintTileLayer::paint(TileLayer *target,
                           int x,
                           int y,
                           std::unique_ptr<TileLayer> source,
                           const QRegion &paintRegion)
{
    auto &data = mLayerData[target];

    // If we haven't touched this target layer before, move over the source
    if (!data.mSource) {
        data.mSource = std::move(source);
        data.mErased = std::make_unique<TileLayer>();
        data.mErased->setCells(target->x(), target->y(), target, paintRegion);
        data.mX = x;
        data.mY = y;
        data.mPaintedRegion = paintRegion;
        return;
    }

    const QRegion combinedRegion = data.mPaintedRegion.united(paintRegion);
    const QRegion addedRegion = combinedRegion.subtracted(data.mPaintedRegion);

    // Copy the additionally painted tiles
    data.mPaintedRegion = combinedRegion;
    data.mSource->setCells(x - data.mSource->x(),
                           y - data.mSource->y(),
                           source.get(),
                           paintRegion.translated(-source->position()));

    // Copy the additionally erased tiles
    data.mErased->setCells(target->x(), target->y(), target, addedRegion);
}

void PaintTileLayer::undo()
{
    for (const auto& [tileLayer, data] : mLayerData) {
        TilePainter painter(mMapDocument, tileLayer);
        painter.setCells(0, 0, data.mErased.get(), data.mPaintedRegion);
    }

    QUndoCommand::undo(); // undo child commands
}

void PaintTileLayer::redo()
{
    QUndoCommand::redo(); // redo child commands

    for (const auto& [tileLayer, data] : mLayerData) {
        TilePainter painter(mMapDocument, tileLayer);
        painter.setCells(data.mX, data.mY, data.mSource.get(), data.mPaintedRegion);
    }
}

void PaintTileLayer::LayerData::mergeWith(const PaintTileLayer::LayerData &o)
{
    if (!mSource) {
        mSource.reset(o.mSource->clone());
        mErased.reset(o.mErased->clone());
        mX = o.mX;
        mY = o.mY;
        mPaintedRegion = o.mPaintedRegion;
        return;
    }

    const QRegion combinedRegion = mPaintedRegion.united(o.mPaintedRegion);
    const QRegion newRegion = combinedRegion.subtracted(mPaintedRegion);

    // Copy the painted tiles from the other command over
    mPaintedRegion = combinedRegion;
    mSource->setCells(o.mX - mSource->x(),
                      o.mY - mSource->y(),
                      o.mSource.get(),
                      o.mPaintedRegion.translated(-mSource->position()));

    // Copy the newly erased tiles from the other command over
    for (const QRect &rect : newRegion)
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
                mErased->setCell(x, y, o.mErased->cellAt(x, y));
}

bool PaintTileLayer::mergeWith(const QUndoCommand *other)
{
    const PaintTileLayer *o = static_cast<const PaintTileLayer*>(other);
    if (!(mMapDocument == o->mMapDocument && o->mMergeable))
        return false;
    if (!cloneChildren(other, this))
        return false;

    for (const auto& [tileLayer, data] : o->mLayerData)
        mLayerData[tileLayer].mergeWith(data);

    return true;
}
