/**
 * PC implementation of the GameCube MTX (matrix/vector/quaternion) library.
 * Replaces all PSMTX/PSVEC/PSQUAT paired-single assembly with standard C math.
 * Also provides the C_MTX/C_VEC/C_QUAT implementations.
 */

#include "dolphin/mtx.h"
#include <math.h>
#include <string.h>

/* ======================================================================== */
/*  MTX 3x4 Functions                                                       */
/* ======================================================================== */

void C_MTXIdentity(Mtx m) {
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
}

void PSMTXIdentity(Mtx m) {
    C_MTXIdentity(m);
}

void C_MTXCopy(const Mtx src, Mtx dst) {
    if (src == dst) return;
    dst[0][0] = src[0][0]; dst[0][1] = src[0][1]; dst[0][2] = src[0][2]; dst[0][3] = src[0][3];
    dst[1][0] = src[1][0]; dst[1][1] = src[1][1]; dst[1][2] = src[1][2]; dst[1][3] = src[1][3];
    dst[2][0] = src[2][0]; dst[2][1] = src[2][1]; dst[2][2] = src[2][2]; dst[2][3] = src[2][3];
}

void PSMTXCopy(const Mtx src, Mtx dst) {
    C_MTXCopy(src, dst);
}

void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab) {
    Mtx mTmp;
    MtxPtr m;

    if ((ab == a) || (ab == b)) {
        m = mTmp;
    } else {
        m = ab;
    }

    m[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0];
    m[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1];
    m[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2];
    m[0][3] = a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3];

    m[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0];
    m[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1];
    m[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2];
    m[1][3] = a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3];

    m[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0];
    m[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1];
    m[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2];
    m[2][3] = a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3];

    if (m == mTmp) {
        C_MTXCopy(mTmp, ab);
    }
}

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) {
    C_MTXConcat(a, b, ab);
}

void C_MTXConcatArray(const Mtx a, const Mtx *srcBase, Mtx *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        C_MTXConcat(a, *srcBase, *dstBase);
        srcBase++;
        dstBase++;
    }
}

void PSMTXConcatArray(const Mtx a, const Mtx *srcBase, Mtx *dstBase, u32 count) {
    C_MTXConcatArray(a, srcBase, dstBase, count);
}

void C_MTXTranspose(const Mtx src, Mtx xPose) {
    Mtx mTmp;
    MtxPtr m;

    if (src == xPose) {
        m = mTmp;
    } else {
        m = xPose;
    }

    m[0][0] = src[0][0]; m[0][1] = src[1][0]; m[0][2] = src[2][0]; m[0][3] = 0.0f;
    m[1][0] = src[0][1]; m[1][1] = src[1][1]; m[1][2] = src[2][1]; m[1][3] = 0.0f;
    m[2][0] = src[0][2]; m[2][1] = src[1][2]; m[2][2] = src[2][2]; m[2][3] = 0.0f;

    if (m == mTmp) {
        C_MTXCopy(mTmp, xPose);
    }
}

void PSMTXTranspose(const Mtx src, Mtx xPose) {
    C_MTXTranspose(src, xPose);
}

u32 C_MTXInverse(const Mtx src, Mtx inv) {
    Mtx mTmp;
    MtxPtr m;
    f32 det;

    if (src == inv) {
        m = mTmp;
    } else {
        m = inv;
    }

    det = src[0][0] * src[1][1] * src[2][2]
        + src[0][1] * src[1][2] * src[2][0]
        + src[0][2] * src[1][0] * src[2][1]
        - src[2][0] * src[1][1] * src[0][2]
        - src[1][0] * src[0][1] * src[2][2]
        - src[0][0] * src[2][1] * src[1][2];

    if (det == 0.0f) return 0;

    det = 1.0f / det;

    m[0][0] =  (src[1][1] * src[2][2] - src[2][1] * src[1][2]) * det;
    m[0][1] = -(src[0][1] * src[2][2] - src[2][1] * src[0][2]) * det;
    m[0][2] =  (src[0][1] * src[1][2] - src[1][1] * src[0][2]) * det;

    m[1][0] = -(src[1][0] * src[2][2] - src[2][0] * src[1][2]) * det;
    m[1][1] =  (src[0][0] * src[2][2] - src[2][0] * src[0][2]) * det;
    m[1][2] = -(src[0][0] * src[1][2] - src[1][0] * src[0][2]) * det;

    m[2][0] =  (src[1][0] * src[2][1] - src[2][0] * src[1][1]) * det;
    m[2][1] = -(src[0][0] * src[2][1] - src[2][0] * src[0][1]) * det;
    m[2][2] =  (src[0][0] * src[1][1] - src[1][0] * src[0][1]) * det;

    m[0][3] = -m[0][0] * src[0][3] - m[0][1] * src[1][3] - m[0][2] * src[2][3];
    m[1][3] = -m[1][0] * src[0][3] - m[1][1] * src[1][3] - m[1][2] * src[2][3];
    m[2][3] = -m[2][0] * src[0][3] - m[2][1] * src[1][3] - m[2][2] * src[2][3];

    if (m == mTmp) {
        C_MTXCopy(mTmp, inv);
    }

    return 1;
}

