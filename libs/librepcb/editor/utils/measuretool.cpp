/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "measuretool.h"

#include "../editorcommandset.h"
#include "../widgets/graphicsview.h"

#include <librepcb/core/graphics/graphicsscene.h>
#include <librepcb/core/library/pkg/footprint.h>
#include <librepcb/core/library/sym/symbol.h>
#include <librepcb/core/project/board/board.h>
#include <librepcb/core/project/board/items/bi_device.h>
#include <librepcb/core/project/board/items/bi_hole.h>
#include <librepcb/core/project/board/items/bi_netpoint.h>
#include <librepcb/core/project/board/items/bi_netsegment.h>
#include <librepcb/core/project/board/items/bi_plane.h>
#include <librepcb/core/project/board/items/bi_polygon.h>
#include <librepcb/core/project/board/items/bi_stroketext.h>
#include <librepcb/core/project/board/items/bi_via.h>
#include <librepcb/core/project/schematic/items/si_netlabel.h>
#include <librepcb/core/project/schematic/items/si_netpoint.h>
#include <librepcb/core/project/schematic/items/si_netsegment.h>
#include <librepcb/core/project/schematic/items/si_polygon.h>
#include <librepcb/core/project/schematic/items/si_symbol.h>
#include <librepcb/core/project/schematic/items/si_text.h>
#include <librepcb/core/project/schematic/schematic.h>
#include <librepcb/core/types/angle.h>
#include <librepcb/core/types/gridproperties.h>
#include <librepcb/core/utils/transform.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

MeasureTool::MeasureTool(GraphicsView& view, QObject* parent) noexcept
  : QObject(parent),
    mView(&view),
    mSnapCandidates(),
    mLastScenePos(),
    mCursorPos(),
    mCursorSnapped(false),
    mStartPos(),
    mEndPos() {
}

MeasureTool::~MeasureTool() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

void MeasureTool::setFootprint(const Footprint* footprint) noexcept {
  mSnapCandidates.clear();
  if (footprint) {
    mSnapCandidates |= snapCandidatesFromFootprint(*footprint, Transform());
  }
}

void MeasureTool::setSymbol(const Symbol* symbol) noexcept {
  mSnapCandidates.clear();
  if (symbol) {
    mSnapCandidates |= snapCandidatesFromSymbol(*symbol, Transform());
  }
}

void MeasureTool::setSchematic(const Schematic* schematic) noexcept {
  mSnapCandidates.clear();
  if (schematic) {
    foreach (const SI_Symbol* symbol, schematic->getSymbols()) {
      mSnapCandidates.insert(symbol->getPosition());
      mSnapCandidates |=
          snapCandidatesFromSymbol(symbol->getLibSymbol(), Transform(*symbol));
    }
    foreach (const SI_NetSegment* segment, schematic->getNetSegments()) {
      foreach (const SI_NetPoint* netpoint, segment->getNetPoints()) {
        mSnapCandidates.insert(netpoint->getPosition());
      }
      foreach (const SI_NetLabel* netlabel, segment->getNetLabels()) {
        mSnapCandidates.insert(netlabel->getPosition());
      }
    }
    foreach (const SI_Polygon* polygon, schematic->getPolygons()) {
      mSnapCandidates |=
          snapCandidatesFromPath(polygon->getPolygon().getPath());
    }
    foreach (const SI_Text* text, schematic->getTexts()) {
      mSnapCandidates.insert(text->getPosition());
    }
  }
}

