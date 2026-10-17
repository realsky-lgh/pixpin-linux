#pragma once

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "AnnotationLayer.h"

enum class SelectionShape {
    Rectangle,
    Ellipse
};

class ToolBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolBarWidget(QWidget *parent = nullptr);

    void resetTools();
    void updatePosition(const QRect &targetRect, const QRect &boundaryRect);
    void setSelectionShape(SelectionShape shape);
    SelectionShape selectionShape() const { return m_currentShape; }

signals:
    void toolSelected(ToolType tool);
    void selectionShapeChanged(SelectionShape shape);
    void colorChanged(const QColor &color);
    void strokeWidthChanged(int width);
    void undoTriggered();
    void pinTriggered();
    void ocrTriggered();
    void copyTriggered();
    void saveTriggered();
    void closeTriggered();

private:
    void setupUI();
    void setupSubBar();
    void updateShapeButton();
    QPushButton *createToolButton(const QString &text, const QString &tooltip, bool checkable = false);

    SelectionShape m_currentShape = SelectionShape::Rectangle;
    QWidget *m_mainBar = nullptr;
    QWidget *m_subBar = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QButtonGroup *m_toolGroup = nullptr;

    QPushButton *m_btnShape = nullptr;
    QPushButton *m_btnPin = nullptr;
    QPushButton *m_btnOcr = nullptr;
    QPushButton *m_btnRect = nullptr;
    QPushButton *m_btnEllipse = nullptr;
    QPushButton *m_btnArrow = nullptr;
    QPushButton *m_btnPen = nullptr;
    QPushButton *m_btnStep = nullptr;
    QPushButton *m_btnMosaic = nullptr;
    QPushButton *m_btnText = nullptr;
    QPushButton *m_btnUndo = nullptr;
    QPushButton *m_btnCopy = nullptr;
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnClose = nullptr;

    QRect m_lastTargetRect;
    QRect m_lastBoundaryRect;
};
