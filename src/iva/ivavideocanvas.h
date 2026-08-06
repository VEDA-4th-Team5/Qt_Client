#pragma once

#include "ivaareamodels.h"

#include <QGraphicsView>

class QGraphicsPixmapItem;
class QGraphicsPolygonItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QImage;
class QMouseEvent;
class QResizeEvent;

class IvaVideoCanvas : public QGraphicsView
{
    Q_OBJECT

public:
    explicit IvaVideoCanvas(QWidget *parent = nullptr);

    void setChannel(int channel, const QSize &coordinateResolution);
    void setFrame(const QImage &frame);
    void setAreas(const QList<IvaAreaDefinition> &areas);
    void setSelectedAreaIndex(int areaIndex);
    void setDrawMode(bool enabled);
    bool drawMode() const { return m_drawMode; }
    bool frameCompatible() const { return m_frameCompatible; }
    QString frameCompatibilityMessage() const { return m_frameCompatibilityMessage; }

    static QList<QPointF> rectangleCoordinates(const QRectF &rectangle);

signals:
    void rectangleDrafted(const QRectF &sourceRectangle);
    void areaSelected(int areaIndex);
    void frameCompatibilityChanged(bool compatible, const QString &message);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointF boundedScenePoint(const QPoint &viewportPoint) const;
    void rebuildOverlays();
    void updateOverlayStyles();
    void fitScene();

    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_frameItem = nullptr;
    QList<QGraphicsPolygonItem *> m_areaItems;
    QGraphicsRectItem *m_draftItem = nullptr;
    QList<IvaAreaDefinition> m_areas;
    QSize m_coordinateResolution;
    QPointF m_dragStart;
    int m_channel = -1;
    int m_selectedAreaIndex = -1;
    bool m_drawMode = false;
    bool m_dragging = false;
    bool m_frameCompatible = false;
    QString m_frameCompatibilityMessage;
};