void MeasureTool::setBoard(const Board* board) noexcept {
  mSnapCandidates.clear();
  if (board) {
    foreach (const BI_Device* device, board->getDeviceInstances()) {
      mSnapCandidates.insert(device->getPosition());
      mSnapCandidates |= snapCandidatesFromFootprint(device->getLibFootprint(),
                                                     Transform(*device));
    }
    foreach (const BI_NetSegment* segment, board->getNetSegments()) {
      foreach (const BI_NetPoint* netpoint, segment->getNetPoints()) {
        mSnapCandidates.insert(netpoint->getPosition());
      }
      foreach (const BI_Via* via, segment->getVias()) {
        mSnapCandidates.insert(via->getPosition());
        Path path = via->getVia().getOutline();
        path.addVertex(Point(via->getSize() / 2, 0));
        path.addVertex(Point(-via->getSize() / 2, 0));
        path.addVertex(Point(0, via->getSize() / 2));
        path.addVertex(Point(0, -via->getSize() / 2));
        mSnapCandidates |=
            snapCandidatesFromPath(path.translated(via->getPosition()));
        mSnapCandidates |= snapCandidatesFromCircle(via->getPosition(),
                                                    *via->getDrillDiameter());
      }
    }
    foreach (const BI_Plane* plane, board->getPlanes()) {
      mSnapCandidates |= snapCandidatesFromPath(plane->getOutline());
      foreach (const Path& fragment, plane->getFragments()) {
        mSnapCandidates |= snapCandidatesFromPath(fragment);
      }
    }
    foreach (const BI_Polygon* polygon, board->getPolygons()) {
      mSnapCandidates |=
          snapCandidatesFromPath(polygon->getPolygon().getPath());
    }
    foreach (const BI_StrokeText* text, board->getStrokeTexts()) {
      mSnapCandidates.insert(text->getPosition());
    }
    foreach (const BI_Hole* hole, board->getHoles()) {
      foreach (const Vertex& vertex, hole->getHole().getPath()->getVertices()) {
        mSnapCandidates |= snapCandidatesFromCircle(
            vertex.getPos(), *hole->getHole().getDiameter());
      }
    }
  }
}

void MeasureTool::enter() noexcept {
  if (mView) {
    if (GraphicsScene* scene = mView->getScene()) {
      scene->setSelectionArea(QPainterPath());  // clear selection
    }
    mView->setGrayOut(true);
    mView->setCursor(Qt::CrossCursor);
    mLastScenePos = mView->mapGlobalPosToScenePos(QCursor::pos(), true, true);
  }
  updateCursorPosition(0);
  updateStatusBarMessage();
}

void MeasureTool::leave() noexcept {
  // Note: Do not clear the current start/end points to make the ruler
  // re-appear on the same coordinates when re-entering this tool some time
  // later. This might be useful in some cases to avoid needing to measure the
  // same distance again.

  if (mView) {
    mView->unsetCursor();
    mView->setOverlayText(QString());
    mView->setSceneCursor(tl::nullopt);
    mView->setRulerPositions(tl::nullopt);
    mView->setGrayOut(false);
  }

  emit statusBarMessageChanged(QString());
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool MeasureTool::processKeyPressed(const QKeyEvent& e) noexcept {
  if (e.key() == Qt::Key_Shift) {
    updateCursorPosition(e.modifiers());
    return true;
  }

  return false;
}

bool MeasureTool::processKeyReleased(const QKeyEvent& e) noexcept {
  if (e.key() == Qt::Key_Shift) {
    updateCursorPosition(e.modifiers());
    return true;
  }

  return false;
}

bool MeasureTool::processGraphicsSceneMouseMoved(
    QGraphicsSceneMouseEvent& e) noexcept {
  mLastScenePos = Point::fromPx(e.scenePos());
  updateCursorPosition(e.modifiers());
  return true;
}

bool MeasureTool::processGraphicsSceneLeftMouseButtonPressed(
    QGraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);

  if ((!mStartPos) || mEndPos) {
    // Set first point.
    mStartPos = mCursorPos;
    mEndPos = tl::nullopt;
  } else {
    // Set second point.
    mEndPos = mCursorPos;
  }

  updateRulerPositions();
  updateStatusBarMessage();

  return true;
}

bool MeasureTool::processCopy() noexcept {
  if (mView && mStartPos && mEndPos) {
    const LengthUnit& unit = mView->getGridProperties().getUnit();
    const Point diff = (*mEndPos) - (*mStartPos);
    const qreal value = unit.convertToUnit(*diff.getLength());
    const QString str = Toolbox::floatToString(value, 12, QLocale());
    qApp->clipboard()->setText(str);
    emit statusBarMessageChanged(tr("Copied to clipboard: %1").arg(str), 3000);
    return true;
  }

  return false;
}

