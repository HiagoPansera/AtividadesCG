#pragma once
#include <cmath>

// ============================================================
// Vetor 3D
// ============================================================
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0.f, float y = 0.f, float z = 0.f) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
    Vec3 operator*(float s)       const { return Vec3(x*s,   y*s,   z*s);   }
    Vec3 cross(const Vec3& o)     const {
        return Vec3(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x);
    }
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 normalize() const {
        float l = sqrtf(x*x + y*y + z*z);
        return l > 1e-6f ? Vec3(x/l, y/l, z/l) : Vec3(0.f, 0.f, 0.f);
    }
};

// ============================================================
// Matriz 4x4 column-major (convencao OpenGL)
// ============================================================
struct Mat4 {
    float m[16];
    Mat4() { for (int i = 0; i < 16; i++) m[i] = 0.f; }
    static Mat4 identity() {
        Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r;
    }
};

inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                r.m[c*4+row] += a.m[k*4+row] * b.m[c*4+k];
    return r;
}

inline Mat4 translate(float tx, float ty, float tz) {
    Mat4 r = Mat4::identity();
    r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
    return r;
}

inline Mat4 scaleM(float s) {
    Mat4 r = Mat4::identity();
    r.m[0] = r.m[5] = r.m[10] = s;
    return r;
}

inline Mat4 scaleXYZ(float sx, float sy, float sz) {
    Mat4 r = Mat4::identity();
    r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
    return r;
}

inline Mat4 rotateX(float a) {
    Mat4 r = Mat4::identity();
    r.m[5]  =  cosf(a); r.m[9]  = -sinf(a);
    r.m[6]  =  sinf(a); r.m[10] =  cosf(a);
    return r;
}

inline Mat4 rotateY(float a) {
    Mat4 r = Mat4::identity();
    r.m[0]  =  cosf(a); r.m[8]  =  sinf(a);
    r.m[2]  = -sinf(a); r.m[10] =  cosf(a);
    return r;
}

inline Mat4 rotateZ(float a) {
    Mat4 r = Mat4::identity();
    r.m[0] =  cosf(a); r.m[4] = -sinf(a);
    r.m[1] =  sinf(a); r.m[5] =  cosf(a);
    return r;
}

inline Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
    Mat4 r;
    float f = 1.f / tanf(fovY * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.f;
    r.m[14] = (2.f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 worldUp) {
    Vec3 f = (center - eye).normalize();
    Vec3 r = f.cross(worldUp).normalize();
    Vec3 u = r.cross(f);
    Mat4 res;
    res.m[0]  =  r.x;  res.m[4]  =  r.y;  res.m[8]  =  r.z;  res.m[12] = -r.dot(eye);
    res.m[1]  =  u.x;  res.m[5]  =  u.y;  res.m[9]  =  u.z;  res.m[13] = -u.dot(eye);
    res.m[2]  = -f.x;  res.m[6]  = -f.y;  res.m[10] = -f.z;  res.m[14] =  f.dot(eye);
    res.m[15] = 1.f;
    return res;
}

// ============================================================
// Classe Camera - primeira pessoa
// ============================================================
class Camera {
public:
    Vec3  position;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Vec3 front, right, up;

    Camera(Vec3 pos      = Vec3(0.f, 0.f, 5.f),
           float yaw     = -90.f,
           float pitch   =   0.f,
           float speed   =   4.f,
           float sens    =   0.1f)
        : position(pos), yaw(yaw), pitch(pitch), speed(speed), sensitivity(sens)
    {
        updateVectors();
    }

    void move(float forward, float strafe, float vertical, float dt) {
        position = position + front                * (forward  * speed * dt);
        position = position + right                * (strafe   * speed * dt);
        position = position + Vec3(0.f, 1.f, 0.f) * (vertical * speed * dt);
    }

    void rotate(float dx, float dy) {
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        if (pitch >  89.f) pitch =  89.f;
        if (pitch < -89.f) pitch = -89.f;
        updateVectors();
    }

    Mat4 getViewMatrix() const {
        return lookAt(position, position + front, Vec3(0.f, 1.f, 0.f));
    }

private:
    void updateVectors() {
        const float PI = 3.14159265f;
        float yR = yaw   * PI / 180.f;
        float pR = pitch * PI / 180.f;
        front = Vec3(
            cosf(pR) * cosf(yR),
            sinf(pR),
            cosf(pR) * sinf(yR)
        ).normalize();
        right = front.cross(Vec3(0.f, 1.f, 0.f)).normalize();
        up    = right.cross(front);
    }
};
