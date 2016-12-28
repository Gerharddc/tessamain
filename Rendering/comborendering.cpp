#include "comborendering.h"

#include <QDebug>
#include <QFile>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/intersect.hpp>
#include <algorithm>
#include <fstream>
#include <thread>

#include "stlimporting.h"
#include "stlexporting.h"
#include "gcodeimporting.h"
#include "Misc/globalsettings.h"
#include "ChopperEngine/chopperengine.h"

// TODO: make relative to bed size
static const float DefaultZoom = 3.0f;

static float viewWidth = 0.0f;
static float viewHeight = 0.0f;

static float centreX = 0.0f;
static float centreY = 0.0f;

static float aimX = 0.0f;
static float aimY = 0.0f;
static float zoom = DefaultZoom;

static glm::mat4 sceneTrans = glm::mat4();
static glm::mat4 sceneProj = glm::mat4();

static glm::mat4 rotOrg = glm::mat4();
static glm::mat4 rotOrgInv = glm::mat4();
static glm::quat sceneRot = glm::quat();

static std::set<Mesh*> stlMeshes;
static std::set<Mesh*> selectedMeshes;

static Toolpath *gcodePath = nullptr;

static bool curMeshesSaved = false;
static std::string curMeshesPath = "";

// These are used to delegate an opengl update call
static ComboRendering::UpdateHandler updateHandler;
static void *updateContext = nullptr;

static void UpdateGridHandler(void*, float)
{
    GridRendering::GridDirty();
    ComboRendering::ResetView(true);
}

static void RecalculateCentre()
{
    // We need to translate the centre of the box to the origin for rotation
    float midX = GlobalSettings::BedWidth.Get() / 2.0f;
    float midY = GlobalSettings::BedLength.Get() / 2.0f;

    rotOrg = glm::translate(glm::mat4(), glm::vec3(midX, midY, 0.0f));
    rotOrgInv = glm::translate(glm::mat4(), glm::vec3(-midX, -midY, 0.0f));
}

static void UpdateProjection()
{
    // Calculate an orthographic projection that centres the view at the aiming position and applies the zoom
    float left = aimX - (centreX / zoom);
    float right = aimX + (centreX / zoom);
    float bottom = aimY - (centreY / zoom);
    float top = aimY + (centreY / zoom);

    // With the orthographic system we need a negative and positive clip plane of enough distance
    sceneProj = glm::ortho(left, right, bottom, top, -GlobalSettings::BedHeight.Get() * 10,
                           GlobalSettings::BedHeight.Get() * 10);

    // Flag the renderers to update their proj matrices
    GridRendering::ProjMatDirty();
    STLRendering::ProjMatDirty();
    ToolpathRendering::ProjMatDirty();
}

