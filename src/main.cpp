#include "eadkpp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace {

constexpr int TILE = 16;
constexpr int SCREEN_W = EADK_SCREEN_WIDTH;
constexpr int SCREEN_H = EADK_SCREEN_HEIGHT;
constexpr int MAP_W = 41;
constexpr int MAP_H = 29;
constexpr int MAX_ROOMS = 9;
constexpr int MAX_ENEMIES = 16;
constexpr int MAX_KEYS = 4;
constexpr int LEVEL_COUNT = 5;
constexpr int PLAYER_STEP = 95;
constexpr int ENEMY_STEP = 235;

enum : uint8_t { WALL, FLOOR };
enum : uint8_t { TITLE, PLAYING, DEAD, WON };
enum : uint8_t { S_FLOOR, S_WALL, S_EXIT, S_KEY, S_HERO, S_SLIME, S_COUNT };

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

const uint16_t C_FLOOR = rgb(44, 44, 56);
const uint16_t C_FLOOR_D = rgb(34, 34, 45);
const uint16_t C_FLOOR_L = rgb(56, 56, 70);
const uint16_t C_MORTAR = rgb(26, 22, 28);
const uint16_t C_WALL = rgb(88, 72, 64);
const uint16_t C_WALL_L = rgb(114, 94, 82);
const uint16_t C_WALL_T = rgb(134, 112, 96);
const uint16_t C_STONE = rgb(70, 66, 78);
const uint16_t C_STONE_D = rgb(42, 38, 50);
const uint16_t C_CYAN = rgb(96, 220, 224);
const uint16_t C_CYAN_D = rgb(34, 116, 132);
const uint16_t C_GOLD = rgb(232, 192, 72);
const uint16_t C_GOLD_D = rgb(160, 112, 24);
const uint16_t C_SKIN = rgb(236, 190, 146);
const uint16_t C_EYE = rgb(28, 22, 28);
const uint16_t C_CLOAK = rgb(188, 70, 52);
const uint16_t C_CLOAK_D = rgb(120, 40, 34);
const uint16_t C_HOOD = rgb(88, 30, 28);
const uint16_t C_SLIME = rgb(108, 204, 84);
const uint16_t C_SLIME_D = rgb(52, 118, 46);
const uint16_t C_SLIME_L = rgb(168, 236, 128);
const uint16_t UI_TEXT = rgb(236, 232, 220);
const uint16_t UI_DIM = rgb(148, 144, 134);
const uint16_t UI_ACCENT = rgb(232, 192, 80);

uint16_t screen[SCREEN_W * SCREEN_H];
uint16_t sprites[S_COUNT * TILE * TILE];

uint8_t map[MAP_H][MAP_W];
uint8_t seen[MAP_H][MAP_W];

struct Room
{
    uint8_t x, y, w, h;
};

struct Actor
{
    int tx, ty;
    float x, y;
    float sx, sy;
    uint32_t start;
    uint32_t duration;
    bool moving;
};

struct Enemy
{
    Actor body;
    uint32_t think;
    uint32_t stun;
};

Room rooms[MAX_ROOMS];
int roomCount;

Actor player;
Enemy enemies[MAX_ENEMIES];
int enemyCount;

int keyX[MAX_KEYS];
int keyY[MAX_KEYS];
bool keyTaken[MAX_KEYS];
int keyCount;
int keysLeft;
int exitX, exitY;

int level;
int hp;
int score;
int state;
uint32_t nowMs;
float lightRadius;
uint32_t flashUntil;
uint32_t hurtUntil;
uint32_t messageUntil;
bool lastOk;

uint32_t rnd(uint32_t n)
{
    return n ? eadk_random() % n : 0;
}

int sign(int v)
{
    return (v > 0) - (v < 0);
}

