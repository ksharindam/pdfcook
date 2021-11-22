/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "common.h"
#include "geometry.h"
#include "debug.h"

static Point min_coordinate (Point p1, Point p2);
static Point max_coordinate (Point p1, Point p2);


Point:: Point() : x(0), y(0)
{ }

Point:: Point(float _x, float _y) : x(_x), y(_y)
{ }


bool Rect:: isZero()
{
    return (left.x==0 && left.y==0 && right.x==0 && right.y==0);
}

bool Rect:: isLandscape()
{
    return right.x > right.y;
}


bool Rect:: getFromObject(PdfObject *src, ObjectTable &obj_table)
{
    if (!src)
        return false;
    if (isRef(src)) {
        src = obj_table.getObject(src->indirect.major, src->indirect.minor);
    }
    assert(isArray(src));
    //message(WARN, "failed to get Rect : obj type isn't array");
    int val, i=0;
    for (auto iter=src->array->begin(); iter!=src->array->end(); iter++, i++){
        PdfObject *obj = (*iter);
        if (!isInt(obj) && !isReal(obj)){
            message(WARN,"failed to get Rect : array item isn't number");
            continue;
        }
        val = round( isReal(obj) ? (obj->real) : (obj->integer));
        switch (i) {
            case 0:
                left.x = val;
                break;
            case 1:
                left.y = val;
                break;
            case 2:
                right.x = val;
                break;
            case 3:
                right.y = val;
                break;
            default:
                message(WARN,"wrong boundaries");
        }
    }
    if (i<4){
        message(FATAL, "wrong boundaries");//todo warn only
    }
    return true;
}

bool Rect:: setToObject(PdfObject *dst)
{
    if (dst==NULL) return false;

    char *str;

    asprintf(&str,"[ %f %f %f %f ]", left.x, left.y, right.x, right.y);
    dst->clear();//free previous data (if any)
    assert (dst->readFromString(str));
    free(str);
    return true;
}



Matrix:: Matrix() : Matrix(1,0,0, 0,1,0, 0,0,1)
{
}

Matrix:: Matrix(float m00, float m01, float m02,
                float m10, float m11, float m12,
                float m20, float m21, float m22)
{
    mat[0][0]=m00; mat[0][1]=m01; mat[0][2]=m02;
    mat[1][0]=m10; mat[1][1]=m11; mat[1][2]=m12;
    mat[2][0]=m20; mat[2][1]=m21; mat[2][2]=m22;
}

bool Matrix:: isIdentity()
{
    if (   mat[0][0]==1 && mat[0][1]==0 && mat[0][2]==0
        && mat[1][0]==0 && mat[1][1]==1 && mat[1][2]==0
        && mat[2][0]==0 && mat[2][1]==0 && mat[2][2]==1) {
        return true;
    }
    return false;
}

// A.B = AB
void Matrix:: multiply (const Matrix &B)
{
    Matrix AB;
    for (int i=0; i<3; ++i){//each row in 1st matrix
        for (int j=0; j<3; ++j){// each col in 2nd matrix
            AB.mat[i][j] = 0;
            for (int k=0; k<3; ++k){
                AB.mat[i][j] += mat[i][k] * B.mat[k][j];
            }
        }
    }
    memcpy(&mat, &AB.mat, sizeof(mat));
}

// transformation order : scale -> rotate -> translate
void Matrix:: scale (float scale)
{
    Matrix matrix(scale,0,0, 0,scale,0, 0,0,1);
    this->multiply(matrix);
}

void Matrix:: rotate (float angle_deg)
{
    // rounding off so that value of cos90 becomes zero instead of 6.12323e-17
    float sinx = sin((angle_deg*PI)/180.0);
    float cosx = cos((angle_deg*PI)/180.0);
    Matrix matrix(cosx,-sinx,0, sinx,cosx,0, 0,0,1);// rotation along z axis
    this->multiply(matrix);
}

void Matrix:: translate (float dx, float dy)
{
    Matrix matrix(1,0,0, 0,1,0, dx,dy,1);
    this->multiply(matrix);
}

void Matrix:: transform (Point &point)
{
    float x = point.x;
    float y = point.y;
    point.x = x* mat[0][0] + y* mat[1][0] + mat[2][0];
    point.y = x* mat[0][1] + y* mat[1][1] + mat[2][1];
}

/* apply transformation matrix, then set min coordinate as bottom left
 and max coordinate as top right coordinate
 _________
|      p2 |
|         |
| p1      |
*/
void Matrix:: transform (Rect &dim)
{
    Point p1 = dim.left;
    Point p2 = dim.right;
    // need to transform 2 points if only multiple of 90 deg rotation allowed
    transform(p1);
    transform(p2);

    dim.left = min_coordinate(p1, p2);
    dim.right = max_coordinate(p1, p2);
}


static Point max_coordinate (Point p1, Point p2)
{
    p1.x = MAX(p1.x, p2.x);
    p1.y = MAX(p1.y, p2.y);
    return p1;
}

static Point min_coordinate (Point p1, Point p2)
{
    p1.x = MIN(p1.x, p2.x);
    p1.y = MIN(p1.y, p2.y);
    return p1;
}

/*
Rect max_dimension (Rect d1, Rect d2)
{
    if (d1.isZero()) {
        return d2;
    }
    if (d2.isZero()) {
        return d1;
    }
    d1.left = min_coordinate(d1.left, d2.left);
    d1.right = max_coordinate(d1.right, d2.right);
    return d1;
}
*/