bool MeasureTool::processRemove() noexcept {
  if (mStartPos && mEndPos) {
    mStartPos = tl::nullopt;
    mEndPos = tl::nullopt;
    updateRulerPositions();
    updateStatusBarMessage();
    return true;
  }

  return false;
}

bool MeasureTool::processAbortCommand() noexcept {
  if (mStartPos && (!mEndPos)) {
    mStartPos = tl::nullopt;
    updateRulerPositions();
    updateStatusBarMessage();
    return true;
  }

  return false;
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

QSet<Point> MeasureTool::snapCandidatesFromSymbol(
    const Symbol& symbol, const Transform& transform) noexcept {
  QSet<Point> candidates;
  for (const SymbolPin& p : symbol.getPins()) {
    candidates.insert(transform.map(p.getPosition()));
    candidates.insert(transform.map(
        p.getPosition() + Point(*p.getLength(), 0).rotated(p.getRotation())));
  }
  for (const Polygon& p : symbol.getPolygons()) {
    candidates |= snapCandidatesFromPath(transform.map(p.getPath()));
  }
  for (const Circle& c : symbol.getCircles()) {
    candidates |= snapCandidatesFromCircle(transform.map(c.getCenter()),
                                           *c.getDiameter());
  }
  for (const Text& s : symbol.getTexts()) {
    candidates.insert(transform.map(s.getPosition()));
  }
  return candidates;
}

QSet<Point> MeasureTool::snapCandidatesFromFootprint(
    const Footprint& footprint, const Transform& transform) noexcept {
  QSet<Point> candidates;
  for (const FootprintPad& p : footprint.getPads()) {
    candidates.insert(transform.map(p.getPosition()));
    Path path = p.getOutline();
    path.addVertex(Point(p.getWidth() / 2, 0));
    path.addVertex(Point(-p.getWidth() / 2, 0));
    path.addVertex(Point(0, p.getHeight() / 2));
    path.addVertex(Point(0, -p.getHeight() / 2));
    candidates |= snapCandidatesFromPath(transform.map(
        path.rotated(p.getRotation()).translated(p.getPosition())));
    if (p.getDrillDiameter() > 0) {
      candidates |= snapCandidatesFromCircle(transform.map(p.getPosition()),
                                             *p.getDrillDiameter());
    }
  }
  for (const Polygon& p : footprint.getPolygons()) {
    candidates |= snapCandidatesFromPath(transform.map(p.getPath()));
  }
  for (const Circle& c : footprint.getCircles()) {
    candidates |= snapCandidatesFromCircle(transform.map(c.getCenter()),
                                           *c.getDiameter());
  }
  for (const StrokeText& s : footprint.getStrokeTexts()) {
    candidates.insert(transform.map(s.getPosition()));
  }
  for (const Hole& h : footprint.getHoles()) {
    foreach (const Vertex& vertex, h.getPath()->getVertices()) {
      candidates |= snapCandidatesFromCircle(transform.map(vertex.getPos()),
                                             *h.getDiameter());
    }
  }
  return candidates;
}

QSet<Point> MeasureTool::snapCandidatesFromPath(const Path& path) noexcept {
  QSet<Point> candidates;
  foreach (const Vertex& v, path.getVertices()) {
    candidates.insert(v.getPos());
  }
  return candidates;
}

QSet<Point> MeasureTool::snapCandidatesFromCircle(
    const Point& center, const Length& diameter) noexcept {
  QSet<Point> candidates;
  candidates.insert(center);
  candidates.insert(center + Point(0, diameter / 2));
  candidates.insert(center + Point(0, -diameter / 2));
  candidates.insert(center + Point(diameter / 2, 0));
  candidates.insert(center + Point(-diameter / 2, 0));
  return candidates;
}

void MeasureTool::updateCursorPosition(
    Qt::KeyboardModifiers modifiers) noexcept {
  if (!mView) {
    return;
  }

  mCursorPos = mLastScenePos;
  mCursorSnapped = false;
  if (!modifiers.testFlag(Qt::ShiftModifier)) {
    Point nearestCandidate;
    Length nearestDistance(-1);
    foreach (const Point& candidate, mSnapCandidates) {
      const UnsignedLength distance = (mCursorPos - candidate).getLength();
      if ((nearestDistance < 0) || (distance < nearestDistance)) {
        nearestCandidate = candidate;
        nearestDistance = *distance;
      }
    }

    const Point posOnGrid =
        mCursorPos.mappedToGrid(mView->getGridProperties().getInterval());
    const Length gridDistance = *(mCursorPos - posOnGrid).getLength();
    if ((nearestDistance >= 0) && (nearestDistance <= gridDistance)) {
      mCursorPos = nearestCandidate;
      mCursorSnapped = true;
    } else {
      mCursorPos = posOnGrid;
    }
  }
  updateRulerPositions();
}

void MeasureTool::updateRulerPositions() noexcept {
  if (!mView) {
    return;
  }

  GraphicsView::CursorOptions cursorOptions = 0;
  if ((!mStartPos) || mEndPos) {
    cursorOptions |= GraphicsView::CursorOption::Cross;
  }
  if (mCursorSnapped) {
    cursorOptions |= GraphicsView::CursorOption::Circle;
  }
  mView->setSceneCursor(std::make_pair(mCursorPos, cursorOptions));

  const Point startPos = mStartPos ? *mStartPos : mCursorPos;
  const Point endPos = mEndPos ? *mEndPos : mCursorPos;
  if (mStartPos) {
    mView->setRulerPositions(std::make_pair(startPos, endPos));
  } else {
    mView->setRulerPositions(tl::nullopt);
  }

  const Point diff = endPos - startPos;
  const UnsignedLength length = diff.getLength();
  const Angle angle =
      Angle::fromRad(qAtan2(diff.toMmQPointF().y(), diff.toMmQPointF().x()));
  const LengthUnit& unit = mView->getGridProperties().getUnit();
  int decimals = unit.getReasonableNumberOfDecimals() + 1;

  QString text;
  text += QString("X0: %1 %2<br>")
              .arg(unit.convertToUnit(startPos.getX()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("Y0: %1 %2<br>")
              .arg(unit.convertToUnit(startPos.getY()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("X1: %1 %2<br>")
              .arg(unit.convertToUnit(endPos.getX()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("Y1: %1 %2<br>")
              .arg(unit.convertToUnit(endPos.getY()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("<br>");
  text += QString("ΔX: %1 %2<br>")
              .arg(unit.convertToUnit(diff.getX()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("ΔY: %1 %2<br>")
              .arg(unit.convertToUnit(diff.getY()), 10, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("<br>");
  text += QString("<b>Δ: %1 %2</b><br>")
              .arg(unit.convertToUnit(*length), 11, 'f', decimals)
              .arg(unit.toShortStringTr());
  text += QString("<b>∠: %1°</b>").arg(angle.toDeg(), 14 - decimals, 'f', 3);
  text.replace(" ", "&nbsp;");
  mView->setOverlayText(text);
}

void MeasureTool::updateStatusBarMessage() noexcept {
  const QList<QKeySequence> copyKeys =
      EditorCommandSet::instance().clipboardCopy.getKeySequences();
  const QList<QKeySequence> deleteKeys =
      EditorCommandSet::instance().remove.getKeySequences();
  const QString disableSnapNote = " " %
      tr("(press %1 to disable snap)")
          .arg(QCoreApplication::translate("QShortcut", "Shift"));

  if (mEndPos && (!copyKeys.isEmpty()) && (!deleteKeys.isEmpty())) {
    emit statusBarMessageChanged(
        tr("Press %1 to copy the value to clipboard or %2 to clear the "
           "measurement")
            .arg(copyKeys.first().toString(QKeySequence::NativeText))
            .arg(deleteKeys.first().toString(QKeySequence::NativeText)));
  } else if (mStartPos && (!mEndPos)) {
    emit statusBarMessageChanged(tr("Click to specify the end point") %
                                 disableSnapNote);
  } else {
    emit statusBarMessageChanged(tr("Click to specify the start point") %
                                 disableSnapNote);
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace librepcb