u32 PSMTXInverse(const Mtx src, Mtx inv) {
    return C_MTXInverse(src, inv);
}

u32 C_MTXInvXpose(const Mtx src, Mtx invX) {
    Mtx mTmp;
    MtxPtr m;
    f32 det;

    if (src == invX) {
        m = mTmp;
    } else {
        m = invX;
    }

    det = src[0][0] * src[1][1] * src[2][2]
        + src[0][1] * src[1][2] * src[2][0]
        + src[0][2] * src[1][0] * src[2][1]
        - src[2][0] * src[1][1] * src[0][2]
        - src[1][0] * src[0][1] * src[2][2]
        - src[0][0] * src[2][1] * src[1][2];

    if (det == 0.0f) return 0;

    det = 1.0f / det;

    m[0][0] =  (src[1][1] * src[2][2] - src[2][1] * src[1][2]) * det;
    m[0][1] = -(src[1][0] * src[2][2] - src[2][0] * src[1][2]) * det;
    m[0][2] =  (src[1][0] * src[2][1] - src[2][0] * src[1][1]) * det;

    m[1][0] = -(src[0][1] * src[2][2] - src[2][1] * src[0][2]) * det;
    m[1][1] =  (src[0][0] * src[2][2] - src[2][0] * src[0][2]) * det;
    m[1][2] = -(src[0][0] * src[2][1] - src[2][0] * src[0][1]) * det;

    m[2][0] =  (src[0][1] * src[1][2] - src[1][1] * src[0][2]) * det;
    m[2][1] = -(src[0][0] * src[1][2] - src[1][0] * src[0][2]) * det;
    m[2][2] =  (src[0][0] * src[1][1] - src[1][0] * src[0][1]) * det;

    m[0][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][3] = 0.0f;

    if (m == mTmp) {
        C_MTXCopy(mTmp, invX);
    }

    return 1;
}

u32 PSMTXInvXpose(const Mtx src, Mtx invX) {
    return C_MTXInvXpose(src, invX);
}

/* ======================================================================== */
/*  MTX Rotation                                                            */
/* ======================================================================== */

void C_MTXRotRad(Mtx m, char axis, f32 rad) {
    f32 sinA = sinf(rad);
    f32 cosA = cosf(rad);
    C_MTXRotTrig(m, axis, sinA, cosA);
}

void PSMTXRotRad(Mtx m, char axis, f32 rad) {
    C_MTXRotRad(m, axis, rad);
}

void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA) {
    switch (axis) {
        case 'x':
        case 'X':
            m[0][0] = 1.0f; m[0][1] = 0.0f;  m[0][2] = 0.0f;  m[0][3] = 0.0f;
            m[1][0] = 0.0f; m[1][1] = cosA;   m[1][2] = -sinA; m[1][3] = 0.0f;
            m[2][0] = 0.0f; m[2][1] = sinA;   m[2][2] = cosA;  m[2][3] = 0.0f;
            break;
        case 'y':
        case 'Y':
            m[0][0] = cosA;  m[0][1] = 0.0f; m[0][2] = sinA;  m[0][3] = 0.0f;
            m[1][0] = 0.0f;  m[1][1] = 1.0f; m[1][2] = 0.0f;  m[1][3] = 0.0f;
            m[2][0] = -sinA; m[2][1] = 0.0f; m[2][2] = cosA;  m[2][3] = 0.0f;
            break;
        case 'z':
        case 'Z':
            m[0][0] = cosA;  m[0][1] = -sinA; m[0][2] = 0.0f; m[0][3] = 0.0f;
            m[1][0] = sinA;  m[1][1] = cosA;  m[1][2] = 0.0f; m[1][3] = 0.0f;
            m[2][0] = 0.0f;  m[2][1] = 0.0f;  m[2][2] = 1.0f; m[2][3] = 0.0f;
            break;
        default:
            break;
    }
}

void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA) {
    C_MTXRotTrig(m, axis, sinA, cosA);
}

void C_MTXRotAxisRad(Mtx m, const Vec *axis, f32 rad) {
    Vec vN;
    f32 s, c, t;
    f32 x, y, z;
    f32 xSq, ySq, zSq;

    s = sinf(rad);
    c = cosf(rad);
    t = 1.0f - c;

    C_VECNormalize(axis, &vN);

    x = vN.x;
    y = vN.y;
    z = vN.z;

    xSq = x * x;
    ySq = y * y;
    zSq = z * z;

    m[0][0] = (t * xSq) + c;
    m[0][1] = (t * x * y) - (s * z);
    m[0][2] = (t * x * z) + (s * y);
    m[0][3] = 0.0f;

    m[1][0] = (t * x * y) + (s * z);
    m[1][1] = (t * ySq) + c;
    m[1][2] = (t * y * z) - (s * x);
    m[1][3] = 0.0f;

    m[2][0] = (t * x * z) - (s * y);
    m[2][1] = (t * y * z) + (s * x);
    m[2][2] = (t * zSq) + c;
    m[2][3] = 0.0f;
}

