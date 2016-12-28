#include "toolpathrendering.h"

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <thread>
#include <iostream>
#include <time.h>
#include <vector>

#include "glhelper.h"
#include "mathhelper.h"
#include "gcodeimporting.h"
#include "comborendering.h"
#include "Misc/globalsettings.h"

struct GroupGLData
{
    ~GroupGLData();

    GLuint mCurPosBuffer = 0;
    GLuint mNextPosBuffer = 0;
    GLuint mPrevPosBuffer = 0;
    GLuint mSideBuffer = 0;

    uint16_t *indices;
    uint16_t *lineIdxs;
    short idxCount = 0;
    short lineIdxCount = 0;
};

static GLuint mProgram = 0;
static GLint mCurPosAttribLocation = 0;
static GLint mNextPosAttribLocation = 0;
static GLint mPrevPosAttribLocation = 0;
static GLint mSideAttribLocation = 0;

static GLint mModelUniformLocation = 0;
static GLint mRadiusUniformLocation = 0;
static GLint mProjUniformLocation = 0;
static GLint mColorUniformLocation = 0;
static GLint mLineOnlyUnformLocation = 0;

static int64_t curPrintToLine = -1;
static int64_t targetPrintToLine = -1;
static int printToChunk = -1;
static int printToIdx = -1;
static int printToLineIdx = -1;

static GroupGLData *groupDatas = nullptr;
static std::size_t groupCount = 0;

// We need flags to determine when matrices have changed as
// to be able to give new ones to opengl
static bool dirtyProjMat = true;
static bool dirtySceneMat = true;
static bool dirtyPath = false;
static bool dirtyColor = true;

static glm::vec3 _color = glm::vec3(0.2f, 0.2f, 0.8f);
static float opacity = 1.0f;

// If the gcode rendering is becoing too slow then we need to temporarily fall back
// a simpler rendering method. We then have a flag that redraws the current frame
// if it has been idle for long enough.

static volatile bool complexify = false;

static clock_t lastDrawTime = clock();

static void checkComplexify()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (complexify)
        {
            lastDrawTime = clock();
            ComboRendering::Update();
            complexify = false;
        }
    }
}

static std::thread *complexifyThread = nullptr;

void ToolpathRendering::FreeMemory()
{
    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }

    if (groupDatas != nullptr)
    {
        delete[] groupDatas;
        groupDatas = nullptr;
    }

    if (complexifyThread != nullptr)
        delete complexifyThread;
}

static Toolpath *path = nullptr;

static void LoadPath()
{
    dirtyPath = false;

    if (groupDatas != nullptr)
        delete[] groupDatas;

    auto chunks = path->CalculateDataChunks();
    groupCount = chunks->size();
    groupDatas = new GroupGLData[groupCount];

    for (std::size_t i = 0; i < groupCount; i++)
    {
        TPDataChunk *dc = &chunks->at(i);
        GroupGLData *gd = groupDatas + i;

        glGenBuffers(1, &gd->mCurPosBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gd->mCurPosBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * dc->curFloatCount, dc->curFloats, GL_STATIC_DRAW);

        glGenBuffers(1, &gd->mNextPosBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gd->mNextPosBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * dc->nextFloatCount, dc->nextFloats, GL_STATIC_DRAW);

        glGenBuffers(1, &gd->mPrevPosBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gd->mPrevPosBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * dc->prevFloatCount, dc->prevFloats, GL_STATIC_DRAW);

        glGenBuffers(1, &gd->mSideBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gd->mSideBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * dc->sideFloatCount, dc->sides, GL_STATIC_DRAW);

        gd->indices = dc->getIndices();
        gd->lineIdxs = dc->getLineIdxs();
        gd->idxCount = dc->idxCount;
        gd->lineIdxCount = dc->lineIdxCount;
    }

    delete chunks;
}

void ToolpathRendering::SceneMatDirty()
{
    dirtySceneMat = true;
}

void ToolpathRendering::ProjMatDirty()
{
    dirtyProjMat = true;
}

void ToolpathRendering::SetToolpath(Toolpath *tp)
{
    path = tp;
    dirtyPath = true;
}

void ToolpathRendering::ShowPrintedToLine(int64_t lineNum)
{
    if (lineNum < 0)
    {
        targetPrintToLine = -1;
        curPrintToLine = -1;
        printToChunk = -1;
        printToIdx = -1;
    }
    else
        targetPrintToLine = lineNum;

    ComboRendering::Update();
}

GroupGLData::~GroupGLData()
{
    if (mCurPosBuffer != 0)
    {
        glDeleteBuffers(1, &mCurPosBuffer);
        mCurPosBuffer = 0;
    }

    if (mNextPosBuffer != 0)
    {
        glDeleteBuffers(1, &mNextPosBuffer);
        mNextPosBuffer = 0;
    }

    if (mPrevPosBuffer != 0)
    {
        glDeleteBuffers(1, &mPrevPosBuffer);
        mPrevPosBuffer = 0;
    }

    if (mSideBuffer != 0)
    {
        glDeleteBuffers(1, &mSideBuffer);
        mSideBuffer = 0;
    }

    if (indices != nullptr)
    {
        free(indices);
        indices = nullptr;
    }

    if (lineIdxs != nullptr)
    {
        free(lineIdxs);
        lineIdxs = nullptr;
    }
}

