#include "stlrendering.h"

#include <QDebug>

#include <string>
#include <queue>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>

#include "glhelper.h"
#include "mathhelper.h"
#include "stlimporting.h"
#include "comborendering.h"
#include "Misc/globalsettings.h"

static GLuint mProgram = 0;
static GLint mPositionAttribLocation = 0;
static GLint mNormalAttribLocation = 0;

static GLint mModelUniformLocation = 0;
static GLint mProjUniformLocation = 0;
static GLint mNormUniformLocation = 0;
static GLint mColorUniformLocation = 0;

static std::map<Mesh*, MeshGroupData*> meshGroups;
static std::queue<MeshGroupData*> toDelete;

// We need flags to determine when matrices have changed as
// to be able to give new ones to opengl
static bool dirtyProjMat = true;
static bool dirtySceneMat = true;
static bool dirtyMesh = false;

static void LoadMesh(MeshGroupData &mg, Mesh *mesh)
{
    glGenBuffers(1, &mg.mVertexPositionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mg.mVertexPositionBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh->trigCount * 9, mesh->getFlatVerts(), GL_STATIC_DRAW);
    mesh->dumpFlatVerts();

    glGenBuffers(1, &mg.mVertexNormalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mg.mVertexNormalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh->trigCount * 9, mesh->getFlatNorms(), GL_STATIC_DRAW);
    mesh->dumpFlatNorms();

    mg.meshDirty = false;
}

// This updates the Mesh in normal memory for saving but does not
// affect the mesh loaded in GPU memory
static void UpdateMesh(Mesh *mesh, MeshGroupData *mg)
{
    //Apply the matrix to the mesh
    for (std::size_t i = 0; i < mesh->vertexCount; i++)
    {
        // TODO: optomize...
        auto idx = i * 3;
        glm::vec4 v = glm::vec4(mesh->vertexFloats[idx + 0], mesh->vertexFloats[idx + 1], mesh->vertexFloats[idx + 2], 1.0f);
        v = mg->gpuMat * glm::inverse(mg->meshMat) * v;
        mesh->vertexFloats[idx + 0] = v.x;
        mesh->vertexFloats[idx + 1] = v.y;
        mesh->vertexFloats[idx + 2] = v.z;
    }

    // Update the min and max values
    glm::vec4 min(mesh->MinVec.x, mesh->MinVec.y, mesh->MinVec.z, 1.0f);
    min = mg->gpuMat * glm::inverse(mg->meshMat) * min;
    mesh->MinVec.x = min.x;
    mesh->MinVec.y = min.y;
    mesh->MinVec.z = min.z;

    glm::vec4 max(mesh->MaxVec.x, mesh->MaxVec.y, mesh->MaxVec.z, 1.0f);
    max = mg->gpuMat * glm::inverse(mg->meshMat) * max;
    mesh->MaxVec.x = max.x;
    mesh->MaxVec.y = max.y;
    mesh->MaxVec.z = max.z;

    // Update the parameters
    mg->rotOnMesh = mg->rotOnMat;
    mg->scaleOnMesh = mg->scaleOnMat;
    mg->moveOnMesh = mg->moveOnMat + mg->meshCentre;
    mg->meshMat = mg->gpuMat;
    mg->meshMatDirty = false;
}

// The base opacity for all stl meshes
static float baseOpacity = 1.0f;

MeshGroupData::~MeshGroupData()
{
    // Delete the buffers

    if (mVertexPositionBuffer != 0)
    {
        glDeleteBuffers(1, &mVertexPositionBuffer);
        mVertexPositionBuffer = 0;
    }

    if (mVertexNormalBuffer != 0)
    {
        glDeleteBuffers(1, &mVertexNormalBuffer);
        mVertexNormalBuffer = 0;
    }
}

void STLRendering::FreeMemory()
{
    // Delete all the meshes on the heap
    for (auto &gPair : meshGroups)
    {
        delete gPair.second;
    }

    // Delete the program
    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }
}

void STLRendering::ProjMatDirty()
{
    dirtyProjMat = true;
}

void STLRendering::SceneMatDirty()
{
    dirtySceneMat = true;
}

// Update the meshdatagroup matrix
static inline void UpdateTempMat(MeshGroupData &mg)
{
    // TODO: optomize ?
    mg.gpuMat = glm::translate(glm::mat4(1.0f), mg.moveOnMat);
    mg.gpuMat = glm::scale(mg.gpuMat, glm::vec3(mg.scaleOnMat));
    mg.gpuMat = mg.gpuMat * glm::mat4(glm::quat(mg.rotOnMat));
    mg.gpuMat = glm::translate(mg.gpuMat, mg.meshCentre);

    mg.sceneMatsDirty = true;
    mg.meshMatDirty = true;
}

