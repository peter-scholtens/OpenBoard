/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */



#include <QString>
#include <QCursor>
#include <QGraphicsRectItem>

#include "UBDocumentThumbnailsView.h"
#include "UBRubberBand.h"
#include "UBMainWindow.h"

#include <QWidget>

#include "board/UBBoardController.h"

#include "core/UBSettings.h"
#include "core/UBApplication.h"

#include "document/UBDocumentProxy.h"
#include "document/UBDocumentController.h"

#include "board/UBDrawingController.h"

#include "core/memcheck.h"

UBDocumentThumbnailsView::UBDocumentThumbnailsView(QWidget* parent)
    : QGraphicsView(parent)
    , mThumbnailWidth(UBSettings::defaultThumbnailWidth)
    , mSpacing(UBSettings::thumbnailSpacing)
    , mLastSelectedThumbnail(0)
    , mSelectionSpan(0)
    , mPrevLassoRect(QRect())
    , mLassoRectItem(0)

{
    // By default, the drag is possible
    bCanDrag = true;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    setFrameShape(QFrame::NoFrame);
    setScene(&mThumbnailsScene);

    setAlignment(Qt::AlignLeft | Qt::AlignTop);

}


UBDocumentThumbnailsView::~UBDocumentThumbnailsView()
{
}


void UBDocumentThumbnailsView::setThumbnailWidth(qreal pThumbnailWidth)
{
    mThumbnailWidth = pThumbnailWidth;

    refreshScene();
}


void UBDocumentThumbnailsView::setSpacing(qreal pSpacing)
{
    mSpacing = pSpacing;

    refreshScene();
}


void UBDocumentThumbnailsView::setGraphicsItems(const QList<QGraphicsItem*>& pGraphicsItems
        , const QList<QUrl>& pItemsPaths
        , const QStringList pLabels
        , const QString& pMimeType)
{
    Q_ASSERT(pItemsPaths.count() == pLabels.count());
    mGraphicItems = pGraphicsItems;
    mItemsPaths = pItemsPaths;
    mMimeType = pMimeType;
    mLabels = pLabels;

    foreach(QGraphicsItem* it, mThumbnailsScene.items())
    {
        mThumbnailsScene.removeItem(it, true);
    }
    // set lasso to 0 as it has been cleared as well
    mLassoRectItem = 0;

    foreach (QGraphicsItem* item, pGraphicsItems)
    {
        if (item->scene() != &mThumbnailsScene){
            mThumbnailsScene.addItem(item);
        }
    }

    mLabelsItems.clear();

    foreach (const QString label, pLabels)
    {
        UBThumbnailTextItem *labelItem =
            new UBThumbnailTextItem(label); // deleted while replace or by the scene destruction

        mThumbnailsScene.addItem(labelItem);
        mLabelsItems << labelItem;
    }

    refreshScene();

    mLastSelectedThumbnail = 0;
}

void UBDocumentThumbnailsView::insertThumbnailToScene(QGraphicsPixmapItem* newThumbnail, UBThumbnailTextItem* thumbnailTextItem)
{
       mThumbnailsScene.addItem(newThumbnail);
       mThumbnailsScene.addItem(thumbnailTextItem);
}


void UBDocumentThumbnailsView::refreshScene()
{
    int nbColumns = (geometry().width() - mSpacing) / (mThumbnailWidth + mSpacing);

    int labelSpacing = 0;

    if (mLabelsItems.size() > 0)
    {
        QFontMetrics fm(mLabelsItems.at(0)->font());
        labelSpacing = UBSettings::thumbnailSpacing + fm.height();  // TODO UB 4.x where is 20 from ??? configure ?? compute based on mSpacing ?? JBA Is it the font height?
    }
    nbColumns = qMax(nbColumns, 1);

    qreal thumbnailHeight = mThumbnailWidth / UBSettings::minScreenRatio;

    for (int i = 0; i < mGraphicItems.size(); i++)
    {
        QGraphicsItem* item = mGraphicItems.at(i);

        if (!item)
            continue;

        UBSceneThumbnailPixmap *thumbnail = dynamic_cast<UBSceneThumbnailPixmap*>(item);
        if (thumbnail)
            thumbnail->setSceneIndex(i);

        qreal scaleWidth = mThumbnailWidth / item->boundingRect().width();
        qreal scaleHeight = thumbnailHeight / item->boundingRect().height();

        qreal scaleFactor = qMin(scaleWidth, scaleHeight);

        //bitmap should not be stretched
        UBThumbnail* pix = dynamic_cast<UBThumbnail*>(item);
        if (pix)
            scaleFactor = qMin(scaleFactor, 1.0);

        QTransform transform;
        transform.scale(scaleFactor, scaleFactor);

        item->setTransform(transform);

        item->setFlag(QGraphicsItem::ItemIsSelectable, true);

        int columnIndex = i % nbColumns;
        int rowIndex = i / nbColumns;

        if (pix)
        {
            pix->setColumn(columnIndex);
            pix->setRow(rowIndex);
        }

        int w = item->boundingRect().width();
        int h = item->boundingRect().height();
        QPointF pos(
                mSpacing + (mThumbnailWidth - w * scaleFactor) / 2 + columnIndex * (mThumbnailWidth + mSpacing),
                mSpacing + rowIndex * (thumbnailHeight + mSpacing + labelSpacing) + (thumbnailHeight - h * scaleFactor) / 2);

        item->setPos(pos);

        if (mLabelsItems.size() > i)
        {
            mLabelsItems.at(i)->setWidth(mThumbnailWidth);
            mLabelsItems.at(i)->setPageNumber(i+1);
            QFontMetrics fm(mLabelsItems.at(i)->font(), this);
            QString elidedText = fm.elidedText(mLabelsItems.at(i)->toPlainText(), Qt::ElideRight, mThumbnailWidth);

            mLabelsItems.at(i)->setPlainText(elidedText);
            mLabelsItems.at(i)->setWidth(fm.horizontalAdvance(elidedText) + 2 * mLabelsItems.at(i)->document()->documentMargin());

            pos.setY(pos.y() + (thumbnailHeight + h * scaleFactor) / 2 + 5);
            qreal labelWidth = fm.horizontalAdvance(elidedText);
            pos.setX(mSpacing + (mThumbnailWidth - labelWidth) / 2 + columnIndex * (mThumbnailWidth + mSpacing));
            mLabelsItems.at(i)->setPos(pos);
        }
    }

    QScrollBar *vertScrollBar = verticalScrollBar();
    int scrollBarThickness = 0;
    if (vertScrollBar && vertScrollBar->isVisible())
        scrollBarThickness = vertScrollBar->width();

    setSceneRect(0, 0,
            geometry().width() - scrollBarThickness,
            mSpacing + ((((mGraphicItems.size() - 1) / nbColumns) + 1) * (thumbnailHeight + mSpacing + labelSpacing)));
}


