#ifndef PARKINGMAPPAGE_H
#define PARKINGMAPPAGE_H

#include "models/parkingstate.h"
#include "models/parkingzonelayout.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QColor;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsEllipseItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QGraphicsSimpleTextItem;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPoint;
class QPointF;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QSlider;
class QTableWidget;
class QTimer;

class ParkingMapPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParkingMapPage(const QString &layoutPath, QWidget *parent = nullptr);
    ~ParkingMapPage() override;
    void render(const ParkingViewState &state);
    bool hasUnsavedLayoutChanges() const { return m_layoutDirty; }
    bool saveLayoutNow(QString *errorMessage = nullptr);

signals:
    void layoutSaveResult(bool success, const QString &message);
    void layoutDirtyChanged(bool dirty);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void addGeneralZone();
    void addEvZone();
    void deleteSelectedZone();
    void saveLayout();
    void reloadLayout();
    void resetDefaultLayout();
    void resetSelectedZoneShape();
    void handleSceneSelectionChanged();
    void handleZoneTableClicked(int row, int column);
    void handleZoneItemMoved(const QString &zoneId);
    void applyEditorFields();
    void setEditMode(bool enabled);

private:
    void loadLayout();
    QString exampleLayoutPath() const;
    QString nextZoneId(const QString &prefix) const;
    QRectF nextZoneRectForChannel(const QString &channel) const;
    int zoneIndexById(const QString &zoneId) const;
    QString selectedZoneId() const;
    ParkingZoneLayout *selectedZone();
    const ParkingZoneLayout *selectedZone() const;
    bool stateForZone(const QString &zoneId, SlotState *state) const;
    SlotVisualState visualStateForZone(const QString &zoneId, bool *known = nullptr) const;
    void handleZoneItemDragStarted(const QString &zoneId);
    void showZoneContextMenu(const QString &zoneId, const QPoint &screenPos);
    void selectZoneById(const QString &zoneId);
    QString channelForScenePoint(const QPointF &point) const;
    QString nextIvaAreaForEv(const QString &channel, const QString &excludeZoneId = QString()) const;
    bool ivaUsedInChannel(const QString &channel, const QString &ivaAreaId, const QString &excludeZoneId) const;
    void applyZoneTypeRules(ParkingZoneLayout *zone, const QString &preferredIva = QString()) const;
    void rebuildScene();
    void updateZoneVisual(const QString &zoneId);
    void updateAllZoneVisuals();
    void updateAlarmAnimationState();
    void updateAlarmPulse();
    void updateEditorFromSelection();
    void updateZoneTable();
    void scrollMapToOrigin();
    void updateGeometrySliderLabels();
    void syncZonesFromItems();
    void syncItemsEditable();
    void markLayoutDirty(const QString &detail = QString());
    void setLayoutDirty(bool dirty, const QString &status = QString());
    QWidget *createLegendItem(const QString &label, const QColor &fill,
                              const QColor &border, bool circular = false);

    QString m_layoutPath;
    QList<ParkingZoneLayout> m_zones;
    ParkingViewState m_lastState;
    bool m_editMode = false;
    bool m_updatingEditor = false;
    bool m_rebuildingScene = false;
    bool m_layoutDirty = false;

    QGraphicsScene *m_scene = nullptr;
    QGraphicsView *m_mapView = nullptr;
    QHash<QString, QGraphicsRectItem *> m_zoneItems;
    QHash<QString, QGraphicsRectItem *> m_zoneAccentBars;
    QHash<QString, QGraphicsSimpleTextItem *> m_zoneLabels;
    QHash<QString, QGraphicsSimpleTextItem *> m_zoneStateLabels;
    QHash<QString, QGraphicsSimpleTextItem *> m_zoneMetaLabels;
    QHash<QString, QGraphicsEllipseItem *> m_zoneAlarmHalos;
    QHash<QString, QGraphicsPathItem *> m_zoneAlarmBeacons;
    QHash<QString, QGraphicsSimpleTextItem *> m_zoneAlarmLabels;
    QTableWidget *m_zoneTable = nullptr;
    QTimer *m_alarmPulseTimer = nullptr;
    double m_alarmPulsePhase = 0.0;

    QLabel *m_selectedTitleLabel = nullptr;
    QLabel *m_selectedMetaLabel = nullptr;
    QLabel *m_selectedStateLabel = nullptr;
    QPushButton *m_editToggleButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QLineEdit *m_zoneIdEdit = nullptr;
    QLineEdit *m_displayNameEdit = nullptr;
    QComboBox *m_zoneTypeCombo = nullptr;
    QComboBox *m_cameraChannelCombo = nullptr;
    QComboBox *m_ivaAreaCombo = nullptr;
    QLineEdit *m_hallSensorEdit = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QDoubleSpinBox *m_xSpin = nullptr;
    QDoubleSpinBox *m_ySpin = nullptr;
    QSlider *m_widthSlider = nullptr;
    QSlider *m_heightSlider = nullptr;
    QSlider *m_rotationSlider = nullptr;
    QLabel *m_widthValueLabel = nullptr;
    QLabel *m_heightValueLabel = nullptr;
    QLabel *m_rotationValueLabel = nullptr;
    QLabel *m_layoutStatusLabel = nullptr;
};

#endif