void STLRendering::PackMeshes()
{
    // TODO: this can probably be improved
    // TODO: run this async

    // If there is only one mesh then centre it
    if (meshGroups.size() == 1)
    {
        auto &pair = *meshGroups.begin();
        MeshGroupData *mg = pair.second;
        mg->moveOnMat.x = GlobalSettings::BedWidth.Get() / 2.0f;
        mg->moveOnMat.y = GlobalSettings::BedLength.Get() / 2.0f;
        UpdateTempMat(*mg);
        return;
    }

    // Create a list of those that still need placing
    std::vector<MeshGroupData*> mgsLeft;
    mgsLeft.reserve(meshGroups.size());
    for (auto pair : meshGroups)
        mgsLeft.push_back(pair.second);

    // Start by packing according to the bed length, we will try to fill the length with the longest object first,
    // continue filling the length with objects almost as wide as the first until the length is full and then move
    // on to a new row that will do the same
    std::vector<std::vector<std::tuple<MeshGroupData*, float, float>>> grid;
    std::vector<float> rowWidths; // TODO: combine maybe
    std::vector<float> rowLefts;
    float gridWidth = 0.0f;

    while (mgsLeft.size() != 0)
    {
        // Start a new row
        std::vector<std::tuple<MeshGroupData*, float, float>> row;
        float lLeft = GlobalSettings::BedLength.Get();

        // Add the longest element
        MeshGroupData* longestMg;
        float longestL = 0.0f;
        float longestW = 0.0f;
        bool turned = false;
        for (unsigned int i = 0; i < mgsLeft.size(); i++)
        {
            MeshGroupData* mg = mgsLeft[i];
            if (mg->length > mg->width && mg->length > longestL)
            {
                longestL = mg->length;
                longestW = mg->width;
                turned = false;
                longestMg = mg;
            }
            else if (mg->width > longestL)
            {
                longestL = mg->width;
                longestW = mg->length;
                turned = true;
                longestMg = mg;
            }
        }

        // Turn 90 degs if needed
        longestMg->rotOnMat.z = (turned) ? glm::radians(90.0f) : 0.0f;

        // Add to row
        row.push_back(std::make_tuple(longestMg, longestW, longestL));
        mgsLeft.erase(std::remove(mgsLeft.begin(), mgsLeft.end(), longestMg));

        lLeft -= longestL;

        // Now pack the rest of the row with whatever object comes closest to the width of the
        // first one and still fits in the row

        // Start by sorting according to closest match
        bool turneds[mgsLeft.size()];
        for (int i = 0; i < ((int)mgsLeft.size() - 1); i++)
        {
            float dif, dif2;
            unsigned int j;

            {
                MeshGroupData* mg = mgsLeft[i];
                float wDif = std::abs(longestW - mg->width);
                float lDif = std::abs(longestW - mg->length);


                if (wDif < lDif)
                {
                    turneds[i] = false;
                    dif = wDif;
                }
                else
                {
                    turneds[i] = true;
                    dif = lDif;
                }
            }

            for (j = i + 1; j < mgsLeft.size(); j++)
            {
                MeshGroupData* mg = mgsLeft[j];
                float wDif = std::abs(longestW - mg->width);
                float lDif = std::abs(longestW - mg->length);

                if (wDif < lDif)
                {
                    turneds[j] = false;
                    dif2 = wDif;
                }
                else
                {
                    turneds[j] = true;
                    dif2 = lDif;
                }
            }

            // Swap if smaller
            if (dif2 < dif)
            {
                auto tM = mgsLeft[i];
                bool tD = turneds[i];
                mgsLeft[i] = mgsLeft[j];
                turneds[i] = turneds[j];
                mgsLeft[j] = tM;
                turneds[j] = tD;
            }
        }

        // Try to fit the thinnest until full
        // We need to clone the vector in order to prevent
        // deleteion on the original one messing things up
        auto temp = std::vector<MeshGroupData*>(mgsLeft);
        for (unsigned int i = 0; i < temp.size(); i++)
        {
            MeshGroupData* mg = temp[i];
            float width, length;

            if (turneds[i])
            {
                width = mg->length;
                length = mg->width;
            }
            else
            {
                length = mg->length;
                width = mg->width;
            }

            if ((lLeft - length) > 0)
            {
                lLeft -= length;

                // Turn 90 degs if needed
                mg->rotOnMat.z = (turneds[i]) ? glm::radians(90.0f) : 0.0f;

                // Add to row
                row.push_back(std::make_tuple(mg, width, length));
                mgsLeft.erase(std::remove(mgsLeft.begin(), mgsLeft.end(), mg));

                // Update the row width
                longestW = std::max(longestW, width);
            }
        }

        grid.push_back(row);
        rowWidths.push_back(longestW);
        gridWidth += longestW;
        rowLefts.push_back(lLeft);
    }

    // Now we can place the items according to the grid
    // If the bed is too small then go over it to alert the user
    float xSpacing = std::max((GlobalSettings::BedWidth.Get() - gridWidth) / (grid.size() + 1), 5.0f);
    float curX = 0.0f;
    for (unsigned int i = 0; i < grid.size(); i++)
    {
        std::vector<std::tuple<MeshGroupData*, float, float>> &row = grid[i];
        curX += xSpacing + rowWidths[i];
        float rowX = curX - (rowWidths[i] / 2.0f);
        float curY = 0.0f;

        for (std::tuple<MeshGroupData*, float, float> item : row)
        {
            MeshGroupData* mg;
            float width, length;
            std::tie(mg, width, length) = item;
            float ySpacing = rowLefts[i] / (row.size() + 1);
            curY += ySpacing + length;
            mg->moveOnMat.x = rowX;
            mg->moveOnMat.y = curY - (length / 2.0f);
            UpdateTempMat(*mg);
        }
    }

    // Call OpenGL upate
    ComboRendering::Update();
}

