#pragma once

#include <cmath>

namespace map_service_internal {

struct Vector3 {
    float x = 0, y = 0, z = 0;
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

inline Vector3 operator+(Vector3 a, const Vector3& b) { return a += b; }
inline Vector3 operator-(Vector3 a, const Vector3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline Vector3 operator*(Vector3 v, float s) { return Vector3(v.x * s, v.y * s, v.z * s); }
inline Vector3 operator*(float s, Vector3 v) { return v * s; }

inline float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vector3 Cross(const Vector3& a, const Vector3& b) {
    return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

struct Quaternion {
    float x = 0, y = 0, z = 0, w = 1;
    Quaternion() = default;
    Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

inline Vector3 operator*(const Quaternion& q, const Vector3& v) {
    float x = q.x * 2.0f, y = q.y * 2.0f, z = q.z * 2.0f;
    float xx = q.x * x, yy = q.y * y, zz = q.z * z;
    float xy = q.x * y, xz = q.x * z, yz = q.y * z;
    float wx = q.w * x, wy = q.w * y, wz = q.w * z;
    return Vector3(
        (1.0f - (yy + zz)) * v.x + (xy - wz) * v.y + (xz + wy) * v.z,
        (xy + wz) * v.x + (1.0f - (xx + zz)) * v.y + (yz - wx) * v.z,
        (xz - wy) * v.x + (yz + wx) * v.y + (1.0f - (xx + yy)) * v.z
    );
}

}
