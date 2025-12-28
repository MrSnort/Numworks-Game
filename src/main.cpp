#include "eadkpp.h"
#include "ressources.h"
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include <algorithm>
#include <string>

uint8_t map2[6][6] = {
        //side wall left,left begin of the wall,2x2 core wall,right end of the wall,side wall right
        {1,2,47,48,3,4},
        {10,11,56,57,12,13},
        {37,20,65,66,21,40},
        {10,29,76,76,30,13}, //29 left corner brick,76 the middle,30 right
        {37,50,5,5,45,40},
        {46,3,48,47,2,49}
};

uint8_t map3[15][15];

int camx, camy;

//the function to push a part of the tilemap to a specific location in the framebuffer
//x,y coordinate in the framebuffer
//x2,y2 coordinate in the tilemap
//w,h width and height of part of tilemap to draw
//TODO : maybe do calculus one time not every loop
inline void PushFramebuffer(uint16_t *fb,int x, int y,uint16_t x2,uint16_t y2,uint16_t w,uint16_t h){
    //camera
    x-=camx;
    y-= camy;
    
    // Screen bounds
    if (x >= 320 || y >= 240) return; //down and right
    if (x+w < 0 || y+h < 0) return; //up and left

    // Clamp width & height
    if (x + w > 320)  w = 320 - x;
    if (y + h > 240)  h = 240 - y;

    //fix a bug when go outside the left part of the screen it starts drawing on the other side and then stop when fully outside
    if (x < 0) {
        x2 += -x;
        w  -= -x;
        x = 0;
    }
    
    if (y < 0) {
        int dy = -y;
        y = 0;
        y2 += dy;
        h  -= dy;
    }

    const uint16_t sizeofline = w*2; //unint16 takes two bytes

    for (uint16_t i = 0; i < h; i++){ //add part of the tilemap to framebuffer horizontal line by horizontal line up to bottom
        memcpy(&fb[(y+i)*320+x] ,&tilemap[(i+y2)*144+x2],sizeofline); //320 is the width of the screen and 144 the width of the tilemap
    }
}

void GenMap(){
    memset(map3,0,sizeof(map3));
    int height = 6 + eadk_random() % 9;
    int width = 6 + eadk_random() % 9;
    //memset(map3,5,sizeof(map3)); //floor gen
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            map3[y][x] = 5;
        }
    }
    for (int i = 0;i < width; i+=2){
        map3[0][i] = 47; //top wall
        map3[0][i+1] = 48;
        map3[height][i] = 47; //bottom wall
        map3[height][i+1] = 48;
        map3[1][i] = 56; //top wall 
        map3[1][i+1] = 57;
        map3[2][i] = 65; //top wall 
        map3[2][i+1] = 66;
        map3[3][i] = 73; // brick on the floor up
        map3[3][i+1] = 73;
    }
    for (int j = 0;j < height; j+=2){
        map3[j][0] = 10; //left wall
        map3[j+1][0] = 37;
        map3[j][width] = 13; //right wall
        map3[j+1][width] = 40;
    }
    for (int j = 3;j < height; j++){
        map3[j][1] = 50; //brick on the floor left
        map3[j][width-1] = 45; //brick on the floor right
    }
    map3[0][0] = 1; // top left corner
    map3[0][1] = 2;
    map3[0][width-1] = 3;
    map3[0][width] = 4; // top right corner
    map3[1][1] = 11;
    map3[1][width-1] = 12;
    map3[3][1] = 29;
    map3[3][width-1] = 30;
    map3[height][0] = 46; //bottom left corner 
    map3[height][width] = 49; //bottom right corner 
}

void DrawMap(uint16_t *fb,uint16_t x,uint16_t y){
    for (uint8_t i = 0;i < 15;i++){
        for (uint8_t j = 0;j < 15;j++){
            PushFramebuffer(fb,j*16+x,i*16+y,(map3[i][j]%9)*16,int(map3[i][j] / 9)*16,16,16);
        }
    }
}

//Numworks stuff
extern const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "GAME"; //name of the app
extern const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

int main(int argc, char *argv[])
{
    bool running = true;
    int x = 0; //player coordinate
    int y = 0; //player coordinate
    char camychar[32];
    eadk_color_t framebuffer[320*240] = {0x07E0};
    GenMap();
    
    
    //game loop
    while (running)
    {
        //home button
        EADK::Keyboard::State kbdState = EADK::Keyboard::scan();
        running = !kbdState.keyDown(EADK::Keyboard::Key::Home);

        //clear framebuffer
        std::fill_n(framebuffer,sizeof(framebuffer)/2,0x3165);
        
        //draw map
        DrawMap(framebuffer,0,0);

        //input
        if (kbdState.keyDown(EADK::Keyboard::Key::Right)){
            x++;
        }
        if (kbdState.keyDown(EADK::Keyboard::Key::Left)){
            x--;
        }
        if (kbdState.keyDown(EADK::Keyboard::Key::Up)){
            y--;
        }
        if (kbdState.keyDown(EADK::Keyboard::Key::Down)){
            y++;
        }
        if (kbdState.keyDown(EADK::Keyboard::Key::OK)){
            GenMap();
        }
        camx = x;
        camy = y;
        snprintf(camychar,sizeof(camychar),"%d",y+camy);
        PushFramebuffer(framebuffer,0,0,80,144,16,16); //player update (torch for test)

        //PushFramebuffer(framebuffer,50,50,32,144,48,48); //push the water tile in the framebuffer
        eadk_display_wait_for_vblank(); //wait for the beginning of the next frame
        eadk_display_push_rect(eadk_screen_rect, framebuffer); //push framebuffer to screen
        eadk_display_draw_string(camychar,eadk_point_t{0,0},false,eadk_color_green,0);
    }
}