void STLRendering::AddMesh(Mesh *mesh)
{
    // Add a new mesh data group to the vector that contains all the metadata and helpers
    MeshGroupData *mg = new MeshGroupData;
    mg->meshDirty = true;
    meshGroups.emplace(mesh, mg);

    // Calculate the neccessary dimensions for the mesh

    mg->width = mesh->MaxVec.x - mesh->MinVec.x;
    mg->length = mesh->MaxVec.y - mesh->MinVec.y;
    mg->height = mesh->MaxVec.z - mesh->MinVec.z;

    mg->meshCentre = glm::vec3(-(mesh->MaxVec.x + mesh->MinVec.x) / 2.0f, -(mesh->MaxVec.y + mesh->MinVec.y) / 2.0f, -(mesh->MaxVec.z + mesh->MinVec.z) / 2.0f);
    mg->moveOnMat.z = mg->height / 2.0f;

    // Create a bounding sphere around the centre of the mesh using the largest distance as radius
    mg->bSphereRadius = std::max(mg->length, std::max(mg->width, mg->height)) / 2.0f;

    // Try to pack the meshes
    PackMeshes(); // TODO: async

    // Signal the global dirty mesh flag
    dirtyMesh = true;
}

void STLRendering::RemoveMesh(Mesh *mesh)
{
    // Queue for deletion in the opengl thread
    toDelete.push(meshGroups[mesh]);
    meshGroups.erase(mesh);

    // Try to repack the meshes
    PackMeshes(); // TODO: async
}

// TODO: these transofrmations hould probably be applied in a bg thread

// This method applies an absolute scale to the original mesh
void STLRendering::ScaleMesh(Mesh *mesh, float absScale)
{
    // We can't scale to 0
    if (absScale == 0)
        absScale = 0.01f;

    MeshGroupData &mg = *meshGroups[mesh];

    // Update the dimension
    mg.moveOnMat.z -= (mg.height / 2.0f);
    mg.moveOnMat.z += (mg.height / 2.0f) * absScale / mg.scaleOnMat;
    mg.bSphereRadius *= absScale / mg.scaleOnMat;
    mg.length *= absScale / mg.scaleOnMat;
    mg.width *= absScale / mg.scaleOnMat;
    mg.height *= absScale / mg.scaleOnMat;

    mg.scaleOnMat = absScale;

    UpdateTempMat(mg);

    // Call OpenGL upate
    ComboRendering::Update();
}

// This method centres the mesh around the current coordinates
void STLRendering::CentreMesh(Mesh *mesh, float absX, float absY)
{
    MeshGroupData &mg = *meshGroups[mesh];

    mg.moveOnMat.x = absX;
    mg.moveOnMat.y = absY;

    UpdateTempMat(mg);

    // Call OpenGL upate
    ComboRendering::Update();
}

// This method places the mesh an absolute height above the bed
void STLRendering::LiftMesh(Mesh *mesh, float absZ)
{
    MeshGroupData &mg = *meshGroups[mesh];

    // The model has its 0 z at its centre so we need to account for that
    mg.moveOnMat.z = absZ + (mg.height / 2.0f);

    UpdateTempMat(mg);

    // Call OpenGL upate
    ComboRendering::Update();
}

// This method applies an absolute rotation to the original mesh
void STLRendering::RotateMesh(Mesh *mesh, float absX, float absY, float absZ)
{
    MeshGroupData &mg = *meshGroups[mesh];

    mg.rotOnMat = glm::vec3(glm::radians(absX), glm::radians(absY), glm::radians(absZ));

    UpdateTempMat(mg);

    // Call OpenGL upate
    ComboRendering::Update();
}

const MeshGroupData &STLRendering::getMeshData(Mesh *mesh)
{
    return *meshGroups[mesh];
}