void PSMTXRotAxisRad(Mtx m, const Vec *axis, f32 rad) {
    C_MTXRotAxisRad(m, axis, rad);
}

/* ======================================================================== */
/*  MTX Translation / Scale                                                 */
/* ======================================================================== */

void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT) {
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = xT;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = yT;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = zT;
}

void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT) {
    C_MTXTrans(m, xT, yT, zT);
}

void C_MTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT) {
    if (src != dst) {
        dst[0][0] = src[0][0]; dst[0][1] = src[0][1]; dst[0][2] = src[0][2];
        dst[1][0] = src[1][0]; dst[1][1] = src[1][1]; dst[1][2] = src[1][2];
        dst[2][0] = src[2][0]; dst[2][1] = src[2][1]; dst[2][2] = src[2][2];
    }
    dst[0][3] = src[0][3] + xT;
    dst[1][3] = src[1][3] + yT;
    dst[2][3] = src[2][3] + zT;
}

void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT) {
    C_MTXTransApply(src, dst, xT, yT, zT);
}

void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS) {
    m[0][0] = xS;   m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = yS;   m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = zS;   m[2][3] = 0.0f;
}

void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS) {
    C_MTXScale(m, xS, yS, zS);
}

void C_MTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS) {
    dst[0][0] = src[0][0] * xS; dst[0][1] = src[0][1] * xS;
    dst[0][2] = src[0][2] * xS; dst[0][3] = src[0][3] * xS;
    dst[1][0] = src[1][0] * yS; dst[1][1] = src[1][1] * yS;
    dst[1][2] = src[1][2] * yS; dst[1][3] = src[1][3] * yS;
    dst[2][0] = src[2][0] * zS; dst[2][1] = src[2][1] * zS;
    dst[2][2] = src[2][2] * zS; dst[2][3] = src[2][3] * zS;
}

void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS) {
    C_MTXScaleApply(src, dst, xS, yS, zS);
}

/* ======================================================================== */
/*  MTX Quaternion / Reflect                                                */
/* ======================================================================== */

void C_MTXQuat(Mtx m, const Quaternion *q) {
    f32 s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;
    s = 2.0f / ((q->x * q->x) + (q->y * q->y) + (q->z * q->z) + (q->w * q->w));

    xs = q->x * s;  ys = q->y * s;  zs = q->z * s;
    wx = q->w * xs;  wy = q->w * ys;  wz = q->w * zs;
    xx = q->x * xs;  xy = q->x * ys;  xz = q->x * zs;
    yy = q->y * ys;  yz = q->y * zs;  zz = q->z * zs;

    m[0][0] = 1.0f - (yy + zz); m[0][1] = xy - wz;           m[0][2] = xz + wy;           m[0][3] = 0.0f;
    m[1][0] = xy + wz;          m[1][1] = 1.0f - (xx + zz);   m[1][2] = yz - wx;           m[1][3] = 0.0f;
    m[2][0] = xz - wy;          m[2][1] = yz + wx;            m[2][2] = 1.0f - (xx + yy);   m[2][3] = 0.0f;
}

void PSMTXQuat(Mtx m, const Quaternion *q) {
    C_MTXQuat(m, q);
}

void C_MTXReflect(Mtx m, const Vec *p, const Vec *n) {
    f32 vxy, vxz, vyz, pdotn;

    vxy = -2.0f * n->x * n->y;
    vxz = -2.0f * n->x * n->z;
    vyz = -2.0f * n->y * n->z;
    pdotn = 2.0f * C_VECDotProduct(p, n);

    m[0][0] = 1.0f - 2.0f * n->x * n->x; m[0][1] = vxy;                          m[0][2] = vxz;                          m[0][3] = pdotn * n->x;
    m[1][0] = vxy;                          m[1][1] = 1.0f - 2.0f * n->y * n->y; m[1][2] = vyz;                          m[1][3] = pdotn * n->y;
    m[2][0] = vxz;                          m[2][1] = vyz;                          m[2][2] = 1.0f - 2.0f * n->z * n->z; m[2][3] = pdotn * n->z;
}

void PSMTXReflect(Mtx m, const Vec *p, const Vec *n) {
    C_MTXReflect(m, p, n);
}

/* ======================================================================== */
/*  MTX MultVec                                                             */
/* ======================================================================== */