QList<QGraphicsItem*> UBDocumentThumbnailsView::selectedItems()
{
    QList<QGraphicsItem*> sortedSelectedItems = mThumbnailsScene.selectedItems();
    std::sort(sortedSelectedItems.begin(), sortedSelectedItems.end(), thumbnailLessThan);
    return sortedSelectedItems;
}

void UBDocumentThumbnailsView::clearSelection()
{
    mThumbnailsScene.clearSelection();
}


void UBDocumentThumbnailsView::mousePressEvent(QMouseEvent *event)
{
    mClickTime.restart();
    mMousePressPos = event->pos();

    UBThumbnailPixmap* sceneItem = dynamic_cast<UBThumbnailPixmap*>(itemAt(mMousePressPos));
    if(sceneItem==NULL)
    {
        event->ignore();
        return;
    }

    mMousePressScenePos = mapToScene(mMousePressPos);
    QGraphicsItem* underlyingItem = itemAt(mMousePressPos);
    UBThumbnail *previousSelectedThumbnail = mLastSelectedThumbnail;

    if (!dynamic_cast<UBThumbnail*>(underlyingItem))
    {
        deleteLasso();

        UBRubberBand rubberBand(QRubberBand::Rectangle);
        QStyleOption option;
        option.initFrom(&rubberBand);

        mPrevLassoRect = QRect();
        mLassoRectItem = new QGraphicsRectItem(0);
        scene()->addItem(mLassoRectItem);

#ifdef Q_OS_OSX
        // The following code must stay in synch with <Qt installation folder>\src\gui\styles\qmacstyle_mac.mm
        QColor strokeColor;
        strokeColor.setHsvF(0, 0, 0.86, 1.0);
        mLassoRectItem->setPen(QPen(strokeColor));
        QColor fillColor(option.palette.color(QPalette::Disabled, QPalette::Highlight));
        fillColor.setHsvF(0, 0, 0.53, 0.25);
        mLassoRectItem->setBrush(fillColor);
#else
        // The following code must stay in synch with <Qt installation folder>\src\gui\styles\qwindowsxpstyle.cpp
        QColor highlight = option.palette.color(QPalette::Active, QPalette::Highlight);
        mLassoRectItem->setPen(highlight.darker(120));
        QColor dimHighlight(qMin(highlight.red() / 2 + 110, 255),
                            qMin(highlight.green() / 2 + 110, 255),
                            qMin(highlight.blue() / 2 + 110, 255),
                            127);
        mLassoRectItem->setBrush(dimHighlight);
#endif

        mLassoRectItem->setZValue(10000);
        mLassoRectItem->setRect(QRectF(mMousePressScenePos, QSizeF()));

        if (Qt::ControlModifier & event->modifiers() || Qt::ShiftModifier & event->modifiers())
        {
           // mSelectedThumbnailItems = selectedItems().toSet();
            return;
        }

        mSelectedThumbnailItems.clear();
        mPreviouslyIncrementalSelectedItemsX.clear();
        mPreviouslyIncrementalSelectedItemsY.clear();
        QGraphicsView::mousePressEvent(event);
    }
    else if (Qt::ShiftModifier & event->modifiers())
    {
        if (previousSelectedThumbnail)
        {
            QGraphicsItem* previousSelectedItem = dynamic_cast<QGraphicsItem*>(previousSelectedThumbnail);
            if (previousSelectedItem)
            {
                int index1 = mGraphicItems.indexOf(previousSelectedItem);
                int index2 = mGraphicItems.indexOf(underlyingItem);
                if (-1 == index2)
                {
                    mSelectedThumbnailItems = selectedItems();
                    return;
                }
                mSelectionSpan = index2 - index1;
                selectItems(qMin(index1, index2), mSelectionSpan < 0 ? - mSelectionSpan + 1 : mSelectionSpan + 1);
            }
        }
    }
    else
    {
        mLastSelectedThumbnail = dynamic_cast<UBThumbnail*>(underlyingItem);
        if (!underlyingItem->isSelected())
        {
            int index = mGraphicItems.indexOf(underlyingItem);
            selectItemAt(index, Qt::ControlModifier & event->modifiers());
        }
        else
        {
            QGraphicsView::mousePressEvent(event);
        }
        if (!mLastSelectedThumbnail && mGraphicItems.count() > 0)
            mLastSelectedThumbnail = dynamic_cast<UBThumbnail*>(mGraphicItems.at(0));
        mSelectionSpan = 0;
    }

    UBApplication::documentController->pageSelectionChanged();
}


