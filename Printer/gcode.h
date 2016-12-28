#ifndef GCODE_H
#define GCODE_H

#include <string>
#include <vector>

class GCode{
private:
    unsigned short g, m = 0;
    unsigned char t = 0;
    float x, y, z, e, f,ii,j,r;
    int s, p;
    unsigned short fields = 128;
    unsigned short fields2 = 0;
    std::string text;

    void ActivateV2OrForceAscii() {}

    void PushBytes(std::vector<char> &buf, void *ptr, std::size_t length) {
        char *read = reinterpret_cast<char*>(ptr);
        for (std::size_t i = 0; i < length; i++)
            buf.push_back(read[i]);
    }

public:
    int n;
    std::string orig;

    bool hasCode() {
        return (fields != 128);
    }
    bool hasText() {
        return ((fields & 32768) != 0);
    }

    std::string getText() {
        return text;
    }
    void setText(std::string value) {
        text = value;

        if (text.length() > 16)
            ActivateV2OrForceAscii();

        fields |= 32768;
    }

    bool hasN() {
        return ((fields & 1) != 0);
    }
    int getN() {
        return n;
    }
    void setN(int value) {
        n = value;
        fields |= 1;
    }

    bool hasM() {
        return ((fields & 2) != 0);
    }
    unsigned short getM() {
        return m;
    }
    void setN(unsigned short value) {
        m = value;
        fields |= 2;
    }

    bool hasG() {
        return ((fields & 4) != 0);
    }
    unsigned short getG() {
        return g;
    }
    void setG(unsigned short value) {
        g = value;
        fields |= 4;
    }

    bool hasT() {
        return ((fields & 512) != 0);
    }
    unsigned char getT() {
        return t;
    }
    void setG(unsigned char value) {
        t = value;
        fields |= 512;
    }

    bool hasS() {
        return ((fields & 1024) != 0);
    }
    int getS() {
        return s;
    }
    void setS(int value) {
        s = value;
        fields |= 1024;
    }

    bool hasP() {
        return ((fields & 2048) != 0);
    }
    int getP() {
        return p;
    }
    void setP(int value) {
        p = value;
        fields |= 2048;
    }

    bool hasX() {
        return ((fields & 8) != 0);
    }
    float getX() {
        return x;
    }
    void setX(float value) {
        x = value;
        fields |= 8;
    }

    bool hasY() {
        return ((fields & 16) != 0);
    }
    float getY() {
        return y;
    }
    void setY(float value) {
        y = value;
        fields |= 16;
    }

    bool hasZ() {
        return ((fields & 32) != 0);
    }
    float getZ() {
        return z;
    }
    void setZ(float value) {
        z = value;
        fields |= 32;
    }

    bool hasE() {
        return ((fields & 64) != 0);
    }
    float getE() {
        return e;
    }
    void setE(float value) {
        e = value;
        fields |= 64;
    }

    bool hasF() {
        return ((fields & 256) != 0);
    }
    float getF() {
        return f;
    }
    void setF(float value) {
        f = value;
        fields |= 256;
    }

    bool hasI() {
        return ((fields2 & 1) != 0);
    }
    float getI() {
        return ii;
    }
    void setI(float value) {
        ii = value;
        fields2 |= 1;
        ActivateV2OrForceAscii();
    }

    bool hasJ() {
        return ((fields2 & 2) != 0);
    }
    float getJ() {
        return j;
    }
    void setJ(float value) {
        j = value;
        fields2 |= 2;
        ActivateV2OrForceAscii();
    }

    bool hasR() {
        return ((fields2 & 4) != 0);
    }
    float getR() {
        return r;
    }
    void setR(float value) {
        r = value;
        fields2 |= 4;
        ActivateV2OrForceAscii();
    }

    bool isV2() {
        return ((fields & 4096) != 0);
    }

    std::vector<char> getBinary(int version) {
        bool v2 = isV2();

        std::vector<char> buf;

#define PB(item) \
    PushBytes(buf, &item, sizeof(item));

#define PBC(type, item) { \
        type tempPBC = item; \
        PushBytes(buf, &tempPBC, sizeof(type)); \
    }

        PB(fields);
        if (v2) {
            PB(fields2);
            if (hasText())
                PBC(unsigned char, text.length());
        }

        if (hasN()) {
            PBC(unsigned short, n & 65535);
        }
        if (v2) {
            if (hasM())
                PB(m);
            if (hasG())
                PB(g);
        }
        else {
            if (hasM())
                PBC(unsigned char, m);
            if (hasG())
                PBC(unsigned char, g);
        }

        if (hasX())
            PB(x);
        if (hasY())
            PB(y);
        if (hasZ())
            PB(z);
        if (hasE())
            PB(e);
        if (hasF())
            PB(f);
        if (hasT())
            PB(t);
        if (hasS())
            PB(s);
        if (hasP())
            PB(p);
        if (hasI())
            PB(ii);
        if (hasJ())
            PB(j);
        if (hasR())
            PB(r);

        if (hasText()) {
            int i, len = text.length();

            if (v2) {
                for (i = 0; i < len; i++)
                    PB(text[i]);
            }
            else {
                if (len > 16)
                    len = 16;

                for (i = 0; i < len; i++)
                    PB(text[i]);

                for (; i < 16; i++)
                    PBC(char, 0);
            }
        }

        // compute fletcher-16 checksum
        int sum1, sum2 = 0;
        for (char c : buf)
        {
            sum1 = (sum1 + c) % 255;
            sum2 = (sum2 + sum1) % 255;
        }
        PBC(char, sum1);
        PBC(char, sum2);

        buf.shrink_to_fit();
        return buf;
    }
};

#endif // GCODE_H
