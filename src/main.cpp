#include "eadkpp.h"
#include <stdint.h>
#include <stdbool.h>

extern const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "GAME";
extern const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

int main(int argc, char *argv[])
{
    EADK::Display::pushRectUniform(EADK::Screen::Rect, 0x922BFF);

    bool running = true;

    while (running)
    {
        EADK::Keyboard::State kbdState = EADK::Keyboard::scan();
        running = !kbdState.keyDown(EADK::Keyboard::Key::Home);
    }
}
