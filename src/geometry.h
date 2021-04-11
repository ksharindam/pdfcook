#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "pdf_objects.h"

class Point
{
public:
    float x = 0;
    float y = 0;

    Point();
    Point(float x, float y);
};

class Rect {
public:
    Point left; // bottom left (0,0)
    Point right;// top right

    bool isZero();
    bool isLandscape();
    bool getFromObject(PdfObject *src, ObjectTable &table);
    bool setToObject(PdfObject *dst);
};

// transformation order : scale -> rotate -> translate

class Matrix
{
public:
    float mat[3][3];
    Matrix();// creates identity matrix
    Matrix( float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22);
    bool isIdentity();
    void multiply(const Matrix &m);
    void scale (float scale);
    void rotate (float angle_deg);
    void translate (float dx, float dy);

    void transform (Point &point);
    void transform (Rect &dim);
};

