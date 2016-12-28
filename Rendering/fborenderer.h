#ifndef FBORENDERER_H
#define FBORENDERER_H

#include <QQuickFramebufferObject>
#include <QString>
#include <QVector3D>
#include <QProcess>
#include <QStringList>

#include "structures.h"
#include "comborendering.h"

class FBORenderer : public QQuickFramebufferObject
{
    Q_OBJECT

private:
    QString m_saveName = "Untitled";
    bool m_slicerRunning = false;
    QString m_slicerStatus = "Not running";
    //QProcess *sliceProcess;
    QString gcodePath = "";
    void EmitMeshProps();

public:
    FBORenderer();
    ~FBORenderer();

    Renderer *createRenderer() const;
    Q_INVOKABLE void rotateView(float x, float y);
    Q_INVOKABLE void panView(float x, float y);
    Q_INVOKABLE void zoomView(float scale);
    Q_INVOKABLE void resetView(bool updateNow = true);
    Q_INVOKABLE void loadMesh(QString path);
    Q_INVOKABLE void testMouseIntersection(float x, float y);
    Q_INVOKABLE void removeSelectedMeshes();
    Q_INVOKABLE void autoArrangeMeshes();
    Q_INVOKABLE QString saveMeshes();
    Q_INVOKABLE QString sliceMeshes();
    Q_INVOKABLE QString printToolpath();

    Q_PROPERTY(float meshOpacity READ meshOpacity WRITE setMeshOpacity NOTIFY meshOpacityChanged)
    bool meshOpacity() { return STLRendering::GetBaseOpacity(); }
    void setMeshOpacity(float o);

    Q_PROPERTY(float tpOpacity READ tpOpacity WRITE setTpOpacity NOTIFY tpOpacityChanged)
    bool tpOpacity() { return ToolpathRendering::GetOpacity(); }
    void setTpOpacity(float o);

    Q_PROPERTY(int meshesSelected READ meshesSelected NOTIFY meshesSelectedChanged)
    int meshesSelected() { return ComboRendering::getSelectedMeshes().size(); }

    Q_PROPERTY(QPointF curMeshPos READ curMeshPos WRITE setCurMeshPos NOTIFY curMeshPosChanged)
    QPointF curMeshPos();
    void setCurMeshPos(QPointF pos);

    Q_PROPERTY(float curMeshLift READ curMeshLift WRITE setCurMeshLift NOTIFY curMeshLiftChanged)
    float curMeshLift();
    void setCurMeshLift(float lift);

    Q_PROPERTY(float curMeshScale READ curMeshScale WRITE setCurMeshScale NOTIFY curMeshScaleChanged)
    float curMeshScale();
    void setCurMeshScale(float scale);

    Q_PROPERTY(QVector3D curMeshRot READ curMeshRot WRITE setCurMeshRot NOTIFY curMeshRotChanged)
    QVector3D curMeshRot();
    void setCurMeshRot(QVector3D rot);

    Q_PROPERTY(int meshCount READ meshCount NOTIFY meshCountChanged)
    int meshCount();

    Q_PROPERTY(bool toolPathLoaded READ toolPathLoaded NOTIFY toolPathLoadedChanged)
    bool toolPathLoaded() { return ToolpathRendering::ToolpathLoaded(); }

    Q_PROPERTY(QString saveName READ saveName WRITE setSaveName NOTIFY saveNameChanged)
    QString saveName() { return m_saveName; }
    void setSaveName(QString sav);

    Q_PROPERTY(bool slicerRunning READ slicerRunning NOTIFY slicerRunningChanged)
    bool slicerRunning() { return m_slicerRunning; }

    Q_PROPERTY(QString slicerStatus READ slicerStatus NOTIFY slicerStatusChanged)
    QString slicerStatus() { return m_slicerStatus; }

public slots:
    void ReadSlicerOutput();
    void SlicerFinsihed(int);
    void StartSliceThread(QStringList arguments);

signals:
   void meshOpacityChanged();
   void tpOpacityChanged();
   void meshesSelectedChanged();
   void curMeshPosChanged();
   void curMeshLiftChanged();
   void curMeshScaleChanged();
   void curMeshRotChanged();
   void meshCountChanged();
   void saveNameChanged();
   void slicerRunningChanged();
   void slicerStatusChanged();
   void toolPathLoadedChanged();
};

#endif // FBORENDERER_H
