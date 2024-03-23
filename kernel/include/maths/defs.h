#ifndef MATHS_DEFS_H_INCLUDED
#define MATHS_DEFS_H_INCLUDED

// +---------+
// | VECTORS |
// +---------+

typedef struct Vec2 {
    int x;
    int y;
} Vec2;

typedef struct Vec3 {
    int x;
    int y;
    int z;
} Vec3;

// +--------+
// | SHAPES |
// +--------+

typedef struct Rect {
    int x;
    int y;
    int w;
    int h;
} Rect;

#endif // MATHS_DEFS_H_INCLUDED