uint16_t shade(uint16_t c, uint8_t light)
{
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = c & 0x1F;
    r = (r * light) >> 4;
    g = (g * light) >> 4;
    b = (b * light) >> 4;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void put(uint16_t *s, int x, int y, uint16_t c)
{
    if (x >= 0 && x < TILE && y >= 0 && y < TILE)
        s[y * TILE + x] = c;
}

void rect(uint16_t *s, int x0, int y0, int x1, int y1, uint16_t c)
{
    if (x0 > x1)
    {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1)
    {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            put(s, x, y, c);
}

void ellipse(uint16_t *s, int cx, int cy, int rx, int ry, uint16_t c)
{
    for (int y = -ry; y <= ry; y++)
        for (int x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
                put(s, cx + x, cy + y, c);
}

void buildSprites()
{
    uint16_t *floor = &sprites[S_FLOOR * TILE * TILE];
    for (int y = 0; y < TILE; y++)
    {
        for (int x = 0; x < TILE; x++)
        {
            uint32_t h = (uint32_t)(x * 374761393u + y * 668265263u);
            h = (h ^ (h >> 13)) * 1274126177u;
            int n = (int)((h >> 20) % 31);
            uint16_t c = C_FLOOR;
            if (n == 0)
                c = C_FLOOR_D;
            else if (n == 1)
                c = C_FLOOR_L;
            floor[y * TILE + x] = c;
        }
    }

    uint16_t *wall = &sprites[S_WALL * TILE * TILE];
    for (int y = 0; y < TILE; y++)
    {
        for (int x = 0; x < TILE; x++)
        {
            int row = y / 4;
            int offset = (row & 1) ? 4 : 0;
            bool mortar = (y % 4 == 3) || (((x + offset) % 8) == 7);
            uint16_t c = C_WALL;
            if (mortar)
                c = C_MORTAR;
            else if (y % 4 == 0)
                c = C_WALL_T;
            else if ((x + offset) % 8 == 0)
                c = C_WALL_L;
            wall[y * TILE + x] = c;
        }
    }

    uint16_t *exit = &sprites[S_EXIT * TILE * TILE];
    rect(exit, 1, 2, 14, 15, C_STONE_D);
    rect(exit, 2, 3, 13, 15, C_STONE);
    rect(exit, 4, 5, 11, 15, C_CYAN_D);
    for (int y = 5; y < 16; y++)
    {
        for (int x = 4; x < 12; x++)
        {
            uint16_t c = C_CYAN_D;
            if (((x + y) % 5) == 0 || (y % 3) == 0)
                c = C_CYAN;
            put(exit, x, y, c);
        }
    }
    rect(exit, 4, 5, 11, 5, C_STONE);
    rect(exit, 4, 2, 4, 4, C_STONE);

    uint16_t *key = &sprites[S_KEY * TILE * TILE];
    ellipse(key, 8, 5, 4, 4, C_GOLD_D);
    ellipse(key, 8, 5, 3, 3, C_GOLD);
    ellipse(key, 8, 5, 1, 1, 0);
    rect(key, 6, 8, 9, 14, C_GOLD_D);
    rect(key, 7, 8, 8, 13, C_GOLD);
    rect(key, 9, 10, 11, 11, C_GOLD);
    rect(key, 9, 13, 11, 14, C_GOLD);

    uint16_t *hero = &sprites[S_HERO * TILE * TILE];
    ellipse(hero, 8, 12, 6, 4, C_CLOAK_D);
    ellipse(hero, 8, 12, 5, 3, C_CLOAK);
    ellipse(hero, 8, 5, 5, 5, C_HOOD);
    ellipse(hero, 8, 6, 4, 4, C_CLOAK_D);
    ellipse(hero, 8, 7, 2, 3, C_SKIN);
    put(hero, 6, 6, C_EYE);
    put(hero, 7, 6, C_EYE);
    put(hero, 9, 6, C_EYE);
    put(hero, 10, 6, C_EYE);
    rect(hero, 5, 13, 10, 13, C_GOLD_D);
    rect(hero, 6, 14, 9, 14, C_CLOAK_D);

    uint16_t *slime = &sprites[S_SLIME * TILE * TILE];
    ellipse(slime, 8, 11, 7, 5, C_SLIME_D);
    ellipse(slime, 8, 11, 6, 4, C_SLIME);
    ellipse(slime, 5, 9, 2, 1, C_SLIME_L);
    put(slime, 5, 10, C_EYE);
    put(slime, 6, 10, C_EYE);
    put(slime, 10, 10, C_EYE);
    put(slime, 11, 10, C_EYE);
    rect(slime, 7, 13, 9, 13, C_SLIME_D);
}

inline bool inBounds(int x, int y)
{
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

inline bool walkable(int x, int y)
{
    return inBounds(x, y) && map[y][x] == FLOOR;
}

void carveRect(int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (inBounds(i, j))
                map[j][i] = FLOOR;
}

void carveH(int x0, int x1, int y)
{
    if (x0 > x1)
    {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++)
        if (inBounds(x, y))
            map[y][x] = FLOOR;
}

void carveV(int y0, int y1, int x)
{
    if (y0 > y1)
    {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++)
        if (inBounds(x, y))
            map[y][x] = FLOOR;
}

bool roomsOverlap(const Room &a, const Room &b)
{
    return a.x < b.x + b.w + 1 && a.x + a.w + 1 > b.x &&
           a.y < b.y + b.h + 1 && a.y + a.h + 1 > b.y;
}

void connectRooms(const Room &a, const Room &b)
{
    int ax = a.x + a.w / 2;
    int ay = a.y + a.h / 2;
    int bx = b.x + b.w / 2;
    int by = b.y + b.h / 2;
    if (rnd(2))
    {
        carveH(ax, bx, ay);
        carveV(ay, by, bx);
    }
    else
    {
        carveV(ay, by, ax);
        carveH(ax, bx, by);
    }
}

void buildDungeon()
{
    do
    {
        memset(map, WALL, sizeof(map));
        roomCount = 0;
        for (int attempt = 0; attempt < 140 && roomCount < MAX_ROOMS; attempt++)
        {
            Room r;
            r.w = (uint8_t)(4 + rnd(5));
            r.h = (uint8_t)(4 + rnd(4));
            r.x = (uint8_t)(2 + rnd(MAP_W - r.w - 4));
            r.y = (uint8_t)(2 + rnd(MAP_H - r.h - 4));
            bool ok = true;
            for (int i = 0; i < roomCount; i++)
                if (roomsOverlap(r, rooms[i]))
                    ok = false;
            if (!ok)
                continue;
            carveRect(r.x, r.y, r.w, r.h);
            if (roomCount > 0)
                connectRooms(rooms[roomCount - 1], r);
            rooms[roomCount++] = r;
        }
    } while (roomCount < 4);
}

bool tileFree(int x, int y)
{
    if (!walkable(x, y))
        return false;
    if (x == exitX && y == exitY)
        return false;
    for (int i = 0; i < keyCount; i++)
        if (!keyTaken[i] && keyX[i] == x && keyY[i] == y)
            return false;
    if (x == player.tx && y == player.ty)
        return false;
    for (int i = 0; i < enemyCount; i++)
        if (enemies[i].body.tx == x && enemies[i].body.ty == y)
            return false;
    return true;
}

void placePlayer()
{
    const Room &r = rooms[0];
    player.tx = r.x + r.w / 2;
    player.ty = r.y + r.h / 2;
    player.x = player.tx + 0.5f;
    player.y = player.ty + 0.5f;
    player.sx = player.x;
    player.sy = player.y;
    player.moving = false;
}

void generateLevel()
{
    buildDungeon();
    placePlayer();

    int startX = player.tx;
    int startY = player.ty;
    int bestRoom = 1;
    int bestDist = -1;
    for (int i = 1; i < roomCount; i++)
    {
        int dx = rooms[i].x + rooms[i].w / 2 - startX;
        int dy = rooms[i].y + rooms[i].h / 2 - startY;
        int d = dx * dx + dy * dy;
        if (d > bestDist)
        {
            bestDist = d;
            bestRoom = i;
        }
    }
    exitX = rooms[bestRoom].x + rooms[bestRoom].w / 2;
    exitY = rooms[bestRoom].y + rooms[bestRoom].h / 2;

    keyCount = 3;
    keysLeft = 0;
    for (int i = 0; i < keyCount; i++)
    {
        keyTaken[i] = false;
        bool placed = false;
        for (int attempt = 0; attempt < 200 && !placed; attempt++)
        {
            int ri = (int)rnd(roomCount);
            const Room &r = rooms[ri];
            int x = r.x + (int)rnd(r.w);
            int y = r.y + (int)rnd(r.h);
            if (ri == 0 || ri == bestRoom)
                continue;
            if (tileFree(x, y))
            {
                keyX[i] = x;
                keyY[i] = y;
                placed = true;
            }
        }
        for (int attempt = 0; attempt < 400 && !placed; attempt++)
        {
            int x = (int)rnd(MAP_W);
            int y = (int)rnd(MAP_H);
            int dx = x - startX;
            int dy = y - startY;
            if (dx * dx + dy * dy < 36)
                continue;
            if (tileFree(x, y))
            {
                keyX[i] = x;
                keyY[i] = y;
                placed = true;
            }
        }
        if (placed)
            keysLeft++;
    }

    enemyCount = 0;
    int wanted = 2 + level * 2;
    if (wanted > MAX_ENEMIES)
        wanted = MAX_ENEMIES;
    for (int i = 0; i < wanted; i++)
    {
        for (int attempt = 0; attempt < 200; attempt++)
        {
            int x = (int)rnd(MAP_W);
            int y = (int)rnd(MAP_H);
            int dx = x - startX;
            int dy = y - startY;
            if (dx * dx + dy * dy < 49)
                continue;
            if (!tileFree(x, y))
                continue;
            Enemy &e = enemies[enemyCount++];
            e.body.tx = x;
            e.body.ty = y;
            e.body.x = x + 0.5f;
            e.body.y = y + 0.5f;
            e.body.sx = e.body.x;
            e.body.sy = e.body.y;
            e.body.moving = false;
            e.body.duration = ENEMY_STEP;
            e.think = nowMs + 500 + rnd(600);
            e.stun = 0;
            break;
        }
    }

    memset(seen, 0, sizeof(seen));
    player.moving = false;
    lightRadius = 4.4f;
    messageUntil = nowMs + 1300;
    flashUntil = 0;
    hurtUntil = 0;
}

void beginGame()
{
    level = 1;
    hp = 3;
    score = 0;
    generateLevel();
    state = PLAYING;
}

bool startMove(Actor &a, int dx, int dy, uint32_t duration)
{
    int nx = a.tx + dx;
    int ny = a.ty + dy;
    if (!walkable(nx, ny))
        return false;
    a.sx = a.x;
    a.sy = a.y;
    a.tx = nx;
    a.ty = ny;
    a.start = nowMs;
    a.duration = duration;
    a.moving = true;
    return true;
}

void updateActor(Actor &a)
{
    if (!a.moving)
    {
        a.x = a.tx + 0.5f;
        a.y = a.ty + 0.5f;
        return;
    }
    float t = (float)(nowMs - a.start) / (float)a.duration;
    if (t >= 1.0f)
    {
        t = 1.0f;
        a.moving = false;
    }
    a.x = a.sx + (a.tx + 0.5f - a.sx) * t;
    a.y = a.sy + (a.ty + 0.5f - a.sy) * t;
}

void thinkEnemy(Enemy &e)
{
    if (nowMs < e.stun)
        return;
    if (e.body.moving || nowMs < e.think)
        return;

    int dx = 0;
    int dy = 0;
    int ddx = player.tx - e.body.tx;
    int ddy = player.ty - e.body.ty;
    int adx = ddx < 0 ? -ddx : ddx;
    int ady = ddy < 0 ? -ddy : ddy;
    int dist = adx + ady;

    if (dist <= 8 && rnd(100) < 78)
    {
        int sx = sign(ddx);
        int sy = sign(ddy);
        if (adx >= ady)
        {
            if (walkable(e.body.tx + sx, e.body.ty))
                dx = sx;
            else if (walkable(e.body.tx, e.body.ty + sy))
                dy = sy;
        }
        else
        {
            if (walkable(e.body.tx, e.body.ty + sy))
                dy = sy;
            else if (walkable(e.body.tx + sx, e.body.ty))
                dx = sx;
        }
    }

    if (dx == 0 && dy == 0 && rnd(100) < 55)
    {
        int dir = (int)rnd(4);
        if (dir == 0)
            dx = 1;
        else if (dir == 1)
            dx = -1;
        else if (dir == 2)
            dy = 1;
        else
            dy = -1;
        if (!walkable(e.body.tx + dx, e.body.ty + dy))
        {
            dx = 0;
            dy = 0;
        }
    }

    if (dx != 0 || dy != 0)
        startMove(e.body, dx, dy, ENEMY_STEP);

    e.think = nowMs + 340 + rnd(240);
}

void reveal()
{
    int r = (int)(lightRadius + 1.0f);
    int cx = (int)player.x;
    int cy = (int)player.y;
    for (int y = cy - r; y <= cy + r; y++)
    {
        for (int x = cx - r; x <= cx + r; x++)
        {
            if (!inBounds(x, y))
                continue;
            float dx = x + 0.5f - player.x;
            float dy = y + 0.5f - player.y;
            if (dx * dx + dy * dy < (lightRadius + 0.5f) * (lightRadius + 0.5f))
                seen[y][x] = 1;
        }
    }
}

int tileLight(int tx, int ty)
{
    if (!inBounds(tx, ty))
        return 0;
    float dx = tx + 0.5f - player.x;
    float dy = ty + 0.5f - player.y;
    float d2 = dx * dx + dy * dy;
    float r2 = lightRadius * lightRadius;
    if (d2 < r2)
    {
        int l = 16 - (int)(d2 * (10.0f / r2));
        return l < 6 ? 6 : l;
    }
    return seen[ty][tx] ? 3 : 0;
}

void takeHit(int index)
{
    hp--;
    hurtUntil = nowMs + 1200;
    flashUntil = nowMs + 200;
    enemies[index].stun = nowMs + 1600;
    if (hp <= 0)
    {
        hp = 0;
        state = DEAD;
    }
}

void update(int dx, int dy, bool okPress)
{
    if (state == TITLE)
    {
        if (okPress)
            beginGame();
        return;
    }

    if (state == DEAD || state == WON)
    {
        if (okPress)
            state = TITLE;
        return;
    }

    lightRadius = 4.4f + 0.35f * (float)((nowMs / 140) % 2);
    reveal();

    updateActor(player);
    if (!player.moving)
    {
        if (dx < 0)
            startMove(player, -1, 0, PLAYER_STEP);
        else if (dx > 0)
            startMove(player, 1, 0, PLAYER_STEP);
        else if (dy < 0)
            startMove(player, 0, -1, PLAYER_STEP);
        else if (dy > 0)
            startMove(player, 0, 1, PLAYER_STEP);
    }

    for (int i = 0; i < enemyCount; i++)
    {
        Enemy &e = enemies[i];
        updateActor(e.body);
        thinkEnemy(e);
        float ddx = player.x - e.body.x;
        float ddy = player.y - e.body.y;
        if (ddx * ddx + ddy * ddy < 0.42f && nowMs >= hurtUntil)
            takeHit(i);
    }

    for (int i = 0; i < keyCount; i++)
    {
        if (!keyTaken[i] && player.tx == keyX[i] && player.ty == keyY[i])
        {
            keyTaken[i] = true;
            keysLeft--;
            score += 100;
        }
    }

    if (player.tx == exitX && player.ty == exitY && keysLeft == 0)
    {
        score += 200 * level;
        level++;
        if (level > LEVEL_COUNT)
        {
            state = WON;
            return;
        }
        if (hp < 3)
            hp++;
        generateLevel();
    }
}

void blit(int sx, int sy, const uint16_t *src, uint8_t light, bool alpha)
{
    int x0 = 0;
    int y0 = 0;
    int x1 = TILE;
    int y1 = TILE;
    if (sx < 0)
        x0 = -sx;
    if (sy < 0)
        y0 = -sy;
    if (sx + TILE > SCREEN_W)
        x1 = SCREEN_W - sx;
    if (sy + TILE > SCREEN_H)
        y1 = SCREEN_H - sy;
    if (x0 >= x1 || y0 >= y1)
        return;
    for (int y = y0; y < y1; y++)
    {
        int row = (sy + y) * SCREEN_W + sx;
        for (int x = x0; x < x1; x++)
        {
            uint16_t c = src[y * TILE + x];
            if (alpha && c == 0)
                continue;
            screen[row + x] = light >= 16 ? c : shade(c, light);
        }
    }
}

void drawTiles(int camX, int camY)
{
    int tx0 = camX / TILE;
    int ty0 = camY / TILE;
    int tx1 = (camX + SCREEN_W) / TILE + 1;
    int ty1 = (camY + SCREEN_H) / TILE + 1;
    if (tx0 < 0)
        tx0 = 0;
    if (ty0 < 0)
        ty0 = 0;
    if (tx1 > MAP_W)
        tx1 = MAP_W;
    if (ty1 > MAP_H)
        ty1 = MAP_H;

    for (int ty = ty0; ty < ty1; ty++)
    {
        for (int tx = tx0; tx < tx1; tx++)
        {
            int l = tileLight(tx, ty);
            if (l == 0)
                continue;
            int sprite = map[ty][tx] == WALL ? S_WALL : S_FLOOR;
            blit(tx * TILE - camX, ty * TILE - camY, &sprites[sprite * TILE * TILE], (uint8_t)l, false);
        }
    }
}

void drawSpriteAt(int tx, int ty, int sprite, int camX, int camY, bool alpha)
{
    int l = tileLight(tx, ty);
    if (l == 0)
        return;
    blit(tx * TILE - camX, ty * TILE - camY, &sprites[sprite * TILE * TILE], (uint8_t)l, alpha);
}

void render()
{
    if (state == TITLE)
    {
        memset(screen, 0, sizeof(screen));
        return;
    }

    int camX = (int)(player.x * TILE) - SCREEN_W / 2;
    int camY = (int)(player.y * TILE) - SCREEN_H / 2;
    if (nowMs < flashUntil)
    {
        camX += (int)((nowMs >> 3) & 1) * 4 - 2;
        camY += (int)((nowMs >> 4) & 1) * 4 - 2;
    }
    int maxX = MAP_W * TILE - SCREEN_W;
    int maxY = MAP_H * TILE - SCREEN_H;
    if (camX < 0)
        camX = 0;
    if (camY < 0)
        camY = 0;
    if (camX > maxX)
        camX = maxX;
    if (camY > maxY)
        camY = maxY;

    memset(screen, 0, sizeof(screen));
    drawTiles(camX, camY);

    drawSpriteAt(exitX, exitY, S_EXIT, camX, camY, true);

    for (int i = 0; i < keyCount; i++)
        if (!keyTaken[i])
            drawSpriteAt(keyX[i], keyY[i], S_KEY, camX, camY, true);

    for (int i = 0; i < enemyCount; i++)
    {
        Enemy &e = enemies[i];
        float ddx = e.body.x - player.x;
        float ddy = e.body.y - player.y;
        if (ddx * ddx + ddy * ddy > (lightRadius + 0.6f) * (lightRadius + 0.6f))
            continue;
        int l = tileLight(e.body.tx, e.body.ty);
        if (l == 0)
            continue;
        int sx = (int)(e.body.x * TILE + 0.5f) - camX - TILE / 2;
        int sy = (int)(e.body.y * TILE + 0.5f) - camY - TILE / 2;
        blit(sx, sy, &sprites[S_SLIME * TILE * TILE], (uint8_t)l, true);
    }

    int px = (int)(player.x * TILE + 0.5f) - camX - TILE / 2;
    int py = (int)(player.y * TILE + 0.5f) - camY - TILE / 2;
    blit(px, py, &sprites[S_HERO * TILE * TILE], 16, true);
}

void text(const char *s, int x, int y, bool large, uint16_t color)
{
    eadk_display_draw_string(s, eadk_point_t{(uint16_t)x, (uint16_t)y}, large, color, 0);
}

void drawOverlay()
{
    char buffer[32];

    if (state == TITLE)
    {
        text("DUNGEON", 108, 62, true, UI_ACCENT);
        text("OF NUMWORKS", 108, 88, true, UI_TEXT);
        text("ARROWS MOVE", 116, 132, false, UI_DIM);
        text("OK SELECT", 124, 148, false, UI_DIM);
        text("HOME QUITS", 124, 164, false, UI_DIM);
        return;
    }

    if (state == DEAD || state == WON)
    {
        eadk_display_push_rect_uniform(eadk_rect_t{56, 78, 208, 84}, 0x0000);
        if (state == DEAD)
            text("GAME OVER", 100, 92, true, rgb(232, 96, 84));
        else
            text("VICTORY", 112, 92, true, UI_ACCENT);
        snprintf(buffer, sizeof(buffer), "SCORE %d", score);
        text(buffer, 104, 124, false, UI_TEXT);
        text("PRESS OK", 112, 142, false, UI_DIM);
        return;
    }

    eadk_display_push_rect_uniform(eadk_rect_t{0, 0, SCREEN_W, 18}, 0x0000);
    snprintf(buffer, sizeof(buffer), "LV %d", level);
    text(buffer, 4, 5, false, UI_ACCENT);
    snprintf(buffer, sizeof(buffer), "HP %d", hp);
    text(buffer, 58, 5, false, hp > 1 ? UI_TEXT : rgb(232, 96, 84));
    snprintf(buffer, sizeof(buffer), "KEY %d/%d", keyCount - keysLeft, keyCount);
    text(buffer, 114, 5, false, keysLeft == 0 ? UI_ACCENT : UI_TEXT);
    snprintf(buffer, sizeof(buffer), "SCORE %d", score);
    text(buffer, 206, 5, false, UI_TEXT);

    if (nowMs < messageUntil)
    {
        snprintf(buffer, sizeof(buffer), "LEVEL %d", level);
        text(buffer, 132, 112, true, UI_ACCENT);
    }
}

}

extern const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "DUNGEON";
extern const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

int main(int argc, char *argv[])
{
    buildSprites();
    state = TITLE;
    level = 1;
    hp = 3;
    score = 0;
    nowMs = 0;
    lastOk = false;

    bool running = true;
    while (running)
    {
        nowMs = (uint32_t)eadk_timing_millis();
        EADK::Keyboard::State kbd = EADK::Keyboard::scan();
        running = !kbd.keyDown(EADK::Keyboard::Key::Home);

        bool ok = kbd.keyDown(EADK::Keyboard::Key::OK);
        bool okPress = ok && !lastOk;
        lastOk = ok;

        int dx = 0;
        int dy = 0;
        if (kbd.keyDown(EADK::Keyboard::Key::Left))
            dx = -1;
        else if (kbd.keyDown(EADK::Keyboard::Key::Right))
            dx = 1;
        else if (kbd.keyDown(EADK::Keyboard::Key::Up))
            dy = -1;
        else if (kbd.keyDown(EADK::Keyboard::Key::Down))
            dy = 1;

        update(dx, dy, okPress);
        render();
        eadk_display_push_rect(eadk_screen_rect, screen);
        drawOverlay();
        eadk_display_wait_for_vblank();
    }
    return 0;
}