void UBDocumentThumbnailsView::mouseMoveEvent(QMouseEvent *event)
{
    int distance = (mMousePressPos - event->pos()).manhattanLength();

    if (0 == (event->buttons() & Qt::LeftButton) || distance < QApplication::startDragDistance())
        return;

    if (mLassoRectItem)
    {
        bSelectionInProgress = true;
        int incrementLassoMinWidth = 2;
        QPointF currentScenePos = mapToScene(event->pos());
        QRectF lassoRect(
            qMin(mMousePressScenePos.x(), currentScenePos.x()), qMin(mMousePressScenePos.y(), currentScenePos.y()),
            qAbs(mMousePressScenePos.x() - currentScenePos.x()), qAbs(mMousePressScenePos.y() - currentScenePos.y()));
        if (QPoint() == prevMoveMousePos)
            prevMoveMousePos = currentScenePos;
        QRectF incrementXSelection(
            qMin(prevMoveMousePos.x(), currentScenePos.x()), qMin(mMousePressScenePos.y(), currentScenePos.y()),
            qAbs(prevMoveMousePos.x() - currentScenePos.x())+incrementLassoMinWidth, qAbs(mMousePressScenePos.y() - currentScenePos.y()));
        QRectF incrementYSelection(
            qMin(mMousePressScenePos.x(), currentScenePos.x()), qMin(prevMoveMousePos.y(), currentScenePos.y()),
            qAbs(mMousePressScenePos.x() - currentScenePos.x()), qAbs(prevMoveMousePos.y() - currentScenePos.y())+incrementLassoMinWidth);

        prevMoveMousePos = currentScenePos;
        mLassoRectItem->setRect(lassoRect);

        QSet<QGraphicsItem*> lassoSelectedThumbnailItems;

        QSet<QGraphicsItem*> toUnset;
        QSet<QGraphicsItem*> toSet;

        // for horizontal moving
        const auto selectedItemsX = scene()->items(incrementXSelection, Qt::IntersectsItemBoundingRect);

        for (QGraphicsItem *lassoSelectedItem : selectedItemsX)
        {
            if (lassoSelectedItem)
            {
                UBThumbnailPixmap *thumbnailItem = dynamic_cast<UBThumbnailPixmap*>(lassoSelectedItem);
                if (thumbnailItem)
                     lassoSelectedThumbnailItems += lassoSelectedItem;
            }
        }

        if(lassoRect.width() < mPrevLassoRect.width())
        {
            if (!lassoSelectedThumbnailItems.contains(mPreviouslyIncrementalSelectedItemsX))
                toUnset += mPreviouslyIncrementalSelectedItemsX - lassoSelectedThumbnailItems;

        }
        mPreviouslyIncrementalSelectedItemsX = lassoSelectedThumbnailItems;

        toSet += lassoSelectedThumbnailItems + mPreviouslyIncrementalSelectedItemsX;


        lassoSelectedThumbnailItems.clear();

        // for vertical moving

        const auto selectedItemsY = scene()->items(incrementYSelection, Qt::IntersectsItemBoundingRect);

        for (QGraphicsItem *lassoSelectedItem : selectedItemsY)
        {
            if (lassoSelectedItem)
            {
                UBThumbnailPixmap *thumbnailItem = dynamic_cast<UBThumbnailPixmap*>(lassoSelectedItem);

                if (thumbnailItem)
                    lassoSelectedThumbnailItems += lassoSelectedItem;
            }
        }

        if(lassoRect.height() < mPrevLassoRect.height())
        {
            if (!lassoSelectedThumbnailItems.contains(mPreviouslyIncrementalSelectedItemsY))
                toUnset += mPreviouslyIncrementalSelectedItemsY - lassoSelectedThumbnailItems;

        }
        mPreviouslyIncrementalSelectedItemsY = lassoSelectedThumbnailItems;


        toSet += lassoSelectedThumbnailItems + mPreviouslyIncrementalSelectedItemsY;


        toSet -= toUnset;

        foreach (QGraphicsItem *item, toSet)
        {
            item->setSelected(true);
        }

        foreach (QGraphicsItem *item, toUnset)
        {
            item->setSelected(false);
        }

        mSelectedThumbnailItems += lassoSelectedThumbnailItems.values();
        mPrevLassoRect = lassoRect;

        if (Qt::ControlModifier & event->modifiers())
        {
            for (int i = 0; i < mSelectedThumbnailItems.count()-1; i++)
            {
                mSelectedThumbnailItems.at(i)->setSelected(true);
            }
        }
    }
    else
    {
        bSelectionInProgress = false;
        if (0 == selectedItems().size())
            return;

        if(bCanDrag)
        {
            QDrag *drag = new QDrag(this);
            QMimeData *mime = new QMimeData();

            if (mMimeType.length() > 0)
                mime->setData(mMimeType, QByteArray()); // trick the d&d system to register our own mime type

            drag->setMimeData(mime);

            QList<QUrl> qlElements;

            foreach (QGraphicsItem* item, selectedItems())
            {
                if (mGraphicItems.contains(item))
                {
                    if (mGraphicItems.indexOf(item) <= mItemsPaths.size()){
                        qlElements << mItemsPaths.at(mGraphicItems.indexOf(item));
                    }
                }
            }

            if (qlElements.size() > 0){
                            mime->setUrls(qlElements);
                            drag->setMimeData(mime);
                            drag->exec(Qt::CopyAction);
            }
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}


void UBDocumentThumbnailsView::mouseReleaseEvent(QMouseEvent *event)
{
    int elapsedTimeSincePress = mClickTime.elapsed();
    prevMoveMousePos = QPoint();
    deleteLasso();
    QGraphicsView::mouseReleaseEvent(event);

    if(elapsedTimeSincePress < STARTDRAGTIME) {
        emit mouseClick(itemAt(event->pos()), 0);
    }
}


void UBDocumentThumbnailsView::keyPressEvent(QKeyEvent *event)
{
    if (mLastSelectedThumbnail)
    {
        QGraphicsItem *lastSelectedGraphicsItem = dynamic_cast<QGraphicsItem*>(mLastSelectedThumbnail);
        if (!lastSelectedGraphicsItem) return;
        int startSelectionIndex = mGraphicItems.indexOf(lastSelectedGraphicsItem);
        int previousSelectedThumbnailIndex = startSelectionIndex + mSelectionSpan;

        switch (event->key())
        {
        case Qt::Key_Down:
        case Qt::Key_Up:
            {
                if (rowCount() <= 1) break;
                if (Qt::ShiftModifier & event->modifiers())
                {
                    int endSelectionIndex;
                    if (Qt::Key_Down == event->key())
                    {
                        endSelectionIndex = previousSelectedThumbnailIndex + columnCount();
                        if (endSelectionIndex >= mGraphicItems.count()) break;
                    }
                    else
                    {
                        endSelectionIndex = previousSelectedThumbnailIndex - columnCount();
                        if (endSelectionIndex < 0) break;
                    }

                    int startIndex = startSelectionIndex < endSelectionIndex ? startSelectionIndex : endSelectionIndex;
                    int count = startSelectionIndex < endSelectionIndex ? endSelectionIndex - startSelectionIndex + 1 : startSelectionIndex - endSelectionIndex + 1;
                    mSelectionSpan = startSelectionIndex < endSelectionIndex ? (count - 1) : - (count - 1);
                    selectItems(startIndex, count);
                }
                else
                {
                    int toSelectIndex;
                    if (Qt::Key_Down == event->key())
                    {
                        toSelectIndex = previousSelectedThumbnailIndex + columnCount();
                        if (toSelectIndex >= mGraphicItems.count()) break;
                    }
                    else
                    {
                        toSelectIndex = previousSelectedThumbnailIndex - columnCount();
                        if (toSelectIndex < 0) break;
                    }

                    selectItemAt(toSelectIndex, Qt::ControlModifier & event->modifiers());
                    mSelectionSpan = 0;
                }
            }
            break;

        case Qt::Key_Left:
        case Qt::Key_Right:
            {
                QGraphicsItem *previousSelectedItem = mGraphicItems.at(previousSelectedThumbnailIndex);
                UBThumbnail *previousSelectedThumbnail = dynamic_cast<UBThumbnail*>(previousSelectedItem);
                if (!previousSelectedThumbnail) break;

                if (Qt::Key_Left == event->key())
                {
                    if (0 == previousSelectedThumbnail->column()) break;
                }
                else
                {
                    if (previousSelectedThumbnail->column() == columnCount() - 1 ||
                        previousSelectedThumbnailIndex == mGraphicItems.count() - 1) break;
                }

                if (Qt::ShiftModifier & event->modifiers())
                {
                    int endSelectionIndex;
                    if (Qt::Key_Left == event->key())
                    {
                        endSelectionIndex = previousSelectedThumbnailIndex - 1;
                        if (endSelectionIndex < 0) break;
                    }
                    else
                    {
                        endSelectionIndex = previousSelectedThumbnailIndex + 1;
                        if (endSelectionIndex >= mGraphicItems.count()) break;
                    }

                    int startIndex = startSelectionIndex < endSelectionIndex ? startSelectionIndex : endSelectionIndex;
                    int count = startSelectionIndex < endSelectionIndex ? endSelectionIndex - startSelectionIndex + 1 : startSelectionIndex - endSelectionIndex + 1;
                    mSelectionSpan = startSelectionIndex < endSelectionIndex ? (count - 1) : - (count - 1);
                    selectItems(startIndex, count);
                }
                else
                {
                    if (Qt::Key_Left == event->key())
                        selectItemAt(previousSelectedThumbnailIndex - 1, Qt::ControlModifier & event->modifiers());
                    else
                        selectItemAt(previousSelectedThumbnailIndex + 1, Qt::ControlModifier & event->modifiers());

                    mSelectionSpan = 0;
                }
            }
            break;

        case Qt::Key_Home:
            {
                if (Qt::ShiftModifier & event->modifiers())
                {
                    mSelectionSpan = - startSelectionIndex;
                    selectItems(0, startSelectionIndex + 1);
                }
                else
                {
                    selectItemAt(0, Qt::ControlModifier & event->modifiers());
                    mSelectionSpan = 0;
                }
            }
            break;

        case Qt::Key_End:
            {
                if (Qt::ShiftModifier & event->modifiers())
                {
                    mSelectionSpan = mGraphicItems.count() - startSelectionIndex - 1;
                    selectItems(startSelectionIndex, mSelectionSpan + 1);
                }
                else
                {
                    selectItemAt(mGraphicItems.count() - 1, Qt::ControlModifier & event->modifiers());
                    mSelectionSpan = 0;
                }
            }
            break;
        case Qt::Key_A:
            {
                if (Qt::ControlModifier & event->modifiers())
                    selectAll();
            }
            break;
        }
    }
    QGraphicsView::keyPressEvent(event);
}


void UBDocumentThumbnailsView::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);

    if (0 == selectedItems().count() && mGraphicItems.count() > 0 && Qt::TabFocusReason == event->reason())
    {
        selectItemAt(0);
        mSelectionSpan = 0;
    }
}


