#include "fborenderer.h"

#include <QOpenGLFramebufferObject>

#ifndef GLES
#include "loadedgl.h"
#endif

#include "stlrendering.h"
#include "toolpathrendering.h"
#include "gridrendering.h"
#include "Misc/globalsettings.h"
#include "Printer/printer.h"
#include <QObject>
#include <iostream>
#include <thread>

class ComboFBORenderer : public QQuickFramebufferObject::Renderer
{
public:

    ComboFBORenderer()
    {
#ifndef GLES
        LoadedGL::ActivateGL();
#endif
        ComboRendering::Init();
#ifndef GLES
        LoadedGL::DeactivateGL();
#endif
    }

    ~ComboFBORenderer()
    {
#ifndef GLES
        LoadedGL::ActivateGL();
#endif
        ComboRendering::FreeMemory();
#ifndef GLES
        LoadedGL::DeactivateGL();
#endif
    }

    void render() {
#ifndef GLES
        LoadedGL::ActivateGL();
#endif
        ComboRendering::Draw();
#ifndef GLES
        LoadedGL::DeactivateGL();
#endif
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(4);
        ComboRendering::SetViewSize(size.width(), size.height());

        return new QOpenGLFramebufferObject(size, format);
    }
};

QQuickFramebufferObject::Renderer *FBORenderer::createRenderer() const
{
    auto ren = new ComboFBORenderer();
    return ren;
}

void CallFBOUpdate(void *context)
{
    // Invoke the update slot through the message queue to make it thread safe
    QMetaObject::invokeMethod((FBORenderer*)context, "update");
}

FBORenderer::FBORenderer()
{
    //sliceProcess = new QProcess();
    //QObject::connect(sliceProcess, SIGNAL(readyReadStandardError()), this, SLOT(ReadSlicerOutput()));
    //QObject::connect(sliceProcess, SIGNAL(finished(int)), this, SLOT(SlicerFinsihed(int)));
    ComboRendering::SetUpdateHandler(CallFBOUpdate, this);
}

FBORenderer::~FBORenderer()
{
    //delete sliceProcess;
}

void FBORenderer::rotateView(float x, float y)
{
    ComboRendering::ApplyRot(x, y);
}

void FBORenderer::panView(float x, float y)
{
    ComboRendering::Move(x, y);
}

void FBORenderer::zoomView(float scale)
{
    ComboRendering::Zoom(scale);
}

void FBORenderer::resetView(bool updateNow)
{
    ComboRendering::ResetView(updateNow);
}

void FBORenderer::autoArrangeMeshes()
{
    STLRendering::PackMeshes();
    EmitMeshProps();
}

QString FBORenderer::saveMeshes()
{
    // TODO: saving is done async; implement error reporting
    std::thread([=]() {
        ComboRendering::SaveMeshes(saveName().toStdString());
    }).detach();
    return "";
}

// This is a helper method used to refresh all the properties
// relating to the current mesh
void FBORenderer::EmitMeshProps()
{
    emit curMeshPosChanged();
    emit curMeshScaleChanged();
    emit curMeshRotChanged();
}

void FBORenderer::loadMesh(QString path)
{
    std::thread([=]() {
        ComboRendering::LoadMesh(path.toStdString());

        EmitMeshProps();
        emit meshCountChanged();
    }).detach();
}

int FBORenderer::meshCount()
{
    return ComboRendering::getMeshCount();
}

void FBORenderer::testMouseIntersection(float x, float y)
{
    // Test for mouse intersection with objects and alert the gui
    // if we have moved between a point of no or any mesh selection

    std::thread([=]() {
        int old = meshesSelected();
        ComboRendering::TestMouseIntersection(x, y);

        if (meshesSelected() != old)
            emit meshesSelectedChanged();

        if (old != 1 && meshesSelected() == 1)
            EmitMeshProps();
    }).detach();
}

void FBORenderer::setMeshOpacity(float o)
{
    if (o != STLRendering::GetBaseOpacity())
    {
        STLRendering::SetBaseOpacity(o);

        emit meshOpacityChanged();
    }
}

