#include "sprNodes.h"
#include <inttypes.h>  // For PRIu32, PRId32

int sprCounter = 0;
SprNode* spriteList = NULL;

void* AllocateMeta(size_t size) {
    void* p = malloc(size);
    if (p) ramMetaUsage += size;
    return p;
}

void FreeMeta(void* p, size_t size) {
    if (p) {
        free(p);
        ramMetaUsage -= size;
    }
}

void* AllocateTex(size_t size) {
    void* p = malloc(size);
    if (p) ramTexUsage += size;
    return p;
}

void FreeTex(void* p, size_t size) {
    if (p) {
        free(p);
        ramTexUsage -= size;
    }
}

void* CallocTex(size_t count, size_t size) {
    void* p = calloc(count, size);
    if (p) ramTexUsage += count * size;
    return p;
}


unsigned char* ReallocateTex(unsigned char* p, size_t oldSize, size_t newSize) {
    if (p) ramTexUsage -= oldSize;
    unsigned char* q = (unsigned char*)realloc(p, newSize);
    if (q) ramTexUsage += newSize;
    return q;
}

void SafeUnloadImage(Image img) {
    if (img.data) {
        FreeTex(img.data, img.width * img.height * 4);
        img.data = NULL;
    }
    UnloadImage(img);
}

void SafeUnloadTexture(Texture2D tex) {
    if (tex.id != 0) {
        ramTexUsage -= tex.width * tex.height * 4;
        UnloadTexture(tex);
    }
}

// Structure definitions
struct ChunkHeader {
    char name[4];
    uint32_t size;
};

// Jump stack implementation (replacing std::stack)
#define MAX_STACK_SIZE 256
static long jumpStackData[MAX_STACK_SIZE];
static int jumpStackTop = -1;

void pushJumpStack(long offset) {
    if (jumpStackTop < MAX_STACK_SIZE - 1) {
        jumpStackData[++jumpStackTop] = offset;
    }
}

long popJumpStack(void) {
    if (jumpStackTop >= 0) {
        return jumpStackData[jumpStackTop--];
    }
    return -1;
}

int isJumpStackEmpty(void) {
    return jumpStackTop == -1;
}

// Helper function to read name into a newly allocated string
char* readName(char name[4]) {
    char* str = (char*)malloc(5);
    if (str) {
        memcpy(str, name, 4);
        str[4] = '\0';
    }
    return str;
}

// Utility: read a uint32
uint32_t readU32(FILE* file) {
    uint32_t v;
    fread(&v, 4, 1, file);
    return v;
}

float readF32(FILE* f) {
    float v;
    fread(&v, 4, 1, f);
    return v;
}

// Helper to read uint16
uint16_t readU16(FILE* file) {
    uint16_t v;
    fread(&v, 2, 1, file);
    return v;
}

// Helper to read int16
int16_t readI16(FILE* file) {
    int16_t v;
    fread(&v, 2, 1, file);
    return v;
}

// Jump stack (like Extensions.cs)
void jump(FILE* file, long offset) {
    long current = ftell(file);
    pushJumpStack(current);
    fseek(file, offset, SEEK_SET);
}

void jumpBack(FILE* file) {
    if (isJumpStackEmpty()) return;
    long pos = popJumpStack();
    fseek(file, pos, SEEK_SET);
}

// Read string from offset pattern (see Extensions.cs)
char* readStringFromOffset(FILE* file) {
    uint32_t offset = readU32(file);
    jump(file, offset - 4);
    uint32_t len = readU32(file);
    char* s = (char*)malloc(len + 1);
    if (s) {
        fread(s, 1, len, file);
        s[len] = '\0';
    }
    jumpBack(file);
    return s;
}