void C_MTXMultVec(const Mtx m, const Vec *src, Vec *dst) {
    Vec tmp;
    tmp.x = m[0][0] * src->x + m[0][1] * src->y + m[0][2] * src->z + m[0][3];
    tmp.y = m[1][0] * src->x + m[1][1] * src->y + m[1][2] * src->z + m[1][3];
    tmp.z = m[2][0] * src->x + m[2][1] * src->y + m[2][2] * src->z + m[2][3];
    *dst = tmp;
}

void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst) {
    C_MTXMultVec(m, src, dst);
}

void C_MTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        C_MTXMultVec(m, &srcBase[i], &dstBase[i]);
    }
}

void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count) {
    C_MTXMultVecArray(m, srcBase, dstBase, count);
}

void C_MTXMultVecSR(const Mtx m, const Vec *src, Vec *dst) {
    Vec tmp;
    tmp.x = m[0][0] * src->x + m[0][1] * src->y + m[0][2] * src->z;
    tmp.y = m[1][0] * src->x + m[1][1] * src->y + m[1][2] * src->z;
    tmp.z = m[2][0] * src->x + m[2][1] * src->y + m[2][2] * src->z;
    *dst = tmp;
}

void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst) {
    C_MTXMultVecSR(m, src, dst);
}

void C_MTXMultVecArraySR(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        C_MTXMultVecSR(m, &srcBase[i], &dstBase[i]);
    }
}

void PSMTXMultVecArraySR(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count) {
    C_MTXMultVecArraySR(m, srcBase, dstBase, count);
}

/* ======================================================================== */
/*  MTX LookAt / Projection                                                 */
/* ======================================================================== */

void C_MTXLookAt(Mtx m, const Point3d *camPos, const Vec *camUp, const Point3d *target) {
    Vec vLook, vRight, vUp;

    vLook.x = camPos->x - target->x;
    vLook.y = camPos->y - target->y;
    vLook.z = camPos->z - target->z;
    C_VECNormalize(&vLook, &vLook);
    C_VECCrossProduct(camUp, &vLook, &vRight);
    C_VECNormalize(&vRight, &vRight);
    C_VECCrossProduct(&vLook, &vRight, &vUp);

    m[0][0] = vRight.x;
    m[0][1] = vRight.y;
    m[0][2] = vRight.z;
    m[0][3] = -(camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z);

    m[1][0] = vUp.x;
    m[1][1] = vUp.y;
    m[1][2] = vUp.z;
    m[1][3] = -(camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z);

    m[2][0] = vLook.x;
    m[2][1] = vLook.y;
    m[2][2] = vLook.z;
    m[2][3] = -(camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z);
}

void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) {
    f32 tmp;
    tmp = 1.0f / (r - l);
    m[0][0] = (2 * n) * tmp;
    m[0][1] = 0.0f;
    m[0][2] = (r + l) * tmp;
    m[0][3] = 0.0f;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = (2 * n) * tmp;
    m[1][2] = (t + b) * tmp;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n) * tmp;
    m[2][3] = -(f * n) * tmp;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f) {
    f32 angle = fovY * 0.5f;
    f32 cot, tmp;
    angle = MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);
    m[0][0] = cot / aspect;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = cot;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n) * tmp;
    m[2][3] = -(f * n) * tmp;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) {
    f32 tmp;
    tmp = 1.0f / (r - l);
    m[0][0] = 2.0f * tmp;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = -(r + l) * tmp;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = 2.0f * tmp;
    m[1][2] = 0.0f;
    m[1][3] = -(t + b) * tmp;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(1.0f) * tmp;
    m[2][3] = -(f) * tmp;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

/* ======================================================================== */
/*  MTX Light Projection                                                    */
/* ======================================================================== */

void C_MTXLightFrustum(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n,
                        f32 scaleS, f32 scaleT, f32 transS, f32 transT) {
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = ((2 * n) * tmp) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = (((r + l) * tmp) * scaleS) - transS;
    m[0][3] = 0.0f;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = ((2 * n) * tmp) * scaleT;
    m[1][2] = (((t + b) * tmp) * scaleT) - transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}

void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS,
                            f32 scaleT, f32 transS, f32 transT) {
    f32 angle, cot;

    angle = fovY * 0.5f;
    angle = MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);

    m[0][0] = (cot / aspect) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = -transS;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot * scaleT;
    m[1][2] = -transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}

void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r,
                      f32 scaleS, f32 scaleT, f32 transS, f32 transT) {
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = (2.0f * tmp * scaleS);
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = ((-(r + l) * tmp) * scaleS) + transS;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = (2.0f * tmp) * scaleT;
    m[1][2] = 0.0f;
    m[1][3] = ((-(t + b) * tmp) * scaleT) + transT;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 0.0f;
    m[2][3] = 1.0f;
}

/* ======================================================================== */
/*  VEC Functions                                                           */
/* ======================================================================== */