void ToolpathRendering::Init()
{
    // Shader source files
    //const std::string vs = "line.vsh";
    const std::string vs = "filament.vsh";
    const std::string fs = "filament.fsh";

    // Set up the shader and its uniform/attribute locations.
    mProgram = GLHelper::CompileProgramFromFile(vs, fs);
    mModelUniformLocation = glGetUniformLocation(mProgram, "uModelMatrix");
    mRadiusUniformLocation = glGetUniformLocation(mProgram, "uFilamentRadius");
    mColorUniformLocation = glGetUniformLocation(mProgram, "uColor");
    mLineOnlyUnformLocation = glGetUniformLocation(mProgram, "uLineOnly");
    mProjUniformLocation = glGetUniformLocation(mProgram, "uProjMatrix");

    mCurPosAttribLocation = glGetAttribLocation(mProgram, "aCurPos");
    mNextPosAttribLocation = glGetAttribLocation(mProgram, "aNextPos");
    mPrevPosAttribLocation = glGetAttribLocation(mProgram, "aPrevPos");
    mSideAttribLocation = glGetAttribLocation(mProgram, "aSide");

    // Start the complexifying thread
    complexifyThread = new std::thread(checkComplexify);
    complexifyThread->detach();
}

void ToolpathRendering::Draw()
{
    if (mProgram == 0)
        return;

    glUseProgram(mProgram);

    if (dirtyPath)
        LoadPath();

    if (dirtySceneMat)
    {
        glUniformMatrix4fv(mModelUniformLocation, 1, GL_FALSE, glm::value_ptr(ComboRendering::getSceneTrans()));
        dirtySceneMat = false;
    }

    if (dirtyProjMat)
    {
        glUniformMatrix4fv(mProjUniformLocation, 1, GL_FALSE, glm::value_ptr(ComboRendering::getSceneProj()));
        dirtyProjMat = false;
    }

    if (dirtyColor)
    {
        glUniform4fv(mColorUniformLocation, 1, glm::value_ptr(glm::vec4(_color, opacity)));
        dirtyColor = false;
    }

    if (path != nullptr && targetPrintToLine != curPrintToLine)
    {
        printToChunk = path->lineInfos[targetPrintToLine - 1].chunkIdx;
        printToIdx = path->lineInfos[targetPrintToLine - 1].idxInChunk;
        printToLineIdx = path->lineInfos[targetPrintToLine - 1].lineIdxInChunk;
        curPrintToLine = targetPrintToLine;
    }

    clock_t now = clock();
    clock_t deltaTicks = now - lastDrawTime;
    clock_t fps = 0;
    if (deltaTicks > 0)
        fps = CLOCKS_PER_SEC / deltaTicks / 4.0; // Not sure why 4
    //std::cout << "FPS: " << fps << std::endl;
    lastDrawTime = now;
    bool simpleDraw = (fps < 30.0);

    std::size_t target = (printToChunk != -1) ? printToChunk + 1 : groupCount;
    for (std::size_t i = 0; i < target; i++)
    {
        GroupGLData *ld = groupDatas + i;

        glBindBuffer(GL_ARRAY_BUFFER, ld->mCurPosBuffer);
        glEnableVertexAttribArray(mCurPosAttribLocation);
        glVertexAttribPointer(mCurPosAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, ld->mNextPosBuffer);
        glEnableVertexAttribArray(mNextPosAttribLocation);
        glVertexAttribPointer(mNextPosAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, ld->mPrevPosBuffer);
        glEnableVertexAttribArray(mPrevPosAttribLocation);
        glVertexAttribPointer(mPrevPosAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, ld->mSideBuffer);
        glEnableVertexAttribArray(mSideAttribLocation);
        glVertexAttribPointer(mSideAttribLocation, 1, GL_FLOAT, GL_FALSE, 0, 0);

        // TODO: implement line rendering using strips and fix issues

        // TODO: derive the layer height from the actual gcode instead
        // or maybe rather use the extrusion diameter
        glUniform1f(mRadiusUniformLocation, 0.225f); // Almost half 0.5f       

        if (simpleDraw)
        {
            glUniform1i(mLineOnlyUnformLocation, true);
            glDrawElements(GL_LINES, (i == target - 1 && printToChunk != -1) ? printToLineIdx : ld->lineIdxCount, GL_UNSIGNED_SHORT, ld->lineIdxs);
            complexify = true;
        }
        else
        {
            glUniform1i(mLineOnlyUnformLocation, false);
            glDrawElements(GL_TRIANGLES, (i == target - 1 && printToChunk != -1) ? printToIdx : ld->idxCount, GL_UNSIGNED_SHORT, ld->indices);
        }
    }
}

void ToolpathRendering::SetOpacity(float alpha)
{
    opacity = alpha;
    dirtyColor = true;

    // Call OpenGL upate
    ComboRendering::Update();
}

float ToolpathRendering::GetOpacity()
{
    return opacity;
}

void ToolpathRendering::SetColor(glm::vec3 color)
{
    _color = color;
    dirtyColor = true;
}

bool ToolpathRendering::ToolpathLoaded()
{
    return (path != nullptr);
}