void addSprt(SprNode **sprInput,
             char* name,
             int width,
             int height,
             int marginLeft,
             int marginRight,
             int marginBottom,
             int marginTop,
             bool transparent,
             bool smooth,
             bool preload,
             int bboxMode,
             int sepMasks,
             int originX,
             int originY,
             bool specialType,
             int sVersion,
             int spriteType,
             float playbackSpeed,
             int playbackSpeedType,
             int sequenceOffset,
             int nineSliceOffset)
{
    SprNode *n_node = (SprNode*)malloc(sizeof(SprNode));
    if (!n_node) return;

    n_node->sprite.id = sprCounter++;
    n_node->sprite.name = name;
    n_node->sprite.width = width;
    n_node->sprite.height = height;
    n_node->sprite.marginLeft = marginLeft;
    n_node->sprite.marginRight = marginRight;
    n_node->sprite.marginBottom = marginBottom;
    n_node->sprite.marginTop = marginTop;
    n_node->sprite.transparent = transparent;
    n_node->sprite.smooth = smooth;
    n_node->sprite.preload = preload;
    n_node->sprite.bboxMode = bboxMode;
    n_node->sprite.sepMasks = sepMasks;
    n_node->sprite.originX = originX;
    n_node->sprite.originY = originY;
    n_node->sprite.specialType = specialType;
    n_node->sprite.sVersion = sVersion;
    n_node->sprite.spriteType = spriteType;
    n_node->sprite.playbackSpeed = playbackSpeed;
    n_node->sprite.playbackSpeedType = playbackSpeedType;
    n_node->sprite.sequenceOffset = sequenceOffset;
    n_node->sprite.nineSliceOffset = nineSliceOffset;
    n_node->sprite.tpagAddrs = NULL;
    n_node->sprite.textures = NULL;
    n_node->sprite.textureCount = 0;

    n_node->next = NULL;

    if (*sprInput == NULL) {
        *sprInput = n_node;
    } else {
        SprNode *cur = *sprInput;
        while (cur->next) cur = cur->next;
        cur->next = n_node;
    }
}

void showSprt(SprNode *spr) {
    if (spr == NULL) {
        printf("Sprite list is empty\n");
        return;
    }

    printf("Sprites (%d):\n", sprCounter);
    SprNode *cur = spr;
    while (cur != NULL) {
        SpriteSt *s = &cur->sprite;
        printf("--------------------------------------------------\n");
        printf("ID: %d\n", s->id);
        printf("Name: %s\n", s->name);
        printf("Size: %dx%d\n", s->width, s->height);
        printf("Margins L/R/B/T: %d / %d / %d / %d\n",
               s->marginLeft, s->marginRight, s->marginBottom, s->marginTop);
        printf("Flags: Transparent=%s Smooth=%s Preload=%s\n",
               s->transparent ? "true" : "false",
               s->smooth ? "true" : "false",
               s->preload ? "true" : "false");
        printf("Bounding box mode: %d\n", s->bboxMode);
        printf("SepMasks: %d\n", s->sepMasks);
        printf("Origin: (%d,%d)\n", s->originX, s->originY);
        printf("Special type: %s\n", s->specialType ? "true" : "false");
        printf("Version: %d\n", s->sVersion);
        printf("Sprite type: %d\n", s->spriteType);
        printf("Playback speed: %f  Type: %d\n", s->playbackSpeed, s->playbackSpeedType);
        printf("Sequence offset: %d  Nine-slice offset: %d\n", s->sequenceOffset, s->nineSliceOffset);

        // Textures list
        printf("Textures:\n");
        texNode *t = s->textures;
        if (!t) {
            printf("  <no textures>\n");
        } else {
            int ti = 0;
            while (t) {
                sprTexPage *p = &t->texture;
                printf("  [%d] PageItem: %d  Src:(%d,%d %dx%d)  Tgt:(%d,%d %dx%d)  Bound:(%dx%d)\n",
                       ti,
                       p->pageItem,
                       p->sourceX, p->sourceY, p->sourceW, p->sourceH,
                       p->targetX, p->targetY, p->targetW, p->targetH,
                       p->boundW, p->boundH);
                t = t->next;
                ti++;
            }
        }

        cur = cur->next;
    }
    printf("--------------------------------------------------\n");
}