void C_VECAdd(const Vec *a, const Vec *b, Vec *ab) {
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

void PSVECAdd(const Vec *a, const Vec *b, Vec *ab) {
    C_VECAdd(a, b, ab);
}

void C_VECSubtract(const Vec *a, const Vec *b, Vec *a_b) {
    a_b->x = a->x - b->x;
    a_b->y = a->y - b->y;
    a_b->z = a->z - b->z;
}

void PSVECSubtract(const Vec *a, const Vec *b, Vec *a_b) {
    C_VECSubtract(a, b, a_b);
}

void C_VECScale(const Vec *src, Vec *dst, f32 scale) {
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

void PSVECScale(const Vec *src, Vec *dst, f32 scale) {
    C_VECScale(src, dst, scale);
}

void C_VECNormalize(const Vec *src, Vec *unit) {
    f32 mag = sqrtf(src->x * src->x + src->y * src->y + src->z * src->z);
    if (mag != 0.0f) {
        f32 inv = 1.0f / mag;
        unit->x = src->x * inv;
        unit->y = src->y * inv;
        unit->z = src->z * inv;
    }
}

void PSVECNormalize(const Vec *src, Vec *unit) {
    C_VECNormalize(src, unit);
}

f32 C_VECSquareMag(const Vec *v) {
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

f32 PSVECSquareMag(const Vec *v) {
    return C_VECSquareMag(v);
}

f32 C_VECMag(const Vec *v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

f32 PSVECMag(const Vec *v) {
    return C_VECMag(v);
}

f32 C_VECDotProduct(const Vec *a, const Vec *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

f32 PSVECDotProduct(const Vec *a, const Vec *b) {
    return C_VECDotProduct(a, b);
}

void C_VECCrossProduct(const Vec *a, const Vec *b, Vec *axb) {
    Vec tmp;
    tmp.x = a->y * b->z - a->z * b->y;
    tmp.y = a->z * b->x - a->x * b->z;
    tmp.z = a->x * b->y - a->y * b->x;
    *axb = tmp;
}

void PSVECCrossProduct(const Vec *a, const Vec *b, Vec *axb) {
    C_VECCrossProduct(a, b, axb);
}

f32 C_VECSquareDistance(const Vec *a, const Vec *b) {
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

f32 PSVECSquareDistance(const Vec *a, const Vec *b) {
    return C_VECSquareDistance(a, b);
}

f32 C_VECDistance(const Vec *a, const Vec *b) {
    return sqrtf(C_VECSquareDistance(a, b));
}

f32 PSVECDistance(const Vec *a, const Vec *b) {
    return C_VECDistance(a, b);
}

void C_VECReflect(const Vec *src, const Vec *normal, Vec *dst) {
    Vec a0, b0;
    f32 dot;

    a0.x = -src->x;
    a0.y = -src->y;
    a0.z = -src->z;

    C_VECNormalize(&a0, &a0);
    C_VECNormalize(normal, &b0);

    dot = C_VECDotProduct(&a0, &b0);
    dst->x = b0.x * 2.0f * dot - a0.x;
    dst->y = b0.y * 2.0f * dot - a0.y;
    dst->z = b0.z * 2.0f * dot - a0.z;

    C_VECNormalize(dst, dst);
}

void C_VECHalfAngle(const Vec *a, const Vec *b, Vec *half) {
    Vec a0, b0, ab;

    a0.x = -a->x; a0.y = -a->y; a0.z = -a->z;
    b0.x = -b->x; b0.y = -b->y; b0.z = -b->z;

    C_VECNormalize(&a0, &a0);
    C_VECNormalize(&b0, &b0);
    C_VECAdd(&a0, &b0, &ab);

    if (C_VECDotProduct(&ab, &ab) > 0.0f) {
        C_VECNormalize(&ab, half);
    } else {
        *half = ab;
    }
}

/* ======================================================================== */
/*  Quaternion Functions                                                    */
/* ======================================================================== */

void C_QUATAdd(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    r->x = p->x + q->x;
    r->y = p->y + q->y;
    r->z = p->z + q->z;
    r->w = p->w + q->w;
}

void PSQUATAdd(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    C_QUATAdd(p, q, r);
}

void C_QUATSubtract(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    r->x = p->x - q->x;
    r->y = p->y - q->y;
    r->z = p->z - q->z;
    r->w = p->w - q->w;
}

void PSQUATSubtract(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    C_QUATSubtract(p, q, r);
}

void C_QUATMultiply(const Quaternion *p, const Quaternion *q, Quaternion *pq) {
    Quaternion tmp;
    tmp.x = p->w * q->x + p->x * q->w + p->y * q->z - p->z * q->y;
    tmp.y = p->w * q->y - p->x * q->z + p->y * q->w + p->z * q->x;
    tmp.z = p->w * q->z + p->x * q->y - p->y * q->x + p->z * q->w;
    tmp.w = p->w * q->w - p->x * q->x - p->y * q->y - p->z * q->z;
    *pq = tmp;
}

void PSQUATMultiply(const Quaternion *p, const Quaternion *q, Quaternion *pq) {
    C_QUATMultiply(p, q, pq);
}

void C_QUATDivide(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    Quaternion qInv;
    C_QUATInverse(q, &qInv);
    C_QUATMultiply(p, &qInv, r);
}

void PSQUATDivide(const Quaternion *p, const Quaternion *q, Quaternion *r) {
    C_QUATDivide(p, q, r);
}

void C_QUATScale(const Quaternion *q, Quaternion *r, f32 scale) {
    r->x = q->x * scale;
    r->y = q->y * scale;
    r->z = q->z * scale;
    r->w = q->w * scale;
}

void PSQUATScale(const Quaternion *q, Quaternion *r, f32 scale) {
    C_QUATScale(q, r, scale);
}

f32 C_QUATDotProduct(const Quaternion *p, const Quaternion *q) {
    return p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
}

f32 PSQUATDotProduct(const Quaternion *p, const Quaternion *q) {
    return C_QUATDotProduct(p, q);
}

void C_QUATNormalize(const Quaternion *src, Quaternion *unit) {
    f32 mag = sqrtf(src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w);
    if (mag > 0.00001f) {
        f32 inv = 1.0f / mag;
        unit->x = src->x * inv;
        unit->y = src->y * inv;
        unit->z = src->z * inv;
        unit->w = src->w * inv;
    }
}

void PSQUATNormalize(const Quaternion *src, Quaternion *unit) {
    C_QUATNormalize(src, unit);
}

void C_QUATInverse(const Quaternion *src, Quaternion *inv) {
    f32 dot = src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w;
    if (dot > 0.0f) {
        f32 invDot = 1.0f / dot;
        inv->x = -src->x * invDot;
        inv->y = -src->y * invDot;
        inv->z = -src->z * invDot;
        inv->w =  src->w * invDot;
    }
}

void PSQUATInverse(const Quaternion *src, Quaternion *inv) {
    C_QUATInverse(src, inv);
}

void C_QUATExp(const Quaternion *q, Quaternion *r) {
    f32 theta = sqrtf(q->x * q->x + q->y * q->y + q->z * q->z);
    if (theta > 0.00001f) {
        f32 sinTheta = sinf(theta) / theta;
        r->x = sinTheta * q->x;
        r->y = sinTheta * q->y;
        r->z = sinTheta * q->z;
        r->w = cosf(theta);
    } else {
        r->x = 0.0f;
        r->y = 0.0f;
        r->z = 0.0f;
        r->w = 1.0f;
    }
}

void C_QUATLogN(const Quaternion *q, Quaternion *r) {
    f32 theta = acosf(q->w);
    if (theta > 0.00001f) {
        f32 invSinTheta = theta / sinf(theta);
        r->x = invSinTheta * q->x;
        r->y = invSinTheta * q->y;
        r->z = invSinTheta * q->z;
    } else {
        r->x = 0.0f;
        r->y = 0.0f;
        r->z = 0.0f;
    }
    r->w = 0.0f;
}

void C_QUATMakeClosest(const Quaternion *q, const Quaternion *qto, Quaternion *r) {
    if (C_QUATDotProduct(q, qto) < 0.0f) {
        r->x = -q->x;
        r->y = -q->y;
        r->z = -q->z;
        r->w = -q->w;
    } else if (r != q) {
        r->x = q->x;
        r->y = q->y;
        r->z = q->z;
        r->w = q->w;
    }
}

void C_QUATRotAxisRad(Quaternion *q, const Vec *axis, f32 rad) {
    Vec dst;
    f32 halfRad, sinHalf, cosHalf;

    C_VECNormalize(axis, &dst);

    halfRad = rad * 0.5f;
    sinHalf = sinf(halfRad);
    cosHalf = cosf(halfRad);

    q->x = sinHalf * dst.x;
    q->y = sinHalf * dst.y;
    q->z = sinHalf * dst.z;
    q->w = cosHalf;
}

void C_QUATMtx(Quaternion *r, const Mtx m) {
    f32 vv0, vv1;
    s32 i, j, k;
    s32 idx[3] = { 1, 2, 0 };
    f32 vec[3];

    vv0 = m[0][0] + m[1][1] + m[2][2];
    if (vv0 > 0.0f) {
        vv1 = sqrtf(vv0 + 1.0f);
        r->w = vv1 * 0.5f;
        vv1 = 0.5f / vv1;
        r->x = (m[2][1] - m[1][2]) * vv1;
        r->y = (m[0][2] - m[2][0]) * vv1;
        r->z = (m[1][0] - m[0][1]) * vv1;
    } else {
        i = 0;
        if (m[1][1] > m[0][0]) i = 1;
        if (m[2][2] > m[i][i]) i = 2;
        j = idx[i];
        k = idx[j];
        vv1 = sqrtf((m[i][i] - (m[j][j] + m[k][k])) + 1.0f);
        vec[i] = vv1 * 0.5f;
        if (vv1 != 0.0f)
            vv1 = 0.5f / vv1;
        r->w = (m[k][j] - m[j][k]) * vv1;
        vec[j] = (m[i][j] + m[j][i]) * vv1;
        vec[k] = (m[i][k] + m[k][i]) * vv1;
        r->x = vec[0];
        r->y = vec[1];
        r->z = vec[2];
    }
}

void C_QUATLerp(const Quaternion *p, const Quaternion *q, Quaternion *r, f32 t) {
    r->x = p->x + t * (q->x - p->x);
    r->y = p->y + t * (q->y - p->y);
    r->z = p->z + t * (q->z - p->z);
    r->w = p->w + t * (q->w - p->w);
}

void C_QUATSlerp(const Quaternion *p, const Quaternion *q, Quaternion *r, f32 t) {
    f32 ratioA, ratioB;
    f32 value = 1.0f;
    f32 cosHalfTheta = p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;

    if (cosHalfTheta < 0.0f) {
        cosHalfTheta = -cosHalfTheta;
        value = -value;
    }

    if (cosHalfTheta <= 0.9999899864196777f) {
        f32 halfTheta = acosf(cosHalfTheta);
        f32 sinHalfTheta = sinf(halfTheta);

        ratioA = sinf((1.0f - t) * halfTheta) / sinHalfTheta;
        ratioB = sinf(t * halfTheta) / sinHalfTheta;
        value *= ratioB;
    } else {
        ratioA = 1.0f - t;
        value *= t;
    }

    r->x = (ratioA * p->x) + (value * q->x);
    r->y = (ratioA * p->y) + (value * q->y);
    r->z = (ratioA * p->z) + (value * q->z);
    r->w = (ratioA * p->w) + (value * q->w);
}

void C_QUATSquad(const Quaternion *p, const Quaternion *a, const Quaternion *b,
                 const Quaternion *q, Quaternion *r, f32 t) {
    Quaternion tmp1, tmp2;
    C_QUATSlerp(p, q, &tmp1, t);
    C_QUATSlerp(a, b, &tmp2, t);
    C_QUATSlerp(&tmp1, &tmp2, r, 2.0f * t * (1.0f - t));
}

void C_QUATCompA(const Quaternion *qprev, const Quaternion *q, const Quaternion *qnext,
                 Quaternion *a) {
    Quaternion qInv, tmp1, tmp2, logSum, expResult;

    C_QUATInverse(q, &qInv);

    C_QUATMultiply(&qInv, qnext, &tmp1);
    C_QUATLogN(&tmp1, &tmp1);

    C_QUATMultiply(&qInv, qprev, &tmp2);
    C_QUATLogN(&tmp2, &tmp2);

    logSum.x = -(tmp1.x + tmp2.x) * 0.25f;
    logSum.y = -(tmp1.y + tmp2.y) * 0.25f;
    logSum.z = -(tmp1.z + tmp2.z) * 0.25f;
    logSum.w = -(tmp1.w + tmp2.w) * 0.25f;

    C_QUATExp(&logSum, &expResult);
    C_QUATMultiply(q, &expResult, a);
}

/* ======================================================================== */
/*  MTX44 Functions                                                         */
/* ======================================================================== */

void PSMTX44Copy(Mtx44 src, Mtx44 dest) {
    memcpy(dest, src, sizeof(Mtx44));
}

/* ======================================================================== */
/*  PS-specific ROMtx / S16Vec functions (stubs for PC)                     */
/* ======================================================================== */

void PSMTXReorder(const Mtx src, ROMtx dest) {
    /* Transpose from Mtx[3][4] to ROMtx[4][3] layout */
    dest[0][0] = src[0][0]; dest[0][1] = src[1][0]; dest[0][2] = src[2][0];
    dest[1][0] = src[0][1]; dest[1][1] = src[1][1]; dest[1][2] = src[2][1];
    dest[2][0] = src[0][2]; dest[2][1] = src[1][2]; dest[2][2] = src[2][2];
    dest[3][0] = src[0][3]; dest[3][1] = src[1][3]; dest[3][2] = src[2][3];
}

void PSMTXROMultVecArray(const ROMtx m, const Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        Vec tmp;
        const Vec *s = &srcBase[i];
        tmp.x = m[0][0] * s->x + m[1][0] * s->y + m[2][0] * s->z + m[3][0];
        tmp.y = m[0][1] * s->x + m[1][1] * s->y + m[2][1] * s->z + m[3][1];
        tmp.z = m[0][2] * s->x + m[1][2] * s->y + m[2][2] * s->z + m[3][2];
        dstBase[i] = tmp;
    }
}

void PSMTXROSkin2VecArray(const ROMtx m0, const ROMtx m1, const f32 *wtBase,
                           const Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        f32 w = wtBase[i];
        f32 w0, w1;
        Vec v0, v1;
        const Vec *s = &srcBase[i];

        /* Blend between two matrices based on weight */
        w1 = w;
        w0 = 1.0f - w1;

        /* Transform by m0 */
        v0.x = m0[0][0] * s->x + m0[1][0] * s->y + m0[2][0] * s->z + m0[3][0];
        v0.y = m0[0][1] * s->x + m0[1][1] * s->y + m0[2][1] * s->z + m0[3][1];
        v0.z = m0[0][2] * s->x + m0[1][2] * s->y + m0[2][2] * s->z + m0[3][2];

        /* Transform by m1 */
        v1.x = m1[0][0] * s->x + m1[1][0] * s->y + m1[2][0] * s->z + m1[3][0];
        v1.y = m1[0][1] * s->x + m1[1][1] * s->y + m1[2][1] * s->z + m1[3][1];
        v1.z = m1[0][2] * s->x + m1[1][2] * s->y + m1[2][2] * s->z + m1[3][2];

        dstBase[i].x = w0 * v0.x + w1 * v1.x;
        dstBase[i].y = w0 * v0.y + w1 * v1.y;
        dstBase[i].z = w0 * v0.z + w1 * v1.z;
    }
}

void PSMTXMultS16VecArray(const Mtx m, const S16Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        f32 x = (f32)srcBase[i].x;
        f32 y = (f32)srcBase[i].y;
        f32 z = (f32)srcBase[i].z;
        dstBase[i].x = m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3];
        dstBase[i].y = m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3];
        dstBase[i].z = m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3];
    }
}