void UBDocumentThumbnailsView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    refreshScene();

    emit resized();
}


void UBDocumentThumbnailsView::sceneSelectionChanged()
{
    emit selectionChanged();
}


void UBDocumentThumbnailsView::selectItemAt(int pIndex, bool extend)
{
    QGraphicsItem* itemToSelect = 0;

    if (pIndex >= 0 && pIndex < mGraphicItems.size())
        itemToSelect = mGraphicItems.at(pIndex);

    foreach (QGraphicsItem* item, items())
    {
        if (item == itemToSelect)
        {
            mLastSelectedThumbnail = dynamic_cast<UBThumbnail*>(item);
            item->setSelected(true);
            ensureVisible(item);
        }
        else if (!extend)
        {
            item->setSelected(false);
        }
    }
}

void UBDocumentThumbnailsView::unselectItemAt(int pIndex)
{
    if (pIndex >= 0 && pIndex < mGraphicItems.size())
    {
        QGraphicsItem *itemToUnselect = mGraphicItems.at(pIndex);
        itemToUnselect->setSelected(false);
    }
}


void UBDocumentThumbnailsView::selectItems(int startIndex, int count)
{
    for (int i = 0; i < mGraphicItems.count(); i++)
    {
        mGraphicItems.at(i)->setSelected(i >= startIndex && i < startIndex + count);
    }
}