void addTx2Sprt(SprNode **sprInput,
                int pageItem,
                int sourceX,
                int sourceY,
                int sourceW,
                int sourceH,
                int targetX,
                int targetY,
                int targetW,
                int targetH,
                int boundW,
                int boundH)
{
    if (!sprInput || !*sprInput) return;

    texNode *n_node = (texNode*)malloc(sizeof(texNode));
    if (!n_node) return;

    n_node->texture.id = (*sprInput)->sprite.textureCount;
    n_node->texture.pageItem = pageItem;
    n_node->texture.sourceX = sourceX;
    n_node->texture.sourceY = sourceY;
    n_node->texture.sourceW = sourceW;
    n_node->texture.sourceH = sourceH;
    n_node->texture.targetX = targetX;
    n_node->texture.targetY = targetY;
    n_node->texture.targetW = targetW;
    n_node->texture.targetH = targetH;
    n_node->texture.boundW = boundW;
    n_node->texture.boundH = boundH;

    n_node->next = NULL;
    (*sprInput)->sprite.textureCount++;

    // Append to sprInput->sprite.textures list
    if ((*sprInput)->sprite.textures == NULL) {
        (*sprInput)->sprite.textures = n_node;
    } else {
        texNode *cur = (*sprInput)->sprite.textures;
        while (cur->next) cur = cur->next;
        cur->next = n_node;
    }
}

// Parse SPRT chunk
void parseSPRT(FILE* file, long chunkStart, uint32_t chunkSize) {
    fseek(file, chunkStart + 8, SEEK_SET);
    uint32_t count = readU32(file);

    printf("Found %u Sprites:\n", count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t addr = readU32(file);
        if (addr == 0) {
            printf("  [%u] <null pointer>\n", i);
            continue;
        }

        // Jump to sprite structure in-file
        jump(file, addr);

        char* name = readStringFromOffset(file);
        uint32_t width  = readU32(file);
        uint32_t height = readU32(file);

        // --- read margins and flags ---
        int32_t marginLeft   = (int32_t)readU32(file);
        int32_t marginRight  = (int32_t)readU32(file);
        int32_t marginBottom = (int32_t)readU32(file);
        int32_t marginTop    = (int32_t)readU32(file);

        uint32_t transparent = readU32(file);
        uint32_t smooth      = readU32(file);
        uint32_t preload     = readU32(file);

        uint32_t bboxMode = readU32(file);
        uint32_t sepMasks = readU32(file);
        uint32_t originX  = readU32(file);
        uint32_t originY  = readU32(file);

        int32_t specialType = readU32(file); // -1 for normal
        uint32_t sVersion = readU32(file);   // 3
        uint32_t spriteType = readU32(file); // 0 = Normal
        float playbackSpeed = readF32(file);
        uint32_t playbackSpeedType = readU32(file);

        uint32_t sequenceOffset = readU32(file);
        uint32_t nineSliceOffset = readU32(file);

        // CREATE SPRITE FIRST (before textures)
        addSprt(&spriteList,
                name,
                width,
                height,
                marginLeft,
                marginRight,
                marginBottom,
                marginTop,
                transparent != 0,
                smooth != 0,
                preload != 0,
                bboxMode,
                sepMasks,
                originX,
                originY,
                specialType != 0,
                sVersion,
                spriteType,
                playbackSpeed,
                playbackSpeedType,
                sequenceOffset,
                nineSliceOffset);

        // Get pointer to newly added sprite (last in list)
        SprNode* currentSprite = spriteList;
        while (currentSprite->next) currentSprite = currentSprite->next;

        // do NOT parse TPAG contents now. Only store TPAG addresses.
        uint32_t textureCount = readU32(file);
        if (textureCount > 0) {
            currentSprite->sprite.tpagAddrs = (uint32_t*)malloc(sizeof(uint32_t)*textureCount);
            for (uint32_t t=0; t<textureCount; ++t) {
                currentSprite->sprite.tpagAddrs[t] = readU32(file);
            }
            currentSprite->sprite.textureCount = textureCount;
        }

        // Leave sprite parsing scope and jump back
        jumpBack(file);
    }

    fseek(file, chunkStart + 8 + chunkSize, SEEK_SET);
}

