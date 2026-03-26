#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <raylib.h>
#include <rlgl.h>
#include <string.h>
#include <thread>

// QOI OPCODES
#define QOI_INDEX    0x00
#define QOI_RUN_8    0x40
#define QOI_RUN_16   0x60
#define QOI_DIFF_8   0x80
#define QOI_DIFF_16  0xC0
#define QOI_DIFF_24  0xE0
#define QOI_COLOR    0xF0

#define QOI_MASK_2   0xC0
#define QOI_MASK_3   0xE0
#define QOI_MASK_4   0xF0

// NON STREAMING DECODER HELPERS

typedef struct QoiImage {
    int width;
    int height;
    uint8_t* pixels;
    size_t pixels_size;
} QoiImage;

QoiImage DecodeQoi(const uint8_t* data, size_t size);

// STREAMING DECODER HELPERS
typedef struct {
    int srcX, srcY, srcW, srcH;
    int dstX, dstY;
    int potW, potH;
    uint8_t* canvas;
    int done;
    int direct;
} SpriteRowWriter;

void QoiWriteRowToSprite(
    void* user,
    int y,
    const uint8_t* row,
    int width
);

typedef struct {
    size_t pos;
    uint8_t r,g,b,a;
    int run;
uint8_t index[64*20];
} QoiCursor;

struct QoiNode{
    QoiCursor cur;
    QoiNode* next;
};

/* --------------------------------------------------------------
   Helper struct used only by DecodeQoiToSpriteFast.
   -------------------------------------------------------------- */
typedef struct {
    const uint8_t* pixelData;   /* compressed QOI pixel stream            */
    size_t         length;      /* length of that stream (bytes)          */
    int            width;       /* image width (pixels)                    */
    int            targetY;     /* row we want to seek to (0‑based)       */
    QoiCursor      result;      /* filled by QoiFastSeek()                */
    /* cache‑line padding – 64 bytes is enough for any typical
       architecture; the extra space is unused.                */
    unsigned char  pad[64];
} SeekTask;

typedef struct {
    const uint8_t* data;
    size_t        size;
    void*         user;
    QoiCursor     cur;          /* cursor at startY */
    int           startY;
    int           endY;         /* exclusive */
    int           width;
    int           height;
    int           length;
    SpriteRowWriter* w;
    void (*RowWriter)(void* user, int y, const uint8_t* row, int width);
} DecodeTask;

void DecodeQoiToSprite(const uint8_t* data, size_t size, void* user,
                       void (*RowWriter)(void* user, int y, const uint8_t* row, int width));

QoiCursor QoiSeekToRow(const uint8_t* pixelData, size_t length,
                       int width, int height, int srcH, int targetY);

QoiCursor QoiFastSeek(const uint8_t* pixelData, size_t length,
                      int width, int targetY);

void DecodeQoiToSpriteFast(const uint8_t* data, size_t size, void* user,
    QoiCursor cur, void (*RowWriter)(void* user, int y, const uint8_t* row, int width));