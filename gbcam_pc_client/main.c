
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "serial.h"
#include "debug.h"

//-------------------------------------------------------------------------------------

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

//-------------------------------------------------------------------------------------

//Window data
SDL_Window * mWindow;
SDL_Renderer * mRenderer;
SDL_GLContext mGLContext;
SDL_Texture * mTexture;
Uint32 mWindowID;

//-------------------------------------------------------------------------------------

//Camera settings
u8 trig_value = 0;
u8 reg1 = 0,reg4 = 0,reg5 = 0;
u16 exptime = 0x0500;

//-------------------------------------------------------------------------------------

//Keyboard events
int takepicture = 0, takeanalog = 0;
int readpicture = 0;
int dither_on = 1;
int debugpicture = 0;

//-------------------------------------------------------------------------------------

#define GBCAM_W (128)
#define GBCAM_H (112)

#define SCREEN_W (GBCAM_W*3 * 2)
#define SCREEN_H (GBCAM_H*3)

#define BIT(n) (1<<(n))

static unsigned char SCREEN_BUFFER[SCREEN_W*SCREEN_H*3];
static unsigned char GBCAM_BUFFER[GBCAM_W*GBCAM_H*3];

int WindowCreate(void)
{
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    //Create window
    mWindow = SDL_CreateWindow("Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if(mWindow != NULL)
    {
        SDL_SetWindowTitle(mWindow,"GBCam_Reverse");

        mGLContext = SDL_GL_CreateContext(mWindow);

        int oglIdx = -1;
        int nRD = SDL_GetNumRenderDrivers();
        int i;
        for(i = 0; i < nRD; i++)
        {
            SDL_RendererInfo info;
            if(!SDL_GetRenderDriverInfo(i, &info)) if(!strcmp(info.name, "opengl"))
            {
                oglIdx = i;
                break;
            }
        }

        //Create renderer for window
        mRenderer = SDL_CreateRenderer(mWindow, oglIdx, SDL_RENDERER_ACCELERATED  /*| SDL_RENDERER_PRESENTVSYNC*/);

        if(mRenderer == NULL)
        {
            Debug_Log("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(mWindow);
            mWindow = NULL;
        }
        else
        {
            mWindowID = SDL_GetWindowID(mWindow); //Grab window identifier

            mTexture = SDL_CreateTexture(mRenderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,
                                         SCREEN_W,SCREEN_H);
            if(mTexture == NULL)
            {
                Debug_Log("Couldn't create texture! SDL Error: %s\n", SDL_GetError());
                SDL_DestroyWindow(mWindow); // this message shows even if everything is correct... weird...
                SDL_GL_DeleteContext(mGLContext);
                mWindow = NULL;
            }
            else
            {
                return 0;
            }
        }
    }
    else
    {
        Debug_Log("Window could not be created! SDL Error: %s\n", SDL_GetError());
    }

    return -1;
}

void WindowClose(void)
{
    SDL_DestroyTexture(mTexture);
    SDL_GL_DeleteContext(mGLContext);
    SDL_DestroyWindow(mWindow);
}

void WindowDrawQuadButton(int ix, int iy, unsigned char on)
{
    on = (on != 0) ? 0 : 2;

    int xbase = GBCAM_W*3 + ix*32;
    int ybase = iy*32;

    struct {
        unsigned char r, g, b;
    } reg_color[4][8] = {
        { {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{255,255,255},{255,255,255},{255,0,0} },
        { {255,0,0},{0,255,255},{0,255,255},{0,255,0},{0,255,0},{0,255,0},{0,255,0},{0,255,0} },
        { {0,0,255},{0,0,255},{0,0,255},{0,0,255},{255,255,255},{255,255,0},{255,255,0},{255,255,0} },
        { {0,255,0},{0,255,0},{255,255,0},{255,255,0},{255,255,0},{255,255,0},{255,255,0},{255,255,0} }
    };

    unsigned char r = reg_color[iy][ix].r >> on;
    unsigned char g = reg_color[iy][ix].g >> on;
    unsigned char b = reg_color[iy][ix].b >> on;

    int i,j;
    for(j = 0; j < 32; j++) for(i = 0; i < 32; i++)
    {
        int x = xbase + i;
        int y = ybase + j;
        int offset = ( x + y*SCREEN_W ) * 3;
        unsigned char * p = &(SCREEN_BUFFER[offset]);
        *p++ = r;
        *p++ = g;
        *p = b;
    }
}

void WindowRender(void)
{
    memset(SCREEN_BUFFER,0x10,sizeof(SCREEN_BUFFER));

    int i;
    for(i = 0; i < 8; i ++)
    {
        WindowDrawQuadButton(i,0,trig_value&BIT(7-i));
        WindowDrawQuadButton(i,1,reg1&BIT(7-i));
        WindowDrawQuadButton(i,2,reg4&BIT(7-i));
        WindowDrawQuadButton(i,3,reg5&BIT(7-i));
    }

    int j;
    for(j = 0; j < GBCAM_H*3; j++) for(i = 0; i < GBCAM_W*3; i++)
    {
        SCREEN_BUFFER[(i+j*SCREEN_W)*3 + 0] = GBCAM_BUFFER[( (i/3)+(j/3)*GBCAM_W)*3 + 0];
        SCREEN_BUFFER[(i+j*SCREEN_W)*3 + 1] = GBCAM_BUFFER[( (i/3)+(j/3)*GBCAM_W)*3 + 1];
        SCREEN_BUFFER[(i+j*SCREEN_W)*3 + 2] = GBCAM_BUFFER[( (i/3)+(j/3)*GBCAM_W)*3 + 2];
    }

    SDL_UpdateTexture(mTexture, NULL, (void*)SCREEN_BUFFER, SCREEN_W*3);
    SDL_RenderClear(mRenderer);

    SDL_Rect src;
    src.x = 0; src.y = 0; src.w = SCREEN_W; src.h = SCREEN_H;
    SDL_Rect dst;
    dst.x = 0; dst.y = 0; dst.w = SCREEN_W; dst.h = SCREEN_H;

    SDL_RenderCopy(mRenderer, mTexture, &src, &dst);
    SDL_RenderPresent(mRenderer);
}

//-------------------------------------------------------------------------------------

unsigned char c1 = 0x40, c2 = 0x80, c3 = 0xC0;

static int HandleEvents(void)
{
    SDL_Event e;

    while(SDL_PollEvent(&e))
    {
        if(e.type == SDL_QUIT)
        {
            return 1;
        }
        else if(e.type == SDL_KEYDOWN)
        {
            switch(e.key.keysym.sym)
            {
                case SDLK_KP_7: c1++; break;
                case SDLK_KP_4: c1--; break;
                case SDLK_KP_8: c2++; break;
                case SDLK_KP_5: c2--; break;
                case SDLK_KP_9: c3++; break;
                case SDLK_KP_6: c3--; break;

                case SDLK_ESCAPE:
                    return 1;

                case SDLK_r:
                    reg1 = 0xE8;
                    reg4 = 0x24;
                    reg5 = 0xBF;
                    exptime = 0x1500;
                    break;

                case SDLK_t:
                    trig_value ++;
                    break;
                case SDLK_g:
                    trig_value --;
                    break;

                case SDLK_q:
                    reg1 ++;
                    break;
                case SDLK_a:
                    reg1 --;
                    break;

                case SDLK_w:
                    reg4 ++;
                    break;
                case SDLK_s:
                    reg4 --;
                    break;

                case SDLK_e:
                    reg5 ++;
                    break;
                case SDLK_d:
                    reg5 --;
                    break;

                case SDLK_z:
                    dither_on = !dither_on;
                    break;

                case SDLK_UP:
                    exptime +=0x40;
                    break;
                case SDLK_DOWN:
                    exptime -=0x40;
                    break;

                case SDLK_RETURN:
                    takepicture = 1;
                    break;
                case SDLK_BACKSPACE:
                    takeanalog = 1;
                    break;
                case SDLK_SPACE:
                    readpicture = 1;
                    break;
                case SDLK_p:
                    debugpicture = 1;
                    break;
            }
        }
        else if(e.type == SDL_MOUSEBUTTONDOWN)
        {
            if(e.button.button == 1)
            {
                int ix = (e.button.x - GBCAM_W*3) / 32;
                int iy = e.button.y / 32;
                if( (ix < 8) && (iy < 4) )
                {
                    if(iy == 0) { trig_value ^= BIT(7-ix); trig_value &= 0x07; }
                    else if(iy == 1) reg1 ^= BIT(7-ix);
                    else if(iy == 2) reg4 ^= BIT(7-ix);
                    else if(iy == 3) reg5 ^= BIT(7-ix);
                }
            }
        }
    }

    return 0;
}

static int Init(void)
{
    Debug_Init();

    //Initialize SDL
    if( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
    {
        Debug_Log( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
        return 1;
    }
    atexit(SDL_Quit);

    if(WindowCreate() != 0)
        return 1;
    atexit(WindowClose);

    return 0;
}

//-------------------------------------------------------------------------------------

unsigned char picturedata[16*14*8*8]; // max( 16*8*14*8, 16*14*16 ) sensor pixels , tile bytes

void ConvertTilesToBitmap(void)
{
    //Convert to bitmap
    memset(GBCAM_BUFFER,0,sizeof(GBCAM_BUFFER));

    const int gb_pal_colors[4] = { 255, 168, 80, 0 };

    int y, x;
    for(y = 0; y < 14*8; y++) for(x = 0; x < 16*8; x ++)
    {
        int basetileaddr = ( ((y>>3)*16+(x>>3)) * 16 );
        int baselineaddr = basetileaddr + ((y&7) << 1);

        unsigned char data = picturedata[baselineaddr];
        unsigned char data2 = picturedata[baselineaddr+1];

        int x_ = 7-(x&7);

        int color = ( (data >> x_) & 1 ) |  ( ( (data2 >> x_)  << 1) & 2);

        int bufindex = (y*GBCAM_W+x)*3;
        GBCAM_BUFFER[bufindex+0] = gb_pal_colors[color];
        GBCAM_BUFFER[bufindex+1] = gb_pal_colors[color];
        GBCAM_BUFFER[bufindex+2] = gb_pal_colors[color];
    }
}

void ConvertAnalogToBitmap(void)
{
    memset(GBCAM_BUFFER,0,sizeof(GBCAM_BUFFER));

    int y, x;
    for(y = 0; y < 14*8; y++) for(x = 0; x < 16*8; x ++)
    {
        int index = y*16*8 + x;

        int color = picturedata[index];

        int bufindex = (y*GBCAM_W+x)*3;
        GBCAM_BUFFER[bufindex+0] = color;
        GBCAM_BUFFER[bufindex+1] = color;
        GBCAM_BUFFER[bufindex+2] = color;
    }
}

//-------------------------------------------------------------------------------------

static inline unsigned int asciihextoint(char c)
{
  if((c >= '0') && (c <= '9')) return c - '0';
  if((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
  return 0;
}

int readByte(unsigned int addr)
{
    char str[50];

    char data[2];

    sprintf(str,"R%04X.",addr&0xFFFF);
    if(SerialWriteData(str,6) == 0)
    {
        Debug_Log("SerialWriteData() error in readByte()");
        return -1;
    }

    while(SerialGetInQueue() < 2)
    {
        if(HandleEvents()) exit(0);
        SDL_Delay(1);
    }

    if(SerialReadData(data,2) != 2)
    {
        Debug_Log("SerialReadData() error in readByte()");
        return -1;
    }

    return (asciihextoint(data[0])<<4)|asciihextoint(data[1]);
}

void writeByte(unsigned int addr, unsigned int value)
{
    char str[50];
    sprintf(str,"W%04X%02X.",addr&0xFFFF,value&0xFF);
    SerialWriteData(str,8);
    return;
}

#define ramEnable() writeByte(0x0000,0x0A)

#define ramDisable() writeByte(0x0000,0x00)

void setRegisterMode(void)
{
    SerialWriteData("Z.",2);
    return;
}

void setRamModeBank0(void)
{
    SerialWriteData("X.",2);
    return;
}

//-------------------------------------------------------------------------------------

void readPicture(void)
{
    SDL_SetWindowTitle(mWindow,"Reading picture...");

    if(SerialWriteData("P.",2)==0)
    {
        Debug_Log("SerialWriteData <P.> error.");
        return;
    }

    int i;
    for(i = 0; i < 16*14*16; i++)
    {
        while(SerialGetInQueue() < 1)
        {
            if(HandleEvents()) exit(0);
            SDL_Delay(1);
        }

        unsigned char data;
        if(SerialReadData((char*)&data,1) != 1)
        {
            Debug_Log("SerialReadData() error in readPicture()");
            return;
        }

        picturedata[i] = data;
    }

    return;
}

void readThumbnail(void) // 2 rows of tiles
{
    SDL_SetWindowTitle(mWindow,"Reading thumbnail...");

    if(SerialWriteData("T.",2)==0)
    {
        Debug_Log("SerialWriteData <P.> error.");
        return;
    }

    int i;
    for(i = 0; i < 16*2*16; i++)
    {
        while(SerialGetInQueue() < 1)
        {
            if(HandleEvents()) exit(0);
            SDL_Delay(1);
        }

        unsigned char data;
        if(SerialReadData((char*)&data,1) != 1)
        {
            Debug_Log("SerialReadData() error in readThumbnail()");
            return;
        }

        picturedata[i] = data;
    }

    return;
}

unsigned int waitPictureReady(void)
{
    setRegisterMode();

    SerialWriteData("F.",2);

    while(SerialGetInQueue() < 8)
    {
        if(HandleEvents()) exit(0);
        SDL_Delay(1);
    }

    char str[9];
    if(SerialReadData(str,8) != 8)
    {
        Debug_Log("SerialReadData() error in waitPictureReady()");
        return 0;
    }
    str[8] = '\0';

    unsigned int value;
    sscanf(str,"%u",&value);

    return value;
}

//-------------------------------------------------------------------------------------

void TakePictureAndTransfer(u8 trigger, u8 unk1, u16 exposure_time, u8 unk2, u8 unk3,
                            int dithering, int thumbnail)
{
    SDL_SetWindowTitle(mWindow,"Taking picture...");

    ramEnable();

    setRegisterMode();

    writeByte(0xA000,0x00);

    writeByte(0xA001,unk1);

    writeByte(0xA002,(exposure_time>>8)&0xFF);
    writeByte(0xA003,exposure_time&0xFF);

    writeByte(0xA004,unk2);

    writeByte(0xA005,unk3);

    //const unsigned char matrix[] = // high light
    //{
    //    0x89, 0x92, 0xA2, 0x8F, 0x9E, 0xC6, 0x8A, 0x95, 0xAB, 0x91, 0xA1, 0xCF,
    //    0x8D, 0x9A, 0xBA, 0x8B, 0x96, 0xAE, 0x8F, 0x9D, 0xC3, 0x8C, 0x99, 0xB7,
    //    0x8A, 0x94, 0xA8, 0x90, 0xA0, 0xCC, 0x89, 0x93, 0xA5, 0x90, 0x9F, 0xC9,
    //    0x8E, 0x9C, 0xC0, 0x8C, 0x98, 0xB4, 0x8E, 0x9B, 0xBD, 0x8B, 0x97, 0xB1
    //};

    const unsigned char matrix[48] = // low light
    {
        0x8C, 0x98, 0xAC, 0x95, 0xA7, 0xDB, 0x8E, 0x9B, 0xB7, 0x97, 0xAA, 0xE7,
        0x92, 0xA2, 0xCB, 0x8F, 0x9D, 0xBB, 0x94, 0xA5, 0xD7, 0x91, 0xA0, 0xC7,
        0x8D, 0x9A, 0xB3, 0x96, 0xA9, 0xE3, 0x8C, 0x99, 0xAF, 0x95, 0xA8, 0xDF,
        0x93, 0xA4, 0xD3, 0x90, 0x9F, 0xC3, 0x92, 0xA3, 0xCF, 0x8F, 0x9E, 0xBF
    };

    int i;
    for(i = 0; i < 48; i++)
    {
        if(dithering)
        {
            writeByte(0xA006+i,matrix[i]);
        }
        else
        {
            switch(i%3)
            {
                case 0: writeByte(0xA006+i,c1); break;
                case 1: writeByte(0xA006+i,c2); break;
                case 2: writeByte(0xA006+i,c3); break;
            }
            //writeByte(0xA006+i,matrix[i%3]);
        }
    }

    setRamModeBank0();

    char str[50];
    if(thumbnail)
        sprintf(str,"T%02X.",trigger&0xFF);
    else
        sprintf(str,"P%02X.",trigger&0xFF);

    if(SerialWriteData(str,4) == 0)
    {
        Debug_Log("SerialWriteData() error in TakePictureAndTransfer()");
        return;
    }

    while(SerialGetInQueue() < 1)
    {
        if(HandleEvents()) exit(0);
        SDL_Delay(1);
    }

    SDL_SetWindowTitle(mWindow,"Reading picture...");

    int size = 16 * (thumbnail ? 2 : 14) * 16;
    for(i = 0; i < size; i++)
    {
        while(SerialGetInQueue() < 1)
        {
            if(HandleEvents()) exit(0);
            SDL_Delay(1);
        }

        unsigned char data;
        if(SerialReadData((char*)&data,1) != 1)
        {
            Debug_Log("SerialReadData() error in TakePictureAndTransfer()");
            return;
        }

        picturedata[i] = data;
    }

    ramDisable();

    ConvertTilesToBitmap();
}

void TakePictureAnalogAndTransfer(u8 trigger, u8 unk1, u16 exposure_time, u8 unk2, u8 unk3,
                            int dithering)
{
    SDL_SetWindowTitle(mWindow,"Taking picture...");

    ramEnable();

    setRegisterMode();

    writeByte(0xA000,0x00);

    writeByte(0xA001,unk1);

    writeByte(0xA002,(exposure_time>>8)&0xFF);
    writeByte(0xA003,exposure_time&0xFF);

    writeByte(0xA004,unk2);

    writeByte(0xA005,unk3);

    char str[50];
    sprintf(str,"A%02X.",trigger&0xFF);

    if(SerialWriteData(str,4) == 0)
    {
        Debug_Log("SerialWriteData() error in TakePictureAnalogAndTransfer()");
        return;
    }

    while(SerialGetInQueue() < 1)
    {
        if(HandleEvents()) exit(0);
        SDL_Delay(1);
    }

    SDL_SetWindowTitle(mWindow,"Reading picture...");

    int size = 16*8 * 14*8;
    int i;
    for(i = 0; i < size; i++)
    {
        while(SerialGetInQueue() < 1)
        {
            if(HandleEvents()) exit(0);
            SDL_Delay(1);
        }

        unsigned char data;
        if(SerialReadData((char*)&data,1) != 1)
        {
            Debug_Log("SerialReadData() error in TakePictureAnalogAndTransfer()");
            return;
        }

        picturedata[i] = data;

        ConvertAnalogToBitmap();
    }
}

void TakePicture(u8 trigger, u8 unk1, u16 exposure_time, u8 unk2, u8 unk3,
                 int dithering_enabled)
{
    SDL_SetWindowTitle(mWindow,"Taking picture...");

    ramEnable();
    setRegisterMode();

    writeByte(0xA000,0x00);

    writeByte(0xA001,unk1);

    writeByte(0xA002,(exposure_time>>8)&0xFF);
    writeByte(0xA003,exposure_time&0xFF);

    writeByte(0xA004,unk2);

    writeByte(0xA005,unk3);

    //const unsigned char matrix[] = // high light
    //{
    //    0x89, 0x92, 0xA2, 0x8F, 0x9E, 0xC6, 0x8A, 0x95, 0xAB, 0x91, 0xA1, 0xCF,
    //    0x8D, 0x9A, 0xBA, 0x8B, 0x96, 0xAE, 0x8F, 0x9D, 0xC3, 0x8C, 0x99, 0xB7,
    //    0x8A, 0x94, 0xA8, 0x90, 0xA0, 0xCC, 0x89, 0x93, 0xA5, 0x90, 0x9F, 0xC9,
    //    0x8E, 0x9C, 0xC0, 0x8C, 0x98, 0xB4, 0x8E, 0x9B, 0xBD, 0x8B, 0x97, 0xB1
    //};

    const unsigned char matrix[48] = // low light
    {
        0x8C, 0x98, 0xAC, 0x95, 0xA7, 0xDB, 0x8E, 0x9B, 0xB7, 0x97, 0xAA, 0xE7,
        0x92, 0xA2, 0xCB, 0x8F, 0x9D, 0xBB, 0x94, 0xA5, 0xD7, 0x91, 0xA0, 0xC7,
        0x8D, 0x9A, 0xB3, 0x96, 0xA9, 0xE3, 0x8C, 0x99, 0xAF, 0x95, 0xA8, 0xDF,
        0x93, 0xA4, 0xD3, 0x90, 0x9F, 0xC3, 0x92, 0xA3, 0xCF, 0x8F, 0x9E, 0xBF
    };

    int i;
    for(i = 0; i < 48; i++)
    {
        if(dithering_enabled)
        {
            writeByte(0xA006+i,matrix[i]);
        }
        else
        {
            writeByte(0xA006+i,matrix[i%3]);
        }
    }

    writeByte(0xA000,trigger);

    unsigned int clks = 0;
    while(1)
    {
        clks += waitPictureReady();
        int a = readByte(0xA000);
        if((a & 1) == 0) break;
        //sprintf(text,"%d - %u",a,clks);
        //SDL_SetWindowTitle(mWindow,text);
    }

    ramDisable();
}

void TakePictureDebug(u8 trigger, u8 unk1, u16 exposure_time, u8 unk2, u8 unk3)
{
    SDL_SetWindowTitle(mWindow,"Taking picture...");

    ramEnable();
    setRegisterMode();

    writeByte(0xA000,0x00);

    writeByte(0xA001,unk1);

    writeByte(0xA002,(exposure_time>>8)&0xFF);
    writeByte(0xA003,exposure_time&0xFF);

    writeByte(0xA004,unk2);

    writeByte(0xA005,unk3);

    writeByte(0xA000,trigger);

    if(SerialWriteData("C.",2) == 0)
    {
        Debug_Log("SerialWriteData() error in TakePictureDebug()");
        return;
    }

    ramDisable();
}

void TransferPicture(void)
{
    ramEnable();
    setRamModeBank0();
    readPicture();
    ramDisable();

    ConvertTilesToBitmap();
}

void TransferThumbnail(void)
{
    ramEnable();
    setRamModeBank0();
    readThumbnail();
    ramDisable();

    ConvertTilesToBitmap();
}

void ClearPicture(void)
{
    memset(picturedata,0xFF,sizeof(picturedata));
    ConvertTilesToBitmap();
    WindowRender();
    SDL_Delay(1);
}

//-------------------------------------------------------------------------------------

#define FLOAT_MS_PER_FRAME ((float)1000.0/(float)30.0)

int main(int argc, char * argv[])
{
    if(Init() != 0)
        return 1;

    SDL_SetWindowTitle(mWindow,"Init...");

    ClearPicture();

    SerialCreate("COM4");

    if(SerialIsConnected())
		Debug_Log("We're connected\n");
    else
        return 2;

    SDL_SetWindowTitle(mWindow,"Inited!");

/*
int bank = 0;
writeByte(0x2000,bank);
//Dump
FILE * f = fopen("out.bin","wb");

//void * g = malloc(0x4000*bank); fwrite(g,0x4000*bank,1,f); free(g);

int i;
for(i = 0x4000; i < 0x8000; i++)
{
    unsigned char a = readByte(i);
    fwrite(&a,1,1,f);

    if((i&0x1F) == 0)
    {
        char str[10]; sprintf(str,"0x%X",i);
        SDL_SetWindowTitle(mWindow,str);
        if(HandleEvents()) break;
        WindowRender();
    }
}
fclose(f);
return 123;
*/
    if(0)
    {
        ramEnable();
        setRamModeBank0();
        int i;
        for(i = 0; i < 16*14*16; i++)
        {
            writeByte(0xA100+i,i&0xFF);

            if((i&63) == 0) {
            char str[10]; sprintf(str,"%d",(i*100)/(16*14*16));
            SDL_SetWindowTitle(mWindow,str);
            }
        }
        ramDisable();
    }

    if(0) // RAM test
    {
        int failed = 0;

        ramEnable();
        setRamModeBank0();
        int i;
        for(i = 0; i < 128; i++)
        {
            writeByte(0xA100+i,0xFF-i);

            if((i&63) == 0) {
            char str[10]; sprintf(str,"%d",(i*100)/(128));
            SDL_SetWindowTitle(mWindow,str);
            }
        }
        for(i = 0; i < 128; i++)
        {
            if(readByte(0xA100+i) != ((0xFF-i)&0xFF))
            {
                failed = 1;
                break;
            }

            if((i&63) == 0) {
            char str[10]; sprintf(str,"%d",(i*100)/(128));
            SDL_SetWindowTitle(mWindow,str);
            }
        }
        ramDisable();

        SDL_SetWindowTitle(mWindow,failed ? "RAM test FAILED" : "RAM test OK");
        SDL_Delay(2000);
    }

    trig_value = 0x03;

    reg1 = 0xE8; // 0x00, 0x0A, 0x20, 0x24, 0x28, 0xE4, 0xE8
    reg4 = 0x24;
    /* 0x05,
       0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x03, 0x04, 0x05, 0x06, 0x07,
       0x23, 0x24, 0x25, 0x26, 0x27, 0x23, 0x24, 0x25, 0x26, 0x27,
       0x23, 0x24, 0x25, 0x26, 0x27, 0x03, 0x04, 0x05, 0x06, 0x07,
       0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x07
    */
    reg5 = 0xBF;
    /*
            0xBF,
            0x80, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE,
            0xBF, 0xBF, 0x3F, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE,
            0xBF, 0xBF, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE,
            0xBF, 0xBF, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE, 0xBF, 0xBF,
            0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE, 0xBF, 0xBF, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE,
            0xBF, 0xBF, 0x80, 0xBF, 0xA0, 0xB0, 0xB8, 0xBC, 0xBE, 0xBF, 0xBF, 0x3F, 0xBF
    */
    exptime = 0x1500;

    float waitforticks = 0;
    int exit = 0;
    while(!exit)
    {
        //Handle events for all windows
        exit = HandleEvents();

        //-------------------

        //TakePictureAndTransfer(0x03,0xE4,0,0x07,0xBF,1,0); //Base

        char str[100];
        sprintf(str,"0x%02X - 0x%02X 0x%02X 0x%02X 0x%04X - Dither %d | %02X %02X %02X",
                    trig_value, reg1,reg4,reg5,exptime&0xFFFF,dither_on,
                    c1,c2,c3);
        SDL_SetWindowTitle(mWindow,str);

        if(takepicture)
        {
            takepicture = 0;
            //ClearPicture();
            TakePictureAndTransfer(trig_value,reg1,exptime&0xFFFF,reg4,reg5,dither_on,0);
        }
        else if(takeanalog)
        {
            takeanalog = 0;
            //ClearPicture();
            TakePictureAnalogAndTransfer(trig_value,reg1,exptime&0xFFFF,reg4,reg5,dither_on);
        }
        if(readpicture)
        {
            readpicture = 0;
            //ClearPicture();
            TransferPicture();
        }
        if(debugpicture)
        {
            debugpicture = 0;
            TakePictureDebug(trig_value,reg1,exptime&0xFFFF,reg4,reg5);
        }

        //-------------------

        WindowRender();

        while(waitforticks >= SDL_GetTicks()) SDL_Delay(1);

        int ticksnow = SDL_GetTicks();
        if(waitforticks < (ticksnow - FLOAT_MS_PER_FRAME)) // if lost a frame or more
            waitforticks = ticksnow + FLOAT_MS_PER_FRAME;
        else
            waitforticks += FLOAT_MS_PER_FRAME;
    }

    return 0;
}