void UBDocumentThumbnailsView::selectAll()
{
    foreach (QGraphicsItem* item, mGraphicItems)
    {
        item->setSelected(true);
    }
}

int UBDocumentThumbnailsView::rowCount() const
{
    UBThumbnail *lastThumbnail = dynamic_cast<UBThumbnail*>(mGraphicItems.last());
    return lastThumbnail ? lastThumbnail->row() + 1 : 0;
}

int UBDocumentThumbnailsView::columnCount() const
{
    UBThumbnail *lastThumbnail = dynamic_cast<UBThumbnail*>(mGraphicItems.last());
    if (!lastThumbnail) return 0;
    int lastRow = lastThumbnail->row();
    int lastColumn = lastThumbnail->column();
    return lastRow > 0 ? (mGraphicItems.count() - lastColumn - 1) / lastRow : mGraphicItems.count();
}


void UBDocumentThumbnailsView::mouseDoubleClickEvent(QMouseEvent * event)
{
    QGraphicsItem* item = itemAt(event->pos());

    if (item)
    {
        int index = mGraphicItems.indexOf(item);
        emit mouseDoubleClick(item, index);
    }
}


bool UBDocumentThumbnailsView::thumbnailLessThan(QGraphicsItem* item1, QGraphicsItem* item2)
{
    UBThumbnail *thumbnail1 = dynamic_cast<UBThumbnail*>(item1);
    UBThumbnail *thumbnail2 = dynamic_cast<UBThumbnail*>(item2);
    if (thumbnail1 && thumbnail2)
    {
        if (thumbnail1->row() != thumbnail2->row())
            return thumbnail1->row() < thumbnail2->row();
        else
            return thumbnail1->column() < thumbnail2->column();
    }
    return false;
}

void UBDocumentThumbnailsView::deleteLasso()
{
    if (mLassoRectItem && scene())
    {
        scene()->removeItem(mLassoRectItem);
        delete mLassoRectItem;
        mLassoRectItem = 0;
    }
}


UBThumbnail::UBThumbnail()
    : mAddedToScene(false)
{
    mSelectionItem = new QGraphicsRectItem(0, 0, 0, 0);
    mSelectionItem->setPen(QPen(UBSettings::treeViewBackgroundColor, 8));
    // TODO UB 4.x fix nasty dependencies : 8 is a bit less than half of UBThumbnailWidget.mSpacing
}