void InitAllSpriteQoiCursors(FILE *f,
                            long sprtChunkOffset,
                            uint32_t sprtChunkSize)
{
    // Throw away any previous list
    freeSpriteList();
    sprCounter = 0;     // reset global counter

    // Seek to SPRT chunk, read sprite count
    fseek(f, sprtChunkOffset + 8, SEEK_SET);
    uint32_t spriteCount = readU32(f);

    // Walk every sprite entry
    for (uint32_t i = 0; i < spriteCount; ++i)
    {
        uint32_t spriteAddr = readU32(f);
        if (spriteAddr == 0) continue;  // safety – should not happen

        jump(f, spriteAddr);    // go to the sprite struct

        // read all fields
        char *name   = readStringFromOffset(f);
        uint32_t w   = readU32(f);
        uint32_t h   = readU32(f);

        int32_t marginL = (int32_t)readU32(f);
        int32_t marginR = (int32_t)readU32(f);
        int32_t marginB = (int32_t)readU32(f);
        int32_t marginT = (int32_t)readU32(f);

        uint32_t transparent = readU32(f);
        uint32_t smooth      = readU32(f);
        uint32_t preload     = readU32(f);

        uint32_t bboxMode = readU32(f);
        uint32_t sepMasks = readU32(f);
        uint32_t originX  = readU32(f);
        uint32_t originY  = readU32(f);

        int32_t  specialType = readU32(f);
        uint32_t sVersion    = readU32(f);
        uint32_t spriteType  = readU32(f);
        float    playbackSpeed = readF32(f);
        uint32_t playbackSpeedType = readU32(f);

        uint32_t sequenceOffset  = readU32(f);
        uint32_t nineSliceOffset = readU32(f);

        // create node & add to master list
        addSprt(&spriteList,
                name,
                (int)w, (int)h,
                marginL, marginR, marginB, marginT,
                transparent != 0,
                smooth != 0,
                preload != 0,
                (int)bboxMode,
                (int)sepMasks,
                (int)originX,
                (int)originY,
                specialType != 0,
                (int)sVersion,
                (int)spriteType,
                playbackSpeed,
                (int)playbackSpeedType,
                (int)sequenceOffset,
                (int)nineSliceOffset);

        // pointer to the node we just added (tail)
        SprNode *newNode = spriteList;
        while (newNode->next) newNode = newNode->next;

        // read TPAG address list (only raw addresses)
        uint32_t texCount = readU32(f);
        if (texCount > 0)
        {
            newNode->sprite.tpagAddrs = (uint32_t*)AllocateMeta(
                sizeof(uint32_t) * texCount);
            newNode->sprite.qoiCursors = (QoiCursor*)AllocateMeta(
                sizeof(QoiCursor) * texCount);
            for (uint32_t t = 0; t < texCount; ++t)
                newNode->sprite.tpagAddrs[t] = readU32(f);
            newNode->sprite.textureCount = (int)texCount;
        }

        /*
        For every frame (TPAG entry) compute the cursor.
        The result is stored in newNode->sprite.qoiCursors[t].
        */
        for (uint32_t t = 0; t < texCount; ++t)
        {
            uint32_t tpagAddr = newNode->sprite.tpagAddrs[t];
            fseek(f, tpagAddr, SEEK_SET);

            uint16_t srcX = readU16(f);
            uint16_t srcY = readU16(f);
            uint16_t srcW = readU16(f);
            uint16_t srcH = readU16(f);
            uint16_t dstX = readU16(f);
            uint16_t dstY = readU16(f);
            uint16_t dstW = readU16(f);
            uint16_t dstH = readU16(f);
            uint16_t boundW = readU16(f);
            uint16_t boundH = readU16(f);
            int16_t  texPage = readI16(f);

            if (texPage < 0 || texPage >= textureCount)
            {
                newNode->sprite.qoiCursors[t] = (QoiCursor){0};
                continue;
            }

            size_t pageOff  = texturePages[texPage].offset;
            size_t pageEnd  = texturePages[texPage].endOffset;
            size_t pageSize = pageEnd - pageOff;

            unsigned char hdr[4];
            fseek(f, pageOff, SEEK_SET);
            fread(hdr, 1, 4, f);
            fseek(f, pageOff, SEEK_SET); // rewind for helper

            if (hdr[0] == 'f' && hdr[1] == 'i' && hdr[2] == 'o' && hdr[3] == 'q')
            {
                // plain QOI (no compression)
                unsigned char *buf = (unsigned char *)AllocateTex(pageSize);
                if (!buf) fprintf(stderr, "OOM",
                    "Failed to allocate %zu bytes for QOI page", pageSize);
                fread(buf, 1, pageSize, f);

                int qoiWidth = buf[4] | (buf[5] << 8);
                int qoiHeight = buf[6] | (buf[7]<<8);
                const uint8_t *pix = buf + 12;
                size_t pixLen = pageSize - 12;

                newNode->sprite.qoiCursors[t] = QoiFastSeek(
                    pix, pixLen, qoiWidth, (int)srcY);

                FreeTex(buf, pageSize);
            }
            else if (hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' && hdr[3] == 'G')
            {
                // PNG – decoder never needs a cursor
                newNode->sprite.qoiCursors[t] = (QoiCursor){0};
            }
            else
            {
                // BZ2+QOI – use the existing streaming helper
                LoadBZ2QOIStreamForQoiPos(
                    f,
                    pageOff,                // texture‑page offset
                    srcX, srcY, srcW, srcH,  // only srcY matters for the cursor
                    dstX, dstY,
                    newNode->sprite.width,   // potW – logical sprite width
                    newNode->sprite.height,  // potH – logical sprite height
                    newNode->sprite);        // helper writes cursor into .qoi

                // helper wrote the cursor into sprite's qoi – copy it
                newNode->sprite.qoiCursors[t] = newNode->sprite.qoi;
            }
        }

        // store the cursor of the first frame as the legacy default
        if (texCount > 0)
            newNode->sprite.qoi = newNode->sprite.qoiCursors[0];

        jumpBack(f);   // back to the SPRT address table
    }

    // Leave file pointer at the end of the SPRT chunk
    fseek(f, sprtChunkOffset + 8 + sprtChunkSize, SEEK_SET);
}

// free sprite list and its arrays
void freeSpriteList(void) {
    SprNode* cur = spriteList;
    while (cur) {
        SprNode* next = cur->next;

        if (cur->sprite.name) {
            free(cur->sprite.name);
            cur->sprite.name = NULL;
        }

        if (cur->sprite.tpagAddrs) {
            free(cur->sprite.tpagAddrs);
            cur->sprite.tpagAddrs = NULL;
            cur->sprite.textureCount = 0;
        }

        // free per‑frame cursor array
        if (cur->sprite.qoiCursors) {
            free(cur->sprite.qoiCursors);
            cur->sprite.qoiCursors = NULL;
        }

        free(cur);
        cur = next;
    }
    spriteList = NULL;
    sprCounter = 0;
}


// Parse TPAG chunk (Texture Page Items)
void parseTPAG(FILE* file, long chunkStart, uint32_t chunkSize) {
    fseek(file, chunkStart + 8, SEEK_SET); // skip header
    uint32_t count = readU32(file);

    printf("Found %u Texture Page Items (TPAG):\n", count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t addr = readU32(file);

        jump(file, addr);

        // Read fields as per UndertaleModLib.Models.UndertaleTexturePageItem
        uint16_t sourceX = readU16(file);
        uint16_t sourceY = readU16(file);
        uint16_t sourceW = readU16(file);
        uint16_t sourceH = readU16(file);
        uint16_t targetX = readU16(file);
        uint16_t targetY = readU16(file);
        uint16_t targetW = readU16(file);
        uint16_t targetH = readU16(file);
        uint16_t boundW  = readU16(file);
        uint16_t boundH  = readU16(file);
        int16_t  texPage = readI16(file);

        printf("  [%u] Src: (%u,%u,%ux%u) Dst: (%u,%u,%ux%u) Bound: %ux%u TexturePage: %d\n",
               i, sourceX, sourceY, sourceW, sourceH,
               targetX, targetY, targetW, targetH,
               boundW, boundH, texPage);

        jumpBack(file);
    }

    // Move to end of TPAG chunk
    fseek(file, chunkStart + 8 + chunkSize, SEEK_SET);
}

// Parse ROOM chunk (very similar, minimal demo)
void parseROOM(FILE* file, long chunkStart, uint32_t chunkSize) {
    fseek(file, chunkStart + 8, SEEK_SET); // skip "ROOM"+size
    uint32_t count = readU32(file);

    printf("Found %u Rooms:\n", count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t addr = readU32(file);

        jump(file, addr);
        char* name = readStringFromOffset(file);

        // Room usually has width/height right after name
        uint32_t width  = readU32(file);
        uint32_t height = readU32(file);

        printf("  [%u] %s (%ux%u)\n", i, name, width, height);

        free(name);
        jumpBack(file);
    }

    // move to the end of the SPRT chunk
    fseek(file, chunkStart + 8 + chunkSize, SEEK_SET);
}

int match(const unsigned char* buf, const char* sig, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] != (unsigned char)sig[i]) return 0;
    }
    return 1;
}