void FBORenderer::setTpOpacity(float o)
{
    if (o != ToolpathRendering::GetOpacity())
    {
        ToolpathRendering::SetOpacity(o);

        emit tpOpacityChanged();
    }
}

QPointF FBORenderer::curMeshPos()
{
    if (meshesSelected() == 1)
    {
        auto &v = STLRendering::getMeshData(*ComboRendering::getSelectedMeshes().begin()).moveOnMat;
        return QPointF(v.x, v.y);
    }
    else
        return QPointF(0.0f, 0.0f);
}

void FBORenderer::setCurMeshPos(QPointF pos)
{
    if (meshesSelected() == 1)
    {
        auto old = curMeshPos();

        // Filter out some noise
        if (std::abs((pos - old).manhattanLength()) >= 0.05f)
        {
            STLRendering::CentreMesh(*ComboRendering::getSelectedMeshes().begin(), pos.x(), pos.y());
            emit curMeshPosChanged();
        }
    }
}

QVector3D FBORenderer::curMeshRot()
{
    if (meshesSelected() == 1)
    {
        auto &v = STLRendering::getMeshData(*ComboRendering::getSelectedMeshes().begin()).rotOnMat;
        return QVector3D(glm::degrees(v.x), glm::degrees(v.y), glm::degrees(v.z));
    }
    else
        return QVector3D(0.0f, 0.0f, 0.0f);
}

void FBORenderer::setCurMeshRot(QVector3D rot)
{
    if (meshesSelected() == 1)
    {
        QVector3D old = curMeshRot();

        // Filter out some noise
        if (std::abs((rot - old).lengthSquared()) >= 0.05f)
        {
            STLRendering::RotateMesh(*ComboRendering::getSelectedMeshes().begin(), rot.x(), rot.y(), rot.z());
            emit curMeshRotChanged();
        }
    }
}

float FBORenderer::curMeshLift()
{
    if (meshesSelected() == 1)
    {
        auto &v = STLRendering::getMeshData(*ComboRendering::getSelectedMeshes().begin()).moveOnMat;
        return v.z;
    }
    else
        return 0.0f;
}

void FBORenderer::setCurMeshLift(float lift)
{
    if (meshesSelected() == 1)
    {
        float old = curMeshLift();

        // Filter out noise
        if (std::abs(lift - old) >= 0.1)
        {
            STLRendering::LiftMesh(*ComboRendering::getSelectedMeshes().begin(), lift);
            emit curMeshLiftChanged();
        }
    }
}

float FBORenderer::curMeshScale()
{
    if (meshesSelected() == 1)
        return STLRendering::getMeshData(*ComboRendering::getSelectedMeshes().begin()).scaleOnMat;
    else
        return 0.0f;
}

void FBORenderer::setCurMeshScale(float scale)
{
    if (meshesSelected() == 1)
    {
        float old = curMeshScale();

        // Filter out noise
        if (std::abs(scale - old) >= 0.005f)
        {
            STLRendering::ScaleMesh(*ComboRendering::getSelectedMeshes().begin(), scale);
            emit curMeshScaleChanged();
        }
    }
}

void FBORenderer::setSaveName(QString sav)
{
    if (sav != m_saveName)
    {
        if (sav == "")
            sav = "Untitled";

        sav.remove("./|\\~"); // TODO: complete list

        m_saveName = sav;
        emit saveNameChanged();
    }
}