UBThumbnail::~UBThumbnail()
{
    if (mSelectionItem && !mAddedToScene)
        delete mSelectionItem;
}

UBSceneThumbnailNavigPixmap::UBSceneThumbnailNavigPixmap(const QPixmap& pix, std::shared_ptr<UBDocumentProxy> proxy, int pSceneIndex)
    : UBSceneThumbnailPixmap(pix, proxy, pSceneIndex)
    , mEditable(false)
{
    if(0 <= UBDocumentContainer::pageFromSceneIndex(pSceneIndex)){
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
    }
}

UBSceneThumbnailNavigPixmap::~UBSceneThumbnailNavigPixmap()
{

}

void UBSceneThumbnailNavigPixmap::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept();
    showUI();
    update();
}

void UBSceneThumbnailNavigPixmap::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept();
    hideUI();
    update();
}

void UBSceneThumbnailNavigPixmap::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    UBSceneThumbnailPixmap::paint(painter, option, widget);
    using namespace UBThumbnailUI;

    if (editable())
    {
        if(deletable())
            draw(painter, *getIcon("close"));
        else
            draw(painter, *getIcon("closeDisabled"));

        draw(painter, *getIcon("duplicate"));
    }
}

void UBSceneThumbnailNavigPixmap::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF p = event->pos();

    using namespace UBThumbnailUI;

    if (triggered(p.y()))
    {
        if(deletable() && getIcon("close")->triggered(p.x()))
        {
            event->accept();
            deletePage();
        }
        else if(getIcon("duplicate")->triggered(p.x()))
        {
            event->accept();
            duplicatePage();
        }
        else
        {
            event->ignore();
        }
        /*
        else if(movableUp() && getIcon("moveUp")->triggered(p.x()))
        {
            event->accept();
            moveUpPage();
        }
        else if (movableDown() && getIcon("moveDown")->triggered(p.x()))
        {
            event->accept();
            moveDownPage();
        }
        */
    }
    else
    {
        event->ignore();
    }
}

void UBSceneThumbnailNavigPixmap::deletePage()
{
    if(UBApplication::mainWindow->yesNoQuestion(QObject::tr("Remove Page"),
                                                QObject::tr("Are you sure you want to remove page %1 ?").arg(sceneIndex()),
                                                QPixmap(":/images/trash-document-page.png")))
    {
        UBApplication::boardController->deleteScene(sceneIndex());
    }
}

void UBSceneThumbnailNavigPixmap::duplicatePage()
{
    UBApplication::boardController->duplicateScene(sceneIndex());
}

void UBSceneThumbnailNavigPixmap::moveUpPage()
{
    if (sceneIndex()!=0)
        UBApplication::boardController->moveSceneToIndex(sceneIndex(), sceneIndex() - 1);
}

void UBSceneThumbnailNavigPixmap::moveDownPage()
{
    if (sceneIndex() < UBApplication::boardController->selectedDocument()->pageCount()-1)
        UBApplication::boardController->moveSceneToIndex(sceneIndex(), sceneIndex() + 1);
}


void UBWidgetTextThumbnailElement::Place(int row, int col, qreal width, qreal height)
{
    int labelSpacing = 0;
    if(this->caption)
    {
        QFontMetrics fm(this->caption->font());
        labelSpacing = UBSettings::thumbnailSpacing + fm.height();
    }
    if(this->thumbnail)
    {
        int w = this->thumbnail->boundingRect().width();
        int h = this->thumbnail->boundingRect().height();

        qreal scaleWidth = width / w;
        qreal scaleHeight = height / h;
        qreal scaleFactor = qMin(scaleWidth, scaleHeight);
        UBThumbnail* pix = dynamic_cast<UBThumbnail*>(this->thumbnail);

        QTransform transform;
        transform.scale(scaleFactor, scaleFactor);

        // Apply the scaling
        this->thumbnail->setTransform(transform);
        this->thumbnail->setFlag(QGraphicsItem::ItemIsSelectable, true);

        if(pix)
        {
            pix->setColumn(col);
            pix->setRow(row);
        }

        QPointF pos(border + (width - w * scaleFactor) / 2 + col * (width + border),
                    border + row * (height + border + labelSpacing) + (height - h * scaleFactor) / 2);

        this->thumbnail->setPos(pos);

        if(this->caption)
        {
            QFontMetrics fm(this->caption->font());
            QString elidedText = fm.elidedText(this->caption->toPlainText(), Qt::ElideRight, width);

            this->caption->setPlainText(elidedText);
            this->caption->setWidth(fm.horizontalAdvance(elidedText) + 2 * this->caption->document()->documentMargin());
            pos.setY(pos.y() + (height + h * scaleFactor) / 2 + 5); // What is this 5 ??
            qreal labelWidth = fm.horizontalAdvance(elidedText);
            pos.setX(border + (width - labelWidth) / 2 + col * (width + border));
            this->caption->setPos(pos);
        }
    }
}

void UBDraggableThumbnailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    UBThumbnailPixmapItem::paint(painter, option, widget);
    using namespace UBThumbnailUI;

    if (editable())
    {
        // apply inverse scaling to keep constant size of buttons
        painter->save();
        auto scale = 1 / transform().m11();
        painter->scale(scale, scale);

        if(deletable())
            draw(painter, *getIcon("close"));
        else
            draw(painter, *getIcon("closeDisabled"));

        draw(painter, *getIcon("duplicate"));

        /*
        if(movableUp())
            draw(painter, *getIcon("moveUp"));
        else
            draw(painter, *getIcon("moveUpDisabled"));

        if(movableDown())
            draw(painter, *getIcon("moveDown"));
        else
            draw(painter, *getIcon("moveDownDisabled"));
        */
        painter->restore();
    }
}

void UBDraggableThumbnailItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept();
    showUI();
    update();
}

void UBDraggableThumbnailItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept();
    hideUI();
    update();
}

void UBDraggableThumbnailItem::deletePage()
{
    if(UBApplication::mainWindow->yesNoQuestion(QObject::tr("Remove Page"),
                                                QObject::tr("Are you sure you want to remove page %1 ?").arg(sceneIndex()+1),
                                                QPixmap(":/images/trash-document-page.png")))
    {
        UBApplication::boardController->deleteScene(sceneIndex());
    }
}

void UBDraggableThumbnailItem::duplicatePage()
{
    UBApplication::boardController->duplicateScene(sceneIndex());
}

void UBDraggableThumbnailItem::moveUpPage()
{
    if (sceneIndex()!=0)
        UBApplication::boardController->moveSceneToIndex(sceneIndex(), sceneIndex() - 1);
}

void UBDraggableThumbnailItem::moveDownPage()
{
    if (sceneIndex() < UBApplication::boardController->selectedDocument()->pageCount()-1)
        UBApplication::boardController->moveSceneToIndex(sceneIndex(), sceneIndex() + 1);
}

void UBDraggableThumbnailItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF p = event->pos() * transform().m11();

    using namespace UBThumbnailUI;

    if (triggered(p.y()))
    {
        if(deletable() && getIcon("close")->triggered(p.x()))
        {
            event->accept();
            deletePage();
        }
        else if(getIcon("duplicate")->triggered(p.x()))
        {
            event->accept();
            duplicatePage();
        }
        else
        {
            event->ignore();
        }
        /*
        else if(movableUp() && getIcon("moveUp")->triggered(p.x()))
            moveUpPage();
        else if (movableDown() && getIcon("moveDown")->triggered(p.x()))
            moveDownPage();*/
    }
    else
    {
        event->ignore();
    }
}

void UBDraggableThumbnailItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
}


UBDraggableLivePixmapItem::UBDraggableLivePixmapItem(std::shared_ptr<UBGraphicsScene> pageScene, std::shared_ptr<UBDocumentProxy> documentProxy, int index, const QPixmap& thumbnail)
    : UBDraggableThumbnailItem(documentProxy, index)
    , mScene(pageScene)
    , mPageNumber(new UBThumbnailTextItem(index))
    , mExposed(false)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setAcceptDrops(true);
    setTransformationMode(Qt::SmoothTransformation);

    mSelectionItem = new QGraphicsRectItem(sceneBoundingRect().x() - sSelectionItemMargin,
                                           sceneBoundingRect().y() - sSelectionItemMargin,
                                           sceneBoundingRect().width() + 2*sSelectionItemMargin,
                                           sceneBoundingRect().height() + 2*sSelectionItemMargin);
    mSelectionItem->setPen(QPen(UBSettings::treeViewBackgroundColor, 8));
    mSelectionItem->setZValue(-1);

    setPixmap(thumbnail);
}

void UBDraggableLivePixmapItem::updatePos(qreal width, qreal height)
{
    QSizeF newSize(width, height);

    if (newSize != mSize && mScene)
    {
        mSize = newSize;
        createPixmap();
    }

    QFontMetrics fm(mPageNumber->font());
    int labelSpacing = UBSettings::thumbnailSpacing + fm.height();

    int w = pixmap().width();
    int h = pixmap().height();

    qreal scaledWidth = width / w;
    qreal scaledHeight = height / h;
    qreal scaledFactor = qMin(scaledWidth, scaledHeight);

    QTransform transform;
    transform.scale(scaledFactor, scaledFactor);

    // Apply the scaling
    setTransform(transform);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    // even spacing, all thumbnails have the same height
    QPointF position(2 * sSelectionItemMargin + (width - w * scaledFactor) / 2,
                     2 * sSelectionItemMargin + sceneIndex() * (height + labelSpacing) + (height - h * scaledFactor) / 2);

    setPos(position);

    mSelectionItem->setRect(sceneBoundingRect().x() - sSelectionItemMargin,
                            sceneBoundingRect().y() - sSelectionItemMargin,
                            sceneBoundingRect().width() + 2*sSelectionItemMargin,
                            sceneBoundingRect().height() + 2*sSelectionItemMargin);

    position.setY(sSelectionItemMargin + position.y() + (height + h * scaledFactor) / 2);
    position.setX(position.x() + (w * scaledFactor - fm.horizontalAdvance(mPageNumber->toPlainText())) / 2);

    mPageNumber->setPos(position);
}

void UBDraggableLivePixmapItem::setHighlighted(bool highlighted)
{
    if (highlighted)
    {
        mSelectionItem->show();
    }
    else
    {
        mSelectionItem->hide();
    }
}