// check if data at pos matches any texture signature
int isTextureSignature(const unsigned char* p) {
    // Only TRUE file headers, never internal markers
    return (p[0] == '\x89' && p[1] == 'P' && p[2] == 'N' && p[3] == 'G') ||
           (p[0] == 'f' && p[1] == 'i' && p[2] == 'o' && p[3] == 'q') ||
           (p[0] == 'B' && p[1] == 'Z' && p[2] == 'h');  // <-- THIS
}

// Parse TXTR chunk (scan for texture signatures, fill texturePages)
void parseTXTR(FILE* file, long chunkStart, uint32_t chunkSize) {
    textureCount = 0;
    size_t chunkEnd = static_cast<size_t>(chunkStart + 8 + chunkSize);
    fseek(file, chunkStart + 8, SEEK_SET);

    const size_t BUFSIZE = 4096; // 4 KB block
    unsigned char buf[BUFSIZE];
    size_t baseOffset = static_cast<size_t>(ftell(file));

    while (baseOffset < chunkEnd && textureCount < 256) {
        size_t remaining = chunkEnd - baseOffset;
        size_t toRead = (remaining > BUFSIZE) ? BUFSIZE : remaining;
        size_t n = fread(buf, 1, toRead, file);
        if (n == 0) break;

        bool foundTexture = false;

        for (size_t i = 0; i + 4 <= n; ++i) {
            if (isTextureSignature(&buf[i])) {
                size_t texOffset = baseOffset + i;

                // forward scan to find end of this texture
                size_t nextPos = texOffset + 4;
                unsigned char sig[4];
                while (nextPos + 4 <= chunkEnd) {
                    fseek(file, nextPos, SEEK_SET);
                    if (fread(sig, 1, 4, file) != 4) break;
                    if (isTextureSignature(sig) || match(sig,"AUDO",4)) break;
                    nextPos++;
                }

                texturePages[textureCount].id = textureCount;
                texturePages[textureCount].offset = texOffset;
                texturePages[textureCount].endOffset = nextPos;
                textureCount++;

                // jump file pointer to end of this texture for next loop
                fseek(file, nextPos, SEEK_SET);
                baseOffset = nextPos;
                foundTexture = true;
                break; // break out of the inner for‑loop
            }
        }

        if (!foundTexture) {
            baseOffset += n; // advance normally if no texture found in this block
        }
    }
}

