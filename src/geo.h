#ifndef GEO_H
#define GEO_H

#include "server.h"

typedef struct client client;

/* Structures used inside geo.c in order to represent points and array of
 * points on the earth. */
// geo.c 内部用于表示地球上的点和点数组的结构。
typedef struct geoPoint
{
    double longitude;
    double latitude;
    double dist;
    double score;
    char *member;
} geoPoint;

typedef struct geoArray
{
    struct geoPoint *array;
    size_t buckets;
    size_t used;
} geoArray;

#endif // GEO_H
