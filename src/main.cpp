#include "eadkpp.h"
#include "ressources.h"
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>

//the function to push a part of the tilemap to a specific location in the framebuffer
//x,y coordinate in the framebuffer
//x2,y2 coordinate in the tilemap
//w,h width and height of part of tilemap to draw
//TODO : maybe do calculus one time not every loop
inline void PushFramebuffer(uint16_t *fb,uint16_t x, uint16_t y,uint16_t x2,uint16_t y2,uint16_t w,uint16_t h){
    const uint16_t sizeofline = w*2; //unint16 takes two bytes
    for (uint16_t i = 0; i < h; i++){ //add part of the tilemap to framebuffer horizontal line by horizontal line up to bottom
        memcpy(&fb[(y+i)*320+x] ,&tilemap[(i+y2)*144+x2],sizeofline); //320 is the width of the screen and 144 the width of the tilemap
    }
}

//Numworks stuff
extern const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "GAME"; //name of the app
extern const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

int main(int argc, char *argv[])
{
    bool running = true;
    eadk_color_t framebuffer[320*240] = {0x07E0};

    //game loop
    while (running)
    {
        EADK::Keyboard::State kbdState = EADK::Keyboard::scan();
        running = !kbdState.keyDown(EADK::Keyboard::Key::Home);

        PushFramebuffer(framebuffer,50,50,32,144,48,48); //push the water tile in the framebuffer
        eadk_display_wait_for_vblank(); //wait for the beginning of the next frame
        eadk_display_push_rect(eadk_screen_rect, framebuffer); //push framebuffer to screen
    }
}