// Recursive chunk walker
void listChunks(FILE* file, long endPos, int depth) {
    while (ftell(file) < endPos) {
        struct ChunkHeader header;
        size_t bytesRead = fread(&header, 1, sizeof(header), file);
        if (bytesRead != sizeof(header)) break;

        char* name = readName(header.name);
        uint32_t size = header.size;
        long chunkStart = ftell(file) - 8;

        for (int i = 0; i < depth; i++) printf("  ");
        printf("Chunk: %s | Size: %u\n", name, size);

        if (strcmp(name, "FORM") == 0) {
            long formEnd = ftell(file) + (long)size;
            listChunks(file, formEnd, depth + 1);
            fseek(file, formEnd, SEEK_SET);
        } else {
            if (strcmp(name, "SPRT") == 0) {
                parseSPRT(file, chunkStart, size);
            } /*else if (strcmp(name, "TPAG") == 0) {
                parseTPAG(file, chunkStart, size);
            } */else if (strcmp(name, "ROOM") == 0) {
                parseROOM(file, chunkStart, size);
            } else if (strcmp(name, "TXTR") == 0) {
                parseTXTR(file, chunkStart, size);
            } else {
                fseek(file, size, SEEK_CUR);
            }
        }

        free(name);

        if (feof(file) || ferror(file)) break;
    }
}