void UBDraggableLivePixmapItem::setExposed(bool exposed)
{
    if (exposed != mExposed)
    {
        mExposed = exposed;

        if (exposed)
        {
            updatePixmap();
        }
    }
}

bool UBDraggableLivePixmapItem::isExposed()
{
    return mExposed;
}

void UBDraggableLivePixmapItem::updatePixmap(const QRectF &region)
{
    if (mScene && mSize.isValid() && mExposed)
    {
        QPixmap pixmap = this->pixmap();
        QRectF pixmapRect;
        const auto tool = UBDrawingController::drawingController()->stylusTool();
        const auto affectsWholeScene = tool == UBStylusTool::Play || tool == UBStylusTool::Selector;

        if (region.isNull() || affectsWholeScene)
        {
            // full update if region unknown or play/selector tool active, which may affect whole scene
            pixmapRect = pixmap.rect();
        }
        else
        {
            // only render affected region

            /*
             * To avoid problems with antialiaing we have to make sure that the
             * pixmapRect has an integer size and position and covers at least
             * the affected region. The following steps assure this.
             */
            pixmapRect = mTransform.mapRect(region);        // translate to pixmap coordinates
            pixmapRect += QMarginsF(1, 1, 1, 1);            // add a margin
            pixmapRect = pixmapRect.toRect();               // cut to integer
            pixmapRect &= pixmap.rect();                    // limit to pixmap area
        }

        if (pixmapRect.isValid())
        {
            QPainter painter(&pixmap);
            QRectF sceneRect = mTransform.inverted().mapRect(pixmapRect);
            mScene->render(&painter, pixmapRect, sceneRect);
            setPixmap(pixmap);
        }
    }
}

void UBDraggableLivePixmapItem::setScene(std::shared_ptr<UBGraphicsScene> scene)
{
    mScene = scene;
}

void UBDraggableLivePixmapItem::adjustThumbnail()
{
    // get the used area of the scene
    mSceneRect = mScene->normalizedSceneRect();

    // expand the scene rect so that it has the aspect ratio of the pixmap
    QSizeF pSize = pixmap().size();
    QPointF center = mSceneRect.center();
    QSizeF sceneSize = pSize.scaled(mSceneRect.size(), Qt::KeepAspectRatioByExpanding);
    mSceneRect.setSize(sceneSize);
    mSceneRect.moveCenter(center);

    // create a transformation from scene to pixmap coordinates
    QRectF pixRect{{0,0}, pSize};
    QPolygonF p1{mSceneRect};
    QPolygonF p2{pixRect};

    p1.removeLast();
    p2.removeLast();

    QTransform::quadToQuad(p1, p2, mTransform);

    // re-paint the pixmap
    updatePixmap();
}

void UBDraggableLivePixmapItem::createPixmap()
{
    if (mScene)
    {
        // always use a constant pixmap size
        // NOTE this code keeps the document pixmap incl its AR
        if (pixmap().width() != UBSettings::maxThumbnailWidth)
        {
            int height = UBSettings::maxThumbnailWidth * mSize.height() / mSize.width() + 0.5;
            QPixmap pixmap = QPixmap(UBSettings::maxThumbnailWidth, height);
            pixmap.fill(Qt::white);
            setPixmap(pixmap);
        }

        adjustThumbnail();
    }
}


UBThumbnailUI::UBThumbnailUIIcon* UBThumbnailUI::addIcon(const QString& thumbnailIcon, int pos)
{
    QString thumbnailIconPath = ":images/" + thumbnailIcon + ".svg";
    UBThumbnailUIIcon* newIcon = new UBThumbnailUIIcon(thumbnailIconPath, pos);

    using namespace UBThumbnailUI::_private;
    if (!newIcon)
        qDebug() << "cannot add Icon : check path : " + thumbnailIconPath;
    else
        catalog.insert(thumbnailIcon, newIcon);

    return newIcon;
}

UBThumbnailUI::UBThumbnailUIIcon* UBThumbnailUI::getIcon(const QString& thumbnailIcon)
{
    using namespace UBThumbnailUI::_private;
    if (!catalog.contains(thumbnailIcon))
        qDebug() << "cannot get Icon: check path ':images/" + thumbnailIcon + ".svg'";

    return catalog.value(thumbnailIcon, NULL);
}

void UBThumbnailUI::draw(QPainter *painter, const UBThumbnailUIIcon &thumbnailIcon)
{
    using namespace UBThumbnailUI;
    painter->drawPixmap(thumbnailIcon.pos() * (ICONSIZE + ICONSPACING), 0, ICONSIZE, ICONSIZE, thumbnailIcon);
}

void UBThumbnailUI::_private::initCatalog()
{
    using namespace UBThumbnailUI;
    using namespace UBThumbnailUI::_private;

    addIcon("close", 0);
    addIcon("closeDisabled", 0);

    addIcon("duplicate", 1);

    addIcon("moveUp", 2);
    addIcon("moveUpDisabled", 2);

    addIcon("moveDown", 3);
    addIcon("moveDownDisabled", 3);
}

bool UBThumbnailUI::triggered(qreal y)
{
    return (y >= 0 && y <= UBThumbnailUI::ICONSIZE);
}