void PSMTXROMultS16VecArray(const ROMtx m, const S16Vec *srcBase, Vec *dstBase, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        f32 x = (f32)srcBase[i].x;
        f32 y = (f32)srcBase[i].y;
        f32 z = (f32)srcBase[i].z;
        dstBase[i].x = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
        dstBase[i].y = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
        dstBase[i].z = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
    }
}

/* ======================================================================== */
/*  MTX Stack Functions                                                     */
/* ======================================================================== */

void MTXInitStack(MtxStack *sPtr, u32 numMtx) {
    sPtr->numMtx = numMtx;
    sPtr->stackPtr = sPtr->stackBase;
    C_MTXIdentity(*sPtr->stackPtr);
}

MtxPtr MTXPush(MtxStack *sPtr, const Mtx m) {
    if ((u32)(sPtr->stackPtr - sPtr->stackBase) >= (sPtr->numMtx - 1)) {
        return NULL;
    }
    sPtr->stackPtr += MTX_PTR_OFFSET;
    C_MTXCopy(m, *sPtr->stackPtr);
    return sPtr->stackPtr;
}

MtxPtr MTXPushFwd(MtxStack *sPtr, const Mtx m) {
    if ((u32)(sPtr->stackPtr - sPtr->stackBase) >= (sPtr->numMtx - 1)) {
        return NULL;
    }
    sPtr->stackPtr += MTX_PTR_OFFSET;
    C_MTXConcat(*(sPtr->stackPtr - MTX_PTR_OFFSET), m, *sPtr->stackPtr);
    return sPtr->stackPtr;
}