void listChunksIndex(FILE* file, long endPos, int depth, DataWinIndex* index) {
    while (ftell(file) < endPos) {
        struct ChunkHeader header;
        if (fread(&header, 1, sizeof(header), file) != sizeof(header)) break;

        char name[5];
        memcpy(name, header.name, 4);
        name[4] = '\0';

        uint32_t size = header.size;
        long chunkStart = ftell(file) - 8;

        if (strcmp(name, "FORM") == 0) {
            long formEnd = ftell(file) + (long)size;
            listChunksIndex(file, formEnd, depth + 1, index);
            fseek(file, formEnd, SEEK_SET);
        } else {
            if (strcmp(name, "SPRT") == 0) {
                index->sprtOffset = chunkStart;
                index->sprtSize   = size;
            } else if (strcmp(name, "TXTR") == 0) {
                index->txtrOffset = chunkStart;
                index->txtrSize   = size;
            } else if (strcmp(name, "ROOM") == 0) {
                index->roomOffset = chunkStart;
                index->roomSize   = size;
            } else if (strcmp(name, "TPAG") == 0) {
                index->tpagOffset = chunkStart;
                index->tpagSize   = size;
            }
            fseek(file, size, SEEK_CUR);
        }

        if (feof(file) || ferror(file)) break;
    }
}

static SpriteOffset* sprtOffsets = NULL;
uint32_t sprtCount = 0;

// Build index of SPRT entries
void scanSPRT(FILE* f, uint32_t sprtChunkOffset, uint32_t sprtChunkSize)
{
    // Skip "SPRT" + size
    fseek(f, sprtChunkOffset + 8, SEEK_SET);

    sprtCount = readU32(f);  // number of sprites
    if (sprtOffsets) { //free(sprtOffsets); sprtOffsets = NULL; }
        FreeMeta(sprtOffsets, sizeof(SpriteOffset) * sprtCount);
        sprtOffsets = NULL;
    }
    //sprtOffsets = (SpriteOffset*)malloc(sizeof(SpriteOffset) * sprtCount);
    sprtOffsets = (SpriteOffset*)AllocateMeta(sizeof(SpriteOffset) * sprtCount);


    for (uint32_t i = 0; i < sprtCount; i++) {
        sprtOffsets[i].offset = ftell(f);   // position of address entry
        sprtOffsets[i].size   = 4;          // each entry is just a uint32 address
        fseek(f, 4, SEEK_CUR);              // skip over the address
    }

    // move to end of chunk
    fseek(f, sprtChunkOffset + 8 + sprtChunkSize, SEEK_SET);
}

/* Simple linear search – the sprite list is tiny (a few hundred
   entries at most) so this is fast enough and keeps the code
   dependency‑free.
*/
SprNode* FindSpriteNodeById(int id)
{
    SprNode *cur = spriteList;
    while (cur)
    {
        if (cur->sprite.id == id) return cur;
        cur = cur->next;
    }
    return NULL;
}

