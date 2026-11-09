extern "C" {
#include "eadk.h"
}
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr int W = EADK_SCREEN_WIDTH;
constexpr int H = EADK_SCREEN_HEIGHT;
constexpr int SCALE = 3;

const uint8_t font[][5] = {
  {0x00, 0x00, 0x00, 0x00, 0x00},
  {0x7E, 0x09, 0x09, 0x09, 0x7E},
  {0x7F, 0x49, 0x49, 0x49, 0x36},
  {0x3E, 0x41, 0x41, 0x41, 0x22},
  {0x7F, 0x41, 0x41, 0x41, 0x3E},
  {0x7F, 0x49, 0x49, 0x49, 0x41},
  {0x7F, 0x09, 0x09, 0x09, 0x01},
  {0x3E, 0x41, 0x49, 0x49, 0x3A},
  {0x7F, 0x08, 0x08, 0x08, 0x7F},
  {0x00, 0x41, 0x7F, 0x41, 0x00},
  {0x20, 0x40, 0x41, 0x3F, 0x01},
  {0x7F, 0x08, 0x14, 0x22, 0x41},
  {0x7F, 0x40, 0x40, 0x40, 0x40},
  {0x7F, 0x02, 0x0C, 0x02, 0x7F},
  {0x7F, 0x02, 0x04, 0x08, 0x7F},
  {0x3E, 0x41, 0x41, 0x41, 0x3E},
  {0x7F, 0x09, 0x09, 0x09, 0x06},
  {0x3E, 0x41, 0x51, 0x21, 0x5E},
  {0x7F, 0x09, 0x19, 0x29, 0x46},
  {0x46, 0x49, 0x49, 0x49, 0x31},
  {0x01, 0x01, 0x7F, 0x01, 0x01},
  {0x3F, 0x40, 0x40, 0x40, 0x3F},
  {0x1F, 0x20, 0x40, 0x20, 0x1F},
  {0x7F, 0x20, 0x18, 0x20, 0x7F},
  {0x63, 0x14, 0x08, 0x14, 0x63},
  {0x03, 0x04, 0x78, 0x04, 0x03},
  {0x61, 0x51, 0x49, 0x45, 0x43},
  {0x3E, 0x51, 0x4D, 0x43, 0x3E},
  {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x41, 0x49, 0x49, 0x49, 0x36},
  {0x18, 0x14, 0x12, 0x7F, 0x10},
  {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30},
  {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E},
  {0x00, 0x00, 0x36, 0x00, 0x00},
  {0x40, 0x30, 0x08, 0x06, 0x01},
  {0x00, 0x00, 0x5F, 0x00, 0x00},
  {0x08, 0x08, 0x08, 0x08, 0x08},
  {0x00, 0x00, 0x60, 0x60, 0x00},
  {0x02, 0x01, 0x51, 0x09, 0x06},
  {0x08, 0x14, 0x22, 0x41, 0x00},
  {0x00, 0x41, 0x22, 0x14, 0x08},
};

const char fontChars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/!-.?<>";

uint16_t fb[W * H];
Display *dpy;
Window win;
GC gc;
XImage *image;
uint32_t *pixels;
uint64_t keysDown;
timespec frameTime;
uint32_t rngState = 0x9E3779B9u;

uint64_t nowUs()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000ull + (uint64_t)t.tv_nsec / 1000ull;
}

void addMs(timespec &t, long ms)
{
    t.tv_nsec += ms * 1000000L;
    while (t.tv_nsec >= 1000000000L)
    {
        t.tv_nsec -= 1000000000L;
        t.tv_sec++;
    }
}

uint32_t expand5(uint16_t v)
{
    return ((v & 0x1F) * 255u) / 31u;
}

uint32_t expand6(uint16_t v)
{
    return ((v & 0x3F) * 255u) / 63u;
}

void present()
{
    for (int y = 0; y < H; y++)
    {
        uint32_t *row = pixels + (size_t)y * SCALE * W * SCALE;
        for (int x = 0; x < W; x++)
        {
            uint16_t c = fb[y * W + x];
            uint32_t r = expand5(c >> 11);
            uint32_t g = expand6(c >> 5);
            uint32_t b = expand5(c);
            uint32_t color = (r << 16) | (g << 8) | b;
            uint32_t *dst = row + (size_t)x * SCALE;
            for (int sy = 0; sy < SCALE; sy++)
            {
                uint32_t *line = dst + (size_t)sy * W * SCALE;
                for (int sx = 0; sx < SCALE; sx++)
                    line[sx] = color;
            }
        }
    }
    XPutImage(dpy, win, gc, image, 0, 0, 0, 0, W * SCALE, H * SCALE);
    XFlush(dpy);
}

void init()
{
    dpy = XOpenDisplay(nullptr);
    if (!dpy)
    {
        fprintf(stderr, "cannot open display\n");
        exit(1);
    }
    int scr = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, W * SCALE, H * SCALE, 0, 0, BlackPixel(dpy, scr));
    XStoreName(dpy, win, "NumWorks EADK Emulator");
    XSizeHints *hints = XAllocSizeHints();
    hints->flags = PMinSize | PMaxSize;
    hints->min_width = hints->max_width = W * SCALE;
    hints->min_height = hints->max_height = H * SCALE;
    XSetWMNormalHints(dpy, win, hints);
    XFree(hints);
    XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask);
    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    XMapWindow(dpy, win);
    gc = XCreateGC(dpy, win, 0, nullptr);
    pixels = (uint32_t *)calloc((size_t)W * SCALE * H * SCALE, sizeof(uint32_t));
    image = XCreateImage(dpy, DefaultVisual(dpy, scr), (unsigned)DefaultDepth(dpy, scr), ZPixmap, 0,
                         (char *)pixels, (unsigned)(W * SCALE), (unsigned)(H * SCALE), 32, 0);
    XFlush(dpy);
    clock_gettime(CLOCK_MONOTONIC, &frameTime);
    rngState ^= (uint32_t)nowUs();
}

