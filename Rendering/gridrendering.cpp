#include "gridrendering.h"

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glhelper.h"
#include "mathhelper.h"
#include "comborendering.h"
#include "Misc/globalsettings.h"

static GLuint mProgram = 0;
static GLint mPositionAttribLocation = 0;
static GLint mModelUniformLocation = 0;
static GLint mProjUniformLocation = 0;
static GLuint mVertexPositionBuffer = 0;

static GridGeneration::Grid *grid = nullptr;
static unsigned int vertCount = 0;

// We need flags to determine when matrices have changed as
// to be able to give new ones to opengl
bool dirtyProjMat = true;
bool dirtySceneMat = true;
bool dirtyGrid = false;
bool gridLoaded = false;

void GridRendering::FreeMemory()
{
    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }

    if (mVertexPositionBuffer != 0)
    {
        glDeleteBuffers(1, &mVertexPositionBuffer);
        mVertexPositionBuffer = 0;
    }

    if (grid != nullptr)
        delete grid;
}

void GridRendering::Init()
{
    // Shader source files
    const std::string vs = "line.vsh";
    const std::string fs = "minimal.fsh";

    // Set up the shader and its uniform/attribute locations.
    mProgram = GLHelper::CompileProgramFromFile(vs, fs);
    mPositionAttribLocation = glGetAttribLocation(mProgram, "aPosition");
    mModelUniformLocation = glGetUniformLocation(mProgram, "uModelMatrix");
    mProjUniformLocation = glGetUniformLocation(mProgram, "uProjMatrix");

    glGenBuffers(1, &mVertexPositionBuffer);
}

void GridRendering::ProjMatDirty()
{
    dirtyProjMat = true;
}

void GridRendering::SceneMatDirty()
{
    dirtySceneMat = true;
}

void GridRendering::GridDirty()
{
    const float gridSpacing = 10.0f;

    // Make sure the grid size is valid
    auto w = std::max(GlobalSettings::BedWidth.Get(), gridSpacing);
    auto l = std::max(GlobalSettings::BedLength.Get(), gridSpacing);
    auto h = std::max(GlobalSettings::BedHeight.Get(), gridSpacing);

    if (grid != nullptr)
        delete grid;

    grid = GridGeneration::GenerateGrids(w, l, h, gridSpacing);
    vertCount = grid->floatCount / 3;

    dirtyGrid = true;
}

void GridRendering::Draw()
{
    if (mProgram == 0)
        return;

    glUseProgram(mProgram);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexPositionBuffer);
    glEnableVertexAttribArray(mPositionAttribLocation);
    glVertexAttribPointer(mPositionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Apply the scene tranformation matrix if it has changed
    if (dirtySceneMat)
    {        
        glUniformMatrix4fv(mModelUniformLocation, 1, GL_FALSE, glm::value_ptr(ComboRendering::getSceneTrans()));
        dirtySceneMat = false;
    }

    // Apply the new projection matrix if it has changed
    if (dirtyProjMat)
    {  
        glUniformMatrix4fv(mProjUniformLocation, 1, GL_FALSE, glm::value_ptr(ComboRendering::getSceneProj()));
        dirtyProjMat = false;
    }

    if (vertCount == 0)
        return;

    if (dirtyGrid)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVertexPositionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * grid->floatCount, grid->floats, GL_STATIC_DRAW);

        delete grid;
        grid = nullptr;
        dirtyGrid = false;
    }

    glDrawArrays(GL_LINES, 0, vertCount);
}