bool STLRendering::TestMeshIntersection(Mesh *mesh, const glm::vec3 &near, const glm::vec3 &far, const glm::mat4 &MV, float &screenZ)
{
    auto &mg = *meshGroups[mesh];

    glm::vec3 sect, norm, sect2, norm2;
    if (glm::intersectLineSphere(near, far, mg.moveOnMat, mg.bSphereRadius, sect, norm, sect2, norm2))
    {
        glm::vec4 s1 = MV * glm::vec4(sect, 1.0f);
        glm::vec4 s2 = MV * glm::vec4(sect2, 1.0f);
        screenZ = std::min(s1.z, s2.z); // -1 is the highest possible

        return true;
    }

    return false;
}

void STLRendering::ColorMesh(Mesh *mesh, const glm::vec4 &colorAlpha)
{
    meshGroups[mesh]->color = colorAlpha;
}

void STLRendering::ColorMesh(Mesh *mesh, const glm::vec3 &color)
{
    meshGroups[mesh]->color = glm::vec4(color, meshGroups[mesh]->color.w);
}

void STLRendering::ColorMesh(Mesh *mesh, float alpha)
{
    meshGroups[mesh]->color.w = alpha;
}

void STLRendering::SetBaseOpacity(float alpha)
{
    baseOpacity = alpha;

    // Call OpenGL upate
    ComboRendering::Update();
}

float STLRendering::GetBaseOpacity()
{
    return baseOpacity;
}

bool STLRendering::PrepMeshesSave(std::set<Mesh *> &meshes)
{
    bool changed = false;

    for (Mesh *mesh : meshes)
    {
        auto mg = meshGroups[mesh];
        if (mg->meshMatDirty)
        {
            changed = true;
            UpdateMesh(mesh, mg);
        }
    }

    return changed;
}

void STLRendering::Init()
{
    // Shader source files
    const std::string vs = "mesh.vsh";
    const std::string fs = "minimal.fsh";

    // Set up the shader and its uniform/attribute locations.
    mProgram = GLHelper::CompileProgramFromFile(vs, fs);
    mPositionAttribLocation = glGetAttribLocation(mProgram, "aPosition");
    mModelUniformLocation = glGetUniformLocation(mProgram, "uModelMatrix");
    mProjUniformLocation = glGetUniformLocation(mProgram, "uProjMatrix");
    mNormalAttribLocation = glGetAttribLocation(mProgram, "aNormal");
    mNormUniformLocation = glGetUniformLocation(mProgram, "uNormMatrix");
    mColorUniformLocation = glGetUniformLocation(mProgram, "uMeshColor");
}

void STLRendering::Draw()
{
    if (mProgram == 0)
        return;

    glUseProgram(mProgram);

    while (toDelete.size() != 0)
    {
        delete toDelete.front();
        toDelete.pop();
        dirtyMesh = true;
    }

    if (dirtyMesh)
    {
        for (auto &gPair : meshGroups)
        {
            if (gPair.second->meshDirty)
                LoadMesh(*gPair.second, gPair.first);
        }

        dirtyMesh = false;
    }

    if (dirtyProjMat)
    {
        glUniformMatrix4fv(mProjUniformLocation, 1, GL_FALSE, glm::value_ptr(ComboRendering::getSceneProj()));
        dirtyProjMat = false;
    }

    for (auto &gPair : meshGroups)
    {
        MeshGroupData &mg = *gPair.second;

        if (mg.mVertexPositionBuffer == 0 || mg.mVertexNormalBuffer == 0)
            continue;

        // Set the matrices to those of this mesh, update if needed first
        if (mg.sceneMatsDirty || dirtySceneMat)
        {
            mg.sceneMat = ComboRendering::getSceneTrans() * mg.gpuMat;
            mg.normalMat = glm::inverse(mg.sceneMat);
            mg.sceneMatsDirty = false;
        }
        glUniformMatrix4fv(mModelUniformLocation, 1, GL_FALSE, glm::value_ptr(mg.sceneMat));
        glUniformMatrix4fv(mNormUniformLocation, 1, GL_FALSE, glm::value_ptr(mg.normalMat));

        // Set the colour for the mesh
        auto tempCol = mg.color;
        tempCol.w *= baseOpacity;
        glUniform4fv(mColorUniformLocation, 1, glm::value_ptr(tempCol));

        glBindBuffer(GL_ARRAY_BUFFER, mg.mVertexPositionBuffer);
        glEnableVertexAttribArray(mPositionAttribLocation);
        glVertexAttribPointer(mPositionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, mg.mVertexNormalBuffer);
        glEnableVertexAttribArray(mNormalAttribLocation);
        glVertexAttribPointer(mNormalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glDrawArrays(GL_TRIANGLES, 0, gPair.first->trigCount * 3);
    }

    dirtySceneMat = false;
}