void pump()
{
    while (XPending(dpy))
    {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == ClientMessage)
            exit(0);
        if (ev.type != KeyPress && ev.type != KeyRelease)
            continue;
        KeySym k = XLookupKeysym(&ev.xkey, 0);
        int bit = -1;
        if (k == XK_Left)
            bit = eadk_key_left;
        else if (k == XK_Up)
            bit = eadk_key_up;
        else if (k == XK_Down)
            bit = eadk_key_down;
        else if (k == XK_Right)
            bit = eadk_key_right;
        else if (k == XK_Return || k == XK_KP_Enter || k == XK_space)
            bit = eadk_key_ok;
        else if (k == XK_BackSpace)
            bit = eadk_key_back;
        else if (k == XK_Escape)
            bit = eadk_key_home;
        if (bit >= 0)
        {
            if (ev.type == KeyPress)
                keysDown |= 1ull << bit;
            else
                keysDown &= ~(1ull << bit);
        }
    }
}

void fillRect(int x, int y, int w, int h, uint16_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w;
    int y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            fb[j * W + i] = color;
}

void drawChar(int x, int y, char ch, int scale, uint16_t fg, uint16_t bg)
{
    if (ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 32);
    const char *at = strchr(fontChars, ch);
    int index = at && ch ? (int)(at - fontChars) : 0;
    fillRect(x, y, 6 * scale, 8 * scale, bg);
    for (int col = 0; col < 5; col++)
    {
        uint8_t bits = font[index][col];
        for (int row = 0; row < 7; row++)
        {
            if (bits & (1 << row))
                fillRect(x + col * scale, y + row * scale, scale, scale, fg);
        }
    }
}

}

extern "C" {

eadk_keyboard_state_t eadk_keyboard_scan()
{
    if (!dpy)
        init();
    return keysDown;
}

void eadk_display_push_rect(eadk_rect_t rect, const eadk_color_t *pixelsSrc)
{
    if (!dpy)
        init();
    for (int j = 0; j < rect.height; j++)
    {
        int y = rect.y + j;
        if (y < 0 || y >= H)
            continue;
        for (int i = 0; i < rect.width; i++)
        {
            int x = rect.x + i;
            if (x < 0 || x >= W)
                continue;
            fb[y * W + x] = pixelsSrc[j * rect.width + i];
        }
    }
}

void eadk_display_push_rect_uniform(eadk_rect_t rect, eadk_color_t color)
{
    if (!dpy)
        init();
    fillRect(rect.x, rect.y, rect.width, rect.height, color);
}

void eadk_display_pull_rect(eadk_rect_t rect, eadk_color_t *pixelsDst)
{
    if (!dpy)
        init();
    for (int j = 0; j < rect.height; j++)
    {
        int y = rect.y + j;
        for (int i = 0; i < rect.width; i++)
        {
            int x = rect.x + i;
            pixelsDst[j * rect.width + i] = (x >= 0 && x < W && y >= 0 && y < H) ? fb[y * W + x] : 0;
        }
    }
}

bool eadk_display_wait_for_vblank()
{
    if (!dpy)
        init();
    present();
    addMs(frameTime, 16);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &frameTime, nullptr);
    timespec check;
    clock_gettime(CLOCK_MONOTONIC, &check);
    if ((uint64_t)check.tv_sec * 1000000000ull + (uint64_t)check.tv_nsec > (uint64_t)frameTime.tv_sec * 1000000000ull + (uint64_t)frameTime.tv_nsec)
        frameTime = check;
    pump();
    return true;
}

void eadk_display_draw_string(const char *str, eadk_point_t point, bool large, eadk_color_t textColor, eadk_color_t backgroundColor)
{
    if (!dpy)
        init();
    int scale = large ? 2 : 1;
    int x = point.x;
    int y = point.y;
    while (*str)
    {
        drawChar(x, y, *str, scale, textColor, backgroundColor);
        x += 6 * scale;
        str++;
    }
}

void eadk_timing_usleep(uint32_t us)
{
    timespec t;
    t.tv_sec = us / 1000000;
    t.tv_nsec = (long)(us % 1000000) * 1000L;
    nanosleep(&t, nullptr);
}

void eadk_timing_msleep(uint32_t ms)
{
    eadk_timing_usleep(ms * 1000);
}

uint64_t eadk_timing_millis()
{
    return nowUs() / 1000ull;
}

uint32_t eadk_random()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

void eadk_backlight_set_brightness(uint8_t) {}
uint8_t eadk_backlight_brightness() { return 255; }
bool eadk_battery_is_charging() { return false; }
uint8_t eadk_battery_level() { return 100; }
float eadk_battery_voltage() { return 4.2f; }
bool eadk_usb_is_plugged() { return false; }
eadk_event_t eadk_event_get(int32_t *) { return (eadk_event_t)-1; }

}