QString FBORenderer::sliceMeshes()
{
    if (m_slicerRunning)
    {
        m_slicerRunning = false;
        m_slicerStatus = "Stopped";
        emit slicerRunningChanged();
        emit slicerStatusChanged();
        return "stopped";
    }
    else if (meshCount() == 0)
        return "no meshes";

    m_slicerRunning = true;
    m_slicerStatus = "Running";
    emit slicerRunningChanged();
    emit slicerStatusChanged();

    // We wait async for the mesh that is being saved async and then start the slicer
    std::thread([](FBORenderer *fbo) {
        //QString stlName = QString::fromStdString(ComboRendering::SaveMeshes(fbo->saveName().toStdString()));
        QString stlName = fbo->saveName() + ".stl";
        fbo->gcodePath = QString(stlName);
        fbo->gcodePath.replace(".stl", ".gcode");

        /*QStringList arguments;
        //arguments << "slice" << "-v";
        //arguments << "-j" << "/home/Simon/.Cura/simon.json";
        //arguments << "-j" << "fdmprinter.json";
        arguments << "-j" << "/home/Simon/.Cura/fdmp.json";
        arguments << "-v" << "-p";
        arguments << "-o" << fbo->gcodePath;
        arguments << "-s" << "infill_sparse_density=" + QString::number(GlobalSettings::InfillDensity.Get());
        arguments << "-s" << "layer_height=" + QString::number(GlobalSettings::LayerHeight.Get());
        arguments << "-s" << "skirt_line_count=" + QString::number(GlobalSettings::SkirtLineCount.Get());
        arguments << "-s" << "skirt_gap=" + QString::number(GlobalSettings::SkirtDistance.Get());
        arguments << "-s" << "speed_print=" + QString::number(GlobalSettings::PrintSpeed.Get());
        arguments << "-s" << "speed_infill=" + QString::number(GlobalSettings::InfillSpeed.Get());
        arguments << "-s" << "speed_topbottom=" + QString::number(GlobalSettings::TopBottomSpeed.Get());
        arguments << "-s" << "speed_travel=" + QString::number(GlobalSettings::TravelSpeed.Get());
        arguments << "-s" << "speed_layer_0=" + QString::number(GlobalSettings::FirstLineSpeed.Get());
        arguments << "-s" << "retraction_speed=" + QString::number(GlobalSettings::RetractionSpeed.Get());
        arguments << "-s" << "retraction_amount=" + QString::number(GlobalSettings::RetractionDistance.Get());
        arguments << "-s" << "shell_thickness=" + QString::number(GlobalSettings::ShellThickness.Get());
        arguments << "-s" << "top_bottom_thickness=" + QString::number(GlobalSettings::TopBottomThickness.Get());
        arguments << "-s" << "material_print_temperature=" + QString::number(GlobalSettings::PrintTemperature.Get());
        arguments << "-s" << "support_enable=false";
        //arguments << "-l" << stlName;
        arguments << stlName;

        // Start the slicer through the message queue (thread safe)
        QMetaObject::invokeMethod(fbo, "StartSliceThread", Q_ARG(QStringList, arguments));*/

        ComboRendering::SliceMeshes(fbo->gcodePath.toStdString());

        QMetaObject::invokeMethod(fbo, "SlicerFinsihed", Q_ARG(int, 1));
    }, this).detach();

    return "started";
}

QString FBORenderer::printToolpath()
{
    if (!toolPathLoaded())
        return "no tp loaded";

    GlobalPrinter.startPrint(gcodePath);
    return "started";
}

void FBORenderer::ReadSlicerOutput()
{
    /*QStringList sl = QString(sliceProcess->readAllStandardError()).split("\n");

    //for (QString str : sl)
      //  std::cout << str.toStdString() << std::endl;

    if (sl.last() == "")
        sl.removeLast(); // The last one is usually empty
    if (sl.count() > 0)
        m_slicerStatus = sl.back();
        emit slicerStatusChanged();*/
}

void FBORenderer::SlicerFinsihed(int)
{
    m_slicerRunning = false;
    m_slicerStatus = "Finished";
    emit slicerRunningChanged();
    emit slicerStatusChanged();

    // Load the toolpath asynchronously because it can take some time
    // TODO: tell the user that we are busy
    std::thread([=]() {
        ComboRendering::LoadToolpath(gcodePath.toStdString());
        emit toolPathLoadedChanged();
    }).detach();
}

void FBORenderer::StartSliceThread(QStringList arguments)
{
    const QString program = "CuraEngine";

    //sliceProcess->start(program, arguments);
}

void FBORenderer::removeSelectedMeshes()
{
    std::thread([=]() {
        for (Mesh *mesh : std::set<Mesh*>(ComboRendering::getSelectedMeshes()))
            ComboRendering::RemoveMesh(mesh);

        emit meshesSelectedChanged();
        emit meshCountChanged();
        EmitMeshProps();
    }).detach();
}