int parseSPRT_single(FILE* file, int currentIndex, int direction)
{
    if (!sprtOffsets || sprtCount == 0) return currentIndex;

    //compute new index
    int index = currentIndex + direction;
    if (index < 0) index = 0;
    else if (index >= (int)sprtCount) index = sprtCount - 1;

    //free old temporary allocations
    if (currentSprite.sprite.name) {
        FreeMeta(currentSprite.sprite.name,
                 strlen(currentSprite.sprite.name) + 1);
        currentSprite.sprite.name = NULL;
    }
    if (currentSprite.sprite.tpagAddrs) {
        FreeMeta(currentSprite.sprite.tpagAddrs,
                 sizeof(uint32_t) * currentSprite.sprite.textureCount);
        currentSprite.sprite.tpagAddrs = NULL;
    }
    currentSprite.sprite.textureCount = 0;

    //Load the sprite structure (same as parseSPRT()).
    fseek(file, sprtOffsets[index].offset, SEEK_SET);
    uint32_t addr = readU32(file);
    if (addr == 0) return index;

    jump(file, addr);

    char *name = readStringFromOffset(file);
    uint32_t width  = readU32(file);
    uint32_t height = readU32(file);

    int32_t marginL = (int32_t)readU32(file);
    int32_t marginR = (int32_t)readU32(file);
    int32_t marginB = (int32_t)readU32(file);
    int32_t marginT = (int32_t)readU32(file);

    uint32_t transparent = readU32(file);
    uint32_t smooth      = readU32(file);
    uint32_t preload     = readU32(file);

    uint32_t bboxMode = readU32(file);
    uint32_t sepMasks = readU32(file);
    uint32_t originX  = readU32(file);
    uint32_t originY  = readU32(file);

    int32_t specialType = readU32(file);
    uint32_t sVersion   = readU32(file);
    uint32_t spriteType = readU32(file);
    float    playbackSpeed = readF32(file);
    uint32_t playbackSpeedType = readU32(file);

    uint32_t sequenceOffset  = readU32(file);
    uint32_t nineSliceOffset = readU32(file);

    //Fill the temporary global `currentSprite`.
    currentSprite.sprite.id      = index;
    currentSprite.sprite.name    = name;
    currentSprite.sprite.width   = (int)width;
    currentSprite.sprite.height  = (int)height;
    currentSprite.sprite.marginLeft  = marginL;
    currentSprite.sprite.marginRight = marginR;
    currentSprite.sprite.marginBottom = marginB;
    currentSprite.sprite.marginTop   = marginT;
    currentSprite.sprite.transparent = transparent != 0;
    currentSprite.sprite.smooth      = smooth != 0;
    currentSprite.sprite.preload     = preload != 0;
    currentSprite.sprite.bboxMode   = (int)bboxMode;
    currentSprite.sprite.sepMasks   = (int)sepMasks;
    currentSprite.sprite.originX    = (int)originX;
    currentSprite.sprite.originY    = (int)originY;
    currentSprite.sprite.specialType = specialType != 0;
    currentSprite.sprite.sVersion   = (int)sVersion;
    currentSprite.sprite.spriteType = (int)spriteType;
    currentSprite.sprite.playbackSpeed        = playbackSpeed;
    currentSprite.sprite.playbackSpeedType    = (int)playbackSpeedType;
    currentSprite.sprite.sequenceOffset       = (int)sequenceOffset;
    currentSprite.sprite.nineSliceOffset      = (int)nineSliceOffset;

    //TPAG addresses
    uint32_t texCount = readU32(file);
    if (texCount > 0) {
        currentSprite.sprite.tpagAddrs = (uint32_t*)AllocateMeta(
            sizeof(uint32_t) * texCount);
        for (uint32_t t = 0; t < texCount; ++t)
            currentSprite.sprite.tpagAddrs[t] = readU32(file);
        currentSprite.sprite.textureCount = (int)texCount;
    }

    //copy the pre‑computed cursor array (and the default/cursor) from the master list.
    SprNode *masterNode = FindSpriteNodeById(index);
    if (masterNode) {
        currentSprite.sprite.qoiCursors = masterNode->sprite.qoiCursors;
        if (masterNode->sprite.textureCount > 0) {
            currentSprite.sprite.qoi = masterNode->sprite.qoiCursors[0];
        }
    }

    jumpBack(file);
    return index;
}

void clearSprites()
{
    SprNode* n = spriteList;
    while (n) {
        SprNode* next = n->next;

        /*free(n->sprite.tpagAddrs);
        free(n->sprite.name);
        free(n);*/
        FreeMeta(n->sprite.tpagAddrs, sizeof(uint32_t) * n->sprite.textureCount);
        FreeMeta(n->sprite.name, strlen(n->sprite.name)+1);
        FreeMeta(n, sizeof(SprNode));

        n = next;
    }
    spriteList = NULL;
}