MtxPtr MTXPushInv(MtxStack *sPtr, const Mtx m) {
    Mtx inv;
    if ((u32)(sPtr->stackPtr - sPtr->stackBase) >= (sPtr->numMtx - 1)) {
        return NULL;
    }
    if (!C_MTXInverse(m, inv)) {
        return NULL;
    }
    sPtr->stackPtr += MTX_PTR_OFFSET;
    C_MTXConcat(inv, *(sPtr->stackPtr - MTX_PTR_OFFSET), *sPtr->stackPtr);
    return sPtr->stackPtr;
}

MtxPtr MTXPushInvXpose(MtxStack *sPtr, const Mtx m) {
    Mtx invX;
    if ((u32)(sPtr->stackPtr - sPtr->stackBase) >= (sPtr->numMtx - 1)) {
        return NULL;
    }
    if (!C_MTXInvXpose(m, invX)) {
        return NULL;
    }
    sPtr->stackPtr += MTX_PTR_OFFSET;
    C_MTXConcat(invX, *(sPtr->stackPtr - MTX_PTR_OFFSET), *sPtr->stackPtr);
    return sPtr->stackPtr;
}

MtxPtr MTXPop(MtxStack *sPtr) {
    if (sPtr->stackPtr == sPtr->stackBase) {
        return NULL;
    }
    sPtr->stackPtr -= MTX_PTR_OFFSET;
    return sPtr->stackPtr;
}

MtxPtr MTXGetStackPtr(const MtxStack *sPtr) {
    return sPtr->stackPtr;
}