static void RecalculateView()
{
    // Update the projection
    UpdateProjection();

    // Reset the scene view looking at the centre of the box from a height above the origin
    RecalculateCentre();
    sceneTrans = rotOrg;
    sceneRot = glm::angleAxis(glm::radians(180.0f + 45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat inv = sceneRot;
    inv.w *= -1; // Invert the angle
    glm::vec4 xDir(1.0f, 0.0f, 0.0f, 1.0f);
    xDir = inv * xDir;
    sceneRot *= glm::angleAxis(glm::radians(45.0f), glm::vec3(xDir.x, xDir.y, 0.0f));
    sceneTrans *= glm::mat4_cast(sceneRot);
    sceneTrans *= rotOrgInv;
}

static void removeCharsFromString(std::string &str, const char* charsToRemove ) {
   for ( unsigned int i = 0; i < strlen(charsToRemove); ++i ) {
      str.erase( std::remove(str.begin(), str.end(), charsToRemove[i]), str.end() );
   }
}

void ComboRendering::SetUpdateHandler(ComboRendering::UpdateHandler handler, void *context)
{
    updateHandler = handler;
    updateContext = context;
}

void ComboRendering::Update()
{
    if (updateContext != nullptr)
        updateHandler(updateContext);
}

void ComboRendering::FreeMemory()
{
    for (Mesh *mesh : stlMeshes)
    {
        if (mesh != nullptr)
        {
            delete mesh;
            mesh = nullptr;
        }
    }

    if (gcodePath != nullptr)
    {
        delete gcodePath;
        gcodePath = nullptr;
    }

    ToolpathRendering::FreeMemory();
    GridRendering::FreeMemory();
    STLRendering::FreeMemory();
}

int ComboRendering::getMeshCount()
{
    return stlMeshes.size();
}

void ComboRendering::LoadMesh(std::string path)
{
    auto mesh = STLImporting::ImportSTL(path.c_str());
    stlMeshes.insert(mesh);
    STLRendering::AddMesh(mesh);
    curMeshesSaved = false;

    // Call OpenGL upate
    Update();
}

void ComboRendering::LoadToolpath(std::string path)
{
    if (gcodePath != nullptr)
        delete gcodePath;

    gcodePath = GCodeImporting::ImportGCode(path.c_str());
    ToolpathRendering::SetToolpath(gcodePath);

    // Call OpenGL upate
    Update();
}

std::string ComboRendering::SaveMeshes(std::string fileName)
{
    // Save the meshes asynchronously because it can take some time
    // TODO: tell the user that we are busy
    // TODO: implement error reporting on this
    // Remove illegal characters
    std::string fN(fileName);
    removeCharsFromString(fN, "./\\|"); // TODO: complete this list
    const std::string saveDir = "/home/Simon/Saved/";
    std::string savePath = saveDir + fN + ".stl";
    std::string error = "";

    // Copy if already saved
    QString src = QString::fromStdString(curMeshesPath);
    QString dest = QString::fromStdString(savePath);
    if (!STLRendering::PrepMeshesSave(stlMeshes) && curMeshesSaved && QFile::exists(src))
    {
        // TODO: do not use Qt here
        if (curMeshesPath != savePath)
        {
            if (QFile::exists(dest))
            {
                QFile::remove(dest);
            }

            QFile::copy(src, dest);
        }
    }
    else
    {
        STLExporting::ExportSTL(savePath, stlMeshes, error);
    }

    if (error == "")
    {
        curMeshesSaved = true;
        curMeshesPath = savePath;
        return savePath;
    }

    return error;
}

std::string ComboRendering::SliceMeshes(std::string fileName)
{
    //SaveMeshes(fileName);
    // TODO: implement slice
    ChopperEngine::SliceFile(stlMeshes.begin().operator *(), fileName);

    return "";
}

void ComboRendering::RemoveMesh(Mesh *mesh)
{
    STLRendering::RemoveMesh(mesh);
    stlMeshes.erase(mesh);
    selectedMeshes.erase(mesh);
    delete mesh;
    curMeshesSaved = false;

    // Call OpenGL upate
    Update();
}

void ComboRendering::SetViewSize(float width, float height)
{
    // Update the viewport variables
    viewWidth = width;
    viewHeight = height;
    centreX = width / 2.0f;
    centreY = height / 2.0f;

    RecalculateView();
}

void ComboRendering::ApplyRot(float x, float y)
{
    // Apply the transformation asynchronously because it can take some time
    // TODO: tell the user that we are busy
    std::thread([=]() {
        // We need to transform the rotation axis into screen space by applying the inverse of the rotation
        // currently being applied to our scene

        // We also need to scale the roation to the screen size and aspect ratio
        // TODO: add dpi scaling
        // For some reason the x rot is in the inverse direction of the mouse movement
        // TODO: 3.0 ?
        float yAng = (x / GlobalSettings::BedWidth.Get() * 1.1) / 3.0;
        float xAng = -(y / GlobalSettings::BedLength.Get() *
                       (1.1 * GlobalSettings::BedLength.Get() / GlobalSettings::BedWidth.Get())) / 3.0;
        glm::vec2 axis = glm::vec2(xAng, yAng);
        auto l = glm::length(axis); // doesnt work
        if (l == 0) // This is important to avoid normaliztion disasters ahead
            return;

        axis = glm::normalize(axis);
        glm::vec4 dir = glm::vec4(axis.x, axis.y, 0.0f, 0.0f);
        glm::quat inv = sceneRot;
        inv.w *= -1;
        dir = inv * dir;
        dir = glm::normalize(dir);

        // Now we need to rotate accordingly around the calculated centre
        sceneTrans = rotOrg;
        sceneRot *= glm::angleAxis(l, glm::vec3(dir.x, dir.y, dir.z));
        sceneTrans *= glm::mat4_cast(sceneRot);
        sceneTrans *= rotOrgInv;

        // Flag the renderers to update their scene matrices
        GridRendering::SceneMatDirty();
        STLRendering::SceneMatDirty();
        ToolpathRendering::SceneMatDirty();

        // Call OpenGL upate
        Update();
    }).detach();
}

void ComboRendering::Move(float x, float y)
{
    // Apply the transformation asynchronously because it can take some time
    // TODO: tell the user that we are busy
    std::thread([=]() {
        // The direction of mouse movement is inverse of the view movement
        // We need to scale the move distance to the size of the viewport
        // We want a constant move distance for a viewport length and take
        // aspect ration into account
        // TODO: add dpi support
        aimX -= (x / GlobalSettings::BedWidth.Get() * 65) / zoom;
        aimY -= (y / GlobalSettings::BedLength.Get()
                 * (65 * GlobalSettings::BedLength.Get() / GlobalSettings::BedWidth.Get())) / zoom;

        UpdateProjection();

        // Call OpenGL upate
        Update();
    }).detach();
}

void ComboRendering::Zoom(float scale)
{
    // Apply the transformation asynchronously because it can take some time
    // TODO: tell the user that we are busy
    std::thread([=]() {
        zoom *= scale;
        UpdateProjection();

        // Call OpenGL upate
        Update();
    }).detach();
}

void ComboRendering::ResetView(bool updateNow)
{
    // Apply the transformation asynchronously because it can take some time
    // TODO: tell the user that we are busy
    std::thread([=]() {
        zoom = DefaultZoom;

        aimX = (GlobalSettings::BedWidth.Get() / 2.0f);
        aimY = (GlobalSettings::BedLength.Get() / 2.0f);
        RecalculateView();

        GridRendering::SceneMatDirty();
        STLRendering::SceneMatDirty();
        ToolpathRendering::SceneMatDirty();

        // Call OpenGL upate
        if (updateNow)
            Update();

        // Call OpenGL upate
        Update();
    }).detach();
}

void ComboRendering::Init()
{
    GridRendering::GridDirty();

    GridRendering::Init();
    STLRendering::Init();
    ToolpathRendering::Init();

    //gcodePath = GCodeImporting::ImportGCode("/home/Simon/Saved/Untitled.gcode");//"test.gcode");
    //ToolpathRendering::SetToolpath(gcodePath);

    // Setup event handler for settings changes
    GlobalSettings::BedHeight.RegisterHandler(UpdateGridHandler, nullptr);
    GlobalSettings::BedWidth.RegisterHandler(UpdateGridHandler, nullptr);
    GlobalSettings::BedLength.RegisterHandler(UpdateGridHandler, nullptr);

    // Reset the view
    ResetView(true);
}

void ComboRendering::Draw()
{
    glViewport(0, 0, viewWidth, viewHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GridRendering::Draw();

    if (STLRendering::GetBaseOpacity() != 0.0f)
        STLRendering::Draw();

    if (ToolpathRendering::GetOpacity() != 0.0f)
        ToolpathRendering::Draw();
}

void ComboRendering::TestMouseIntersection(float x, float y)
{
    // TODO: this should only run if the STLs are visible

    // Normalized screen coordinates
    float screenX = (2.0f * x) / viewWidth - 1.0f;
    float screenY = (2.0 * y) / viewHeight - 1.0f;

    // Calculate the farthest and closest points from a line caused by the cursor
    // TODO: cache some of this shit
    glm::mat4 MV = sceneProj * sceneTrans;
    glm::mat4 invMat = glm::inverse(MV);
    glm::vec4 clipCoords = glm::vec4(screenX, screenY, -1.0f, 1.0f);
    glm::vec4 worldCoords = invMat * clipCoords;
    glm::vec3 near = glm::vec3(worldCoords.x, worldCoords.y, worldCoords.z);
    clipCoords.z = 1.0f;
    worldCoords = invMat * clipCoords;
    glm::vec3 far = glm::vec3(worldCoords.x, worldCoords.y, worldCoords.z);

    Mesh *highestMesh = nullptr;
    float z = 2.0f;

    // Find the mesh with the 'highest' intersection
    // (the most negative z value)
    for (Mesh *mesh : stlMeshes)
    {
        float nZ;
        if (STLRendering::TestMeshIntersection(mesh, near, far, MV, nZ))
        {
            if (nZ < z)
            {
                highestMesh = mesh;
                z = nZ;
            }
        }
    }

    if (highestMesh != nullptr)
    {
        // (de)select the mesh
        if (selectedMeshes.count(highestMesh) != 0)
        {
            // Deslect
            selectedMeshes.erase(highestMesh);
            STLRendering::ColorMesh(highestMesh, normalMeshCol);
        }
        else
        {
            // Select
            selectedMeshes.insert(highestMesh);
            STLRendering::ColorMesh(highestMesh, selectedMeshCol);
        }

        // Call OpenGL upate
        Update();
    }
}

const std::set<Mesh*> &ComboRendering::getSelectedMeshes()
{
    return selectedMeshes;
}

const glm::mat4 &ComboRendering::getSceneTrans()
{
    return sceneTrans;
}

const glm::mat4 &ComboRendering::getSceneProj()
{
    return sceneProj;
}

const Toolpath *ComboRendering::getToolpath()
{
    return gcodePath;
}
