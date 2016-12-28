#include "gcodeimporting.h"

#include <fstream>
#include <string>
#include <cstring>

#include "structures.h"

#include <QDebug>

inline void ShrinkIsle(Island *isle)
{
    if (isle != nullptr)
    {
        if (isle->movePoints.size() > 0)
            isle->movePoints.shrink_to_fit();

        if (isle->printPoints.size() > 0)
            isle->printPoints.shrink_to_fit();
    }
}

Toolpath* GCodeImporting::ImportGCode(const char *path)
{
    std::ifstream is(path);

    Toolpath *tp = new Toolpath();

    // Check for a valid file
    if (is)
    {
        // Store the previous values for G0 and G1
        float prevX = -1;
        float prevY = -1;
        float prevZ = -1;
        float prevF[2] = {-1, -1};
        float prevE = 0;
        bool prevValid = false;

        Layer *curLayer = nullptr;
        Island *curIsle = nullptr;
        Point3 lastPoint;
        Point2 lastPoint2;

        int8_t g = -1;
        bool rel = false;

        // Keep track if the last action was a move or
        // an extrusion
        bool lastWasMove = true;

        // TODO: add support for set pos command

        // Read through each line
        std::string line;
        LineInfo *curLineInfo = nullptr;
        int64_t lineNum = -1;
        while (std::getline(is, line))
        {
            lineNum++;
            tp->lineInfos.emplace_back();
            curLineInfo = &(tp->lineInfos.back());

            bool extruded = false;
            bool setPos = false;
            int type = -1;

            // Remove all comments from the line
            line = line.substr(0, line.find(';') - 1);

            //qDebug() << QString(line.c_str());

            // Confirm the line starts wit g and get the code
            if (line[0] != 'G')
                continue;
            else
            {
                auto pos = line.find(' ');
                std::string code = line.substr(1, pos - 1);
                type = std::stoi(code);

                if (type == 0 || type == 1)
                    g = type;
                else if (type == SetPos)
                    setPos = true;
                else
                {
                    if (type == SetRel)
                        rel = true;
                    else if (type == SetAbs)
                        rel = false;
                    else if (type == Home)
                    {
                        prevX = 0;
                        prevY = 0;
                        prevZ = 0;
                    }

                    continue;
                }
            }

            bool moved = false; // Flag if movement occured

            // Create a point at the last position
            lastPoint = { prevX, prevY, prevZ };
            lastPoint2 = { prevX, prevY, -1 }; // The zero is the irrelevant line number

            // Read the parts as spilt by spaces
            // TODO: add support or at least warnings for irregularly long whitespaces
            // Working correctly with the c_str  directly in strtok
            // breaks the std::string a bit and complicates debugging
//#define PRESERVE_LINE_STR
#ifdef PRESERVE_LINE_STR
            auto temp = line.c_str();
            auto len = strlen(temp);
            char *lineCpy = new char[len];
            memcpy(lineCpy, temp, sizeof(char) * len);
            char *pch = std::strtok(lineCpy, " ");
#else
            char *pch = std::strtok((char*)line.c_str(), " ");
#endif

            while (pch != NULL)
            {
                std::string part(pch);
                char c = part[0];
                auto numLen = strlen(pch); // Keep the null termination (-1 + 1)
                char numS[numLen]; // Buffer for number
                memcpy(numS, &pch[1], numLen);
                float num = std::atof(numS);

                pch = std::strtok(NULL, "     "); // Get the next part so long

                switch(c) {
                    case 'X' :
                    {
                        if (setPos)
                        {
                            prevX = num;
                            moved = true;
                            break;
                        }

                        float old = prevX;

                        if (rel)
                            prevX += num;
                        else
                            prevX = num;

                        if (!moved && (prevX != old))
                        {
                            moved = true;
                        }
                        break;
                    }
                    case 'Y' :
                    {
                        if (setPos)
                        {
                            prevY = num;
                            moved = true;
                            break;
                        }

                        float old = prevY;

                        if (rel)
                            prevY += num;
                        else
                            prevY = num;

                        if (!moved && (prevY != old))
                        {
                            moved = true;
                        }
                        break;
                    }
                    case 'Z' :
                    {
                        if (setPos)
                        {
                            prevZ = num;
                            moved = true;
                            break;
                        }

                        float nZ = (rel) ? (prevZ + num) : num;

                        if (nZ != prevZ)
                        {
                            // Create a new layer when moving to a new z
                            // and optimize the old one
                            //if (curLayer != nullptr && curLayer->islands.size() > 0)
                              //  curLayer->islands.shrink_to_fit();

                            tp->layers.emplace_back();
                            curLayer = &(tp->layers.back());
                            curLayer->z = num;

                            ShrinkIsle(curIsle);

                            curLayer->islands.emplace_back();
                            lastWasMove = true;
                            curIsle = &(curLayer->islands.back());
                            // Move to the island
                            curIsle->movePoints.push_back(lastPoint);

                            moved = true;
                        }

                        prevZ = num;
                        break;
                    }
                    case 'F' :
                    {
                        // G92 doesn't have an F parameter
                        //if (rel) // don't think this exists
                          //  prevF[g] += num;
                        //else
                            prevF[g] = num;
                        break;
                    }
                    case 'E' :
                    {
                        if (setPos)
                        {
                            prevE = num;
                            moved = true;
                            break;
                        }

                        float nE = (rel) ? (prevE + num) : num;

                        // TODO: detect retractions
                        if (prevE != nE)
                            extruded = true;
                        break;
                    }
                }
            }
#ifdef PRESERVE_LINE_STR
            delete[] lineCpy;
#endif

            // If no parameters were given for G92 then everything becomes 0
            if (setPos)
            {
                if (!moved)
                {
                    prevX = 0.0f;
                    prevY = 0.0f;
                    prevZ = 0.0f;
                    prevE = 0.0f;
                }

                continue;
            }

            // Skip the line if there was no movement
            if (!moved)
            {
                continue;
            }

            // Wait until we have current values to work from
            if (!prevValid)
            {
                prevValid = (prevX != -1) && (prevY != -1) && (prevZ != -1);

                if (!prevValid)
                    continue;
            }

            // TODO: points get lost when not on layers
            if (curLayer == nullptr)
                continue;

            // Roughly estimate the time for the line
            double dist = std::sqrt(std::pow(prevX - lastPoint.x, 2) + std::pow(prevY - lastPoint.y, 2));
            curLineInfo->milliSecs = dist * 60000 / prevF[g]; // Feedrate per minute...
            tp->totalMillis += curLineInfo->milliSecs;

            if (extruded)
            {
                // Add the first point after moves
                if (lastWasMove)
                    curIsle->printPoints.push_back(lastPoint2);

                curIsle->printPoints.push_back({prevX, prevY, lineNum});
                curLineInfo->isExtruded = true;

                lastWasMove = false;
            }
            else
            {
                if (!lastWasMove)
                {
                    ShrinkIsle(curIsle);

                    // If the last action was not a move, then we are now starting a new island
                    curLayer->islands.emplace_back();
                    curIsle = &(curLayer->islands.back());

                    // Move to the island
                    curIsle->movePoints.push_back(lastPoint);
                }

                curIsle->movePoints.push_back({prevX, prevY, prevZ});
                curLineInfo->isMove = true;
                lastWasMove = true;
            }
        }

        if (curLayer != nullptr && curLayer->islands.size() > 0)
            curLayer->islands.shrink_to_fit();

        if (curIsle != nullptr)
        {
            curIsle->movePoints.shrink_to_fit();
            curIsle->printPoints.shrink_to_fit();
        }
    }

    return tp;
}
