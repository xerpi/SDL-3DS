/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_3DS

#include "SDL_hints.h"
#include "../SDL_sysrender.h"
#include <3ds.h>
/*
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pspge.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vram.h>*/


/* 3DS renderer implementation, based on the CTRULIB  */


extern int SDL_RecreateWindow(SDL_Window * window, Uint32 flags);


static SDL_Renderer *N3DS_CreateRenderer(SDL_Window * window, Uint32 flags);
static void N3DS_WindowEvent(SDL_Renderer * renderer,
                             const SDL_WindowEvent *event);
static int N3DS_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int N3DS_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                              const SDL_Rect * rect, const void *pixels,
                              int pitch);
static int N3DS_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, void **pixels, int *pitch);
static void N3DS_UnlockTexture(SDL_Renderer * renderer,
                               SDL_Texture * texture);
static int N3DS_SetRenderTarget(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static int N3DS_UpdateViewport(SDL_Renderer * renderer);
static int N3DS_RenderClear(SDL_Renderer * renderer);
static int N3DS_RenderDrawPoints(SDL_Renderer * renderer,
                                 const SDL_FPoint * points, int count);
static int N3DS_RenderDrawLines(SDL_Renderer * renderer,
                                const SDL_FPoint * points, int count);
static int N3DS_RenderFillRects(SDL_Renderer * renderer,
                                const SDL_FRect * rects, int count);
static int N3DS_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                           const SDL_Rect * srcrect,
                           const SDL_FRect * dstrect);
static int N3DS_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch);
static int N3DS_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                         const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
static void N3DS_RenderPresent(SDL_Renderer * renderer);
static void N3DS_DestroyTexture(SDL_Renderer * renderer,
                                SDL_Texture * texture);
static void N3DS_DestroyRenderer(SDL_Renderer * renderer);

/*
SDL_RenderDriver N3DS_RenderDriver = {
    N3DS_CreateRenderer,
    {
     "PSP",
     (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE),
     1,
     {SDL_PIXELFORMAT_ABGR8888},
     0,
     0}
};
*/
SDL_RenderDriver N3DS_RenderDriver = {
    .CreateRenderer = N3DS_CreateRenderer,
    .info = {
        .name = "3DS",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = { [0] = SDL_PIXELFORMAT_BGR565,
	                     [1] = SDL_PIXELFORMAT_ABGR1555,
	                     [2] = SDL_PIXELFORMAT_ABGR4444,
	                     [3] = SDL_PIXELFORMAT_ABGR8888,
        },
        .max_texture_width = 512,
        .max_texture_height = 512,
     }
};

#define N3DS_SCREEN_WIDTH    400
#define N3DS_SCREEN_HEIGHT   240

#define N3DS_FRAME_BUFFER_WIDTH  512
#define N3DS_FRAME_BUFFER_SIZE   (N3DS_FRAME_BUFFER_WIDTH*N3DS_SCREEN_HEIGHT)

#define N3DS_GPU_FIFO_SIZE       0x80000
#define N3DS_TEMPPOOL_SIZE       0x80000
//static unsigned int __attribute__((aligned(16))) DisplayList[262144];

#define RGBA8(r, g, b, a) ((((r)&0xFF)<<24) | (((g)&0xFF)<<16) | (((b)&0xFF)<<8) | (((a)&0xFF)<<0))
#define COL5650(r,g,b,a)    ((r>>3) | ((g>>2)<<5) | ((b>>3)<<11))
#define COL5551(r,g,b,a)    ((r>>3) | ((g>>3)<<5) | ((b>>3)<<10) | (a>0?0x7000:0))
#define COL4444(r,g,b,a)    ((r>>4) | ((g>>4)<<4) | ((b>>4)<<8) | ((a>>4)<<12))
#define COL8888(r,g,b,a)    ((r) | ((g)<<8) | ((b)<<16) | ((a)<<24))



typedef struct
{
	// GPU commando fifo
	u32 *gpu_cmd;
	// GPU framebuffer address
	u32 *gpu_fb_addr;
	// GPU depth buffer address
	u32 *gpu_depth_fb_addr;
	// Temporary memory pool
	void *pool_addr;
	u32 pool_index;
	u32 pool_size;


	void*           frontbuffer;
	void*           backbuffer;
	SDL_bool        initialized ;
	SDL_bool        displayListAvail ;
	unsigned int    psm ;
	unsigned int    bpp ;

	SDL_bool        vsync;
	unsigned int    currentColor;
	int             currentBlendMode;

} N3DS_RenderData;


typedef struct
{
    void                *data;                              /**< Image data. */
    unsigned int        size;                               /**< Size of data in bytes. */
    unsigned int        width;                              /**< Image width. */
    unsigned int        height;                             /**< Image height. */
    unsigned int        textureWidth;                       /**< Texture width (power of two). */
    unsigned int        textureHeight;                      /**< Texture height (power of two). */
    unsigned int        bits;                               /**< Image bits per pixel. */
    unsigned int        format;                             /**< Image format - one of ::pgePixelFormat. */
    unsigned int        pitch;
    SDL_bool            swizzled;                           /**< Is image swizzled. */

} N3DS_TextureData;

typedef struct
{
    float   x, y, z;
} VertV;


typedef struct
{
    float   u, v;
    float   x, y, z;

} VertTV;

//stolen from staplebutt
static void GPU_SetDummyTexEnv(u8 num)
{
	GPU_SetTexEnv(num,
		GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
		GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
		GPU_TEVOPERANDS(0,0,0),
		GPU_TEVOPERANDS(0,0,0),
		GPU_REPLACE,
		GPU_REPLACE,
		0xFFFFFFFF);
}

void *N3DS_pool_malloc(N3DS_RenderData *data, u32 size)
{
	if ((data->pool_index + size) < data->pool_size) {
		void *addr = (void *)((u32)data->pool_addr + data->pool_index);
		data->pool_index += size;
		return addr;
	}
	return NULL;
}

void *N3DS_pool_memalign(N3DS_RenderData *data, u32 size, u32 alignment)
{
	u32 new_index = (data->pool_index + alignment - 1) & ~(alignment - 1);
	if ((new_index + size) < data->pool_size) {
		void *addr = (void *)((u32)data->pool_addr + new_index);
		data->pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int N3DS_pool_space_free(N3DS_RenderData *data)
{
	return data->pool_size - data->pool_index;
}

void N3DS_pool_reset(N3DS_RenderData *data)
{
	data->pool_index = 0;
}

/* Return next power of 2 */
static int
TextureNextPow2(unsigned int w)
{
    if(w == 0)
        return 0;

    unsigned int n = 2;

    while(w > n)
        n <<= 1;

    return n;
}


static int
GetScaleQuality(void)
{
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);

    if (!hint || *hint == '0' || SDL_strcasecmp(hint, "nearest") == 0) {
        return GPU_NEAREST; /* GU_NEAREST good for tile-map */
    } else {
        return GPU_LINEAR; /* GU_LINEAR good for scaling */
    }
}

static int
PixelFormatTo3DSFMT(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_BGR565:
        return GPU_RGB565;
    case SDL_PIXELFORMAT_ABGR1555:
        return GPU_RGBA5551;
    case SDL_PIXELFORMAT_ABGR4444:
        return GPU_RGBA4;
    case SDL_PIXELFORMAT_ABGR8888:
        return GPU_RGBA8;
    default:
        return GPU_RGBA8;
    }
}

void
StartDrawing(SDL_Renderer * renderer)
{
	N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
	if(data->displayListAvail)
		return;

	N3DS_pool_reset(data);
	GPUCMD_SetBufferOffset(0);
	//sceGuStart(GU_DIRECT, DisplayList);

	data->displayListAvail = SDL_TRUE;
}


int
TextureSwizzle(N3DS_TextureData *n3ds_texture)
{
    if(n3ds_texture->swizzled)
        return 1;

    int bytewidth = n3ds_texture->textureWidth*(n3ds_texture->bits>>3);
    int height = n3ds_texture->size / bytewidth;

    int rowblocks = (bytewidth>>4);
    int rowblocksadd = (rowblocks-1)<<7;
    unsigned int blockaddress = 0;
    unsigned int *src = (unsigned int*) n3ds_texture->data;

    unsigned char *data = NULL;
    data = malloc(n3ds_texture->size);

    int j;

    for(j = 0; j < height; j++, blockaddress += 16)
    {
        unsigned int *block;

        block = (unsigned int*)&data[blockaddress];

        int i;

        for(i = 0; i < rowblocks; i++)
        {
            *block++ = *src++;
            *block++ = *src++;
            *block++ = *src++;
            *block++ = *src++;
            block += 28;
        }

        if((j & 0x7) == 0x7)
            blockaddress += rowblocksadd;
    }

    free(n3ds_texture->data);
    n3ds_texture->data = data;
    n3ds_texture->swizzled = SDL_TRUE;

    return 1;
}
int TextureUnswizzle(N3DS_TextureData *n3ds_texture)
{
    if(!n3ds_texture->swizzled)
        return 1;

    int blockx, blocky;

    int bytewidth = n3ds_texture->textureWidth*(n3ds_texture->bits>>3);
    int height = n3ds_texture->size / bytewidth;

    int widthblocks = bytewidth/16;
    int heightblocks = height/8;

    int dstpitch = (bytewidth - 16)/4;
    int dstrow = bytewidth * 8;

    unsigned int *src = (unsigned int*) n3ds_texture->data;

    unsigned char *data = NULL;

    data = malloc(n3ds_texture->size);

    if(!data)
        return 0;

    //sceKernelDcacheWritebackAll();

    int j;

    unsigned char *ydst = (unsigned char *)data;

    for(blocky = 0; blocky < heightblocks; ++blocky)
    {
        unsigned char *xdst = ydst;

        for(blockx = 0; blockx < widthblocks; ++blockx)
        {
            unsigned int *block;

            block = (unsigned int*)xdst;

            for(j = 0; j < 8; ++j)
            {
                *(block++) = *(src++);
                *(block++) = *(src++);
                *(block++) = *(src++);
                *(block++) = *(src++);
                block += dstpitch;
            }

            xdst += 16;
        }

        ydst += dstrow;
    }

    free(n3ds_texture->data);

    n3ds_texture->data = data;

    n3ds_texture->swizzled = SDL_FALSE;

    return 1;
}


SDL_Renderer *
N3DS_CreateRenderer(SDL_Window * window, Uint32 flags)
{
	SDL_Renderer *renderer;
	N3DS_RenderData *data;
	int pixelformat;

	renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
	if (!renderer) {
		SDL_OutOfMemory();
		return NULL;
	}

	data = (N3DS_RenderData *) SDL_calloc(1, sizeof(*data));
	if (!data) {
		N3DS_DestroyRenderer(renderer);
		SDL_OutOfMemory();
		return NULL;
	}

	renderer->WindowEvent = N3DS_WindowEvent;
	renderer->CreateTexture = N3DS_CreateTexture;
	renderer->UpdateTexture = N3DS_UpdateTexture;
	renderer->LockTexture = N3DS_LockTexture;
	renderer->UnlockTexture = N3DS_UnlockTexture;
	renderer->SetRenderTarget = N3DS_SetRenderTarget;
	renderer->UpdateViewport = N3DS_UpdateViewport;
	renderer->RenderClear = N3DS_RenderClear;
	renderer->RenderDrawPoints = N3DS_RenderDrawPoints;
	renderer->RenderDrawLines = N3DS_RenderDrawLines;
	renderer->RenderFillRects = N3DS_RenderFillRects;
	renderer->RenderCopy = N3DS_RenderCopy;
	renderer->RenderReadPixels = N3DS_RenderReadPixels;
	renderer->RenderCopyEx = N3DS_RenderCopyEx;
	renderer->RenderPresent = N3DS_RenderPresent;
	renderer->DestroyTexture = N3DS_DestroyTexture;
	renderer->DestroyRenderer = N3DS_DestroyRenderer;
	renderer->info = N3DS_RenderDriver.info;
	renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	renderer->driverdata = data;
	renderer->window = window;

	if (data->initialized != SDL_FALSE)
		return 0;
	data->initialized = SDL_TRUE;

	if (flags & SDL_RENDERER_PRESENTVSYNC) {
		data->vsync = SDL_TRUE;
	} else {
		data->vsync = SDL_FALSE;
	}

	pixelformat = PixelFormatTo3DSFMT(SDL_GetWindowPixelFormat(window));
	switch (pixelformat) {
	case GPU_RGBA4:
	case GPU_RGB565:
	case GPU_RGBA5551:
		//data->frontbuffer = (unsigned int *)(N3DS_FRAME_BUFFER_SIZE<<1);
		//data->backbuffer =  (unsigned int *)(0);
		data->bpp = 2;
		data->psm = pixelformat;
		break;
	default:
		//data->frontbuffer = (unsigned int *)(N3DS_FRAME_BUFFER_SIZE<<2);
		//data->backbuffer =  (unsigned int *)(0);
		data->bpp = 4;
		data->psm = GPU_RGBA8;
		break;
	}

	data->gpu_fb_addr       = vramMemAlign(400*240*4*2, 0x100);
	data->gpu_depth_fb_addr = vramMemAlign(400*240*4*2, 0x100);
	data->gpu_cmd           = linearAlloc(N3DS_GPU_FIFO_SIZE);
	data->pool_addr         = linearAlloc(N3DS_TEMPPOOL_SIZE);
	data->pool_size         = N3DS_TEMPPOOL_SIZE;

	gfxInitDefault();
	GPU_Init(NULL);
	gfxSet3D(false);
	GPU_Reset(NULL, data->gpu_cmd, N3DS_GPU_FIFO_SIZE);

	//Setup the shader
	//data->dvlb = DVLB_ParseFile((u32 *)shader_vsh_shbin, shader_vsh_shbin_size);
	//shaderProgramInit(&data->shader);
	//shaderProgramSetVsh(&data->shader, &dvlb->DVLE[0]);

	//Get shader uniform descriptors
	//data->projection_desc = shaderInstanceGetUniformLocation(data->shader.vertexShader, "projection");

	//shaderProgramUse(&data->shader);

	//matrix_init_orthographic(data->ortho_matrix_top, 0.0f, 400.0f, 0.0f, 240.0f, 0.0f, 1.0f);
	//matrix_init_orthographic(data->ortho_matrix_bot, 0.0f, 320.0f, 0.0f, 240.0f, 0.0f, 1.0f);
	//matrix_gpu_set_uniform(data->ortho_matrix_top, data->projection_desc);

	GPU_SetViewport((u32 *)osConvertVirtToPhys((u32)data->gpu_depth_fb_addr),
		(u32 *)osConvertVirtToPhys((u32)data->gpu_fb_addr),
		0, 0, 240, 400);

	GPU_DepthMap(-1.0f, 0.0f);
	GPU_SetFaceCulling(GPU_CULL_NONE);
	GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
	GPU_SetStencilOp(GPU_KEEP, GPU_KEEP, GPU_KEEP);
	GPU_SetBlendingColor(0,0,0,0);
	GPU_SetDepthTestAndWriteMask(true, GPU_GEQUAL, GPU_WRITE_ALL);
	GPUCMD_AddMaskedWrite(GPUREG_0062, 0x1, 0);
	GPUCMD_AddWrite(GPUREG_0118, 0);

	GPU_SetAlphaBlending(
		GPU_BLEND_ADD,
		GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
		GPU_ONE, GPU_ZERO
	);

	GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

	GPU_SetDummyTexEnv(1);
	GPU_SetDummyTexEnv(2);
	GPU_SetDummyTexEnv(3);
	GPU_SetDummyTexEnv(4);
	GPU_SetDummyTexEnv(5);

	GPUCMD_Finalize();
	GPUCMD_FlushAndRun(NULL);
	gspWaitForP3D();

	N3DS_pool_reset(data);

	consoleInit(GFX_BOTTOM, NULL); /* !!!! */
	printf("SDL2\n");

	return renderer;
}

static void
N3DS_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{

}


static int
N3DS_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
/*      N3DS_RenderData *renderdata = (N3DS_RenderData *) renderer->driverdata; */
    N3DS_TextureData* n3ds_texture = (N3DS_TextureData*) SDL_calloc(1, sizeof(*n3ds_texture));

    if(!n3ds_texture)
        return -1;

    n3ds_texture->swizzled = SDL_FALSE;
    n3ds_texture->width = texture->w;
    n3ds_texture->height = texture->h;
    n3ds_texture->textureHeight = TextureNextPow2(texture->h);
    n3ds_texture->textureWidth = TextureNextPow2(texture->w);
    n3ds_texture->format = PixelFormatTo3DSFMT(texture->format);

    switch(n3ds_texture->format)
    {
        case GPU_RGB565:
        case GPU_RGBA5551:
        case GPU_RGBA4:
            n3ds_texture->bits = 16;
            break;

        case GPU_RGBA8:
            n3ds_texture->bits = 32;
            break;

        default:
            return -1;
    }

    n3ds_texture->pitch = n3ds_texture->textureWidth * SDL_BYTESPERPIXEL(texture->format);
    n3ds_texture->size = n3ds_texture->textureHeight*n3ds_texture->pitch;
    n3ds_texture->data = SDL_calloc(1, n3ds_texture->size);

    if(!n3ds_texture->data)
    {
        SDL_free(n3ds_texture);
        return SDL_OutOfMemory();
    }
    texture->driverdata = n3ds_texture;

    return 0;
}


void
TextureActivate(SDL_Texture * texture)
{
    N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;
    int scaleMode = GetScaleQuality();

    /* Swizzling is useless with small textures. */
    if (texture->w >= 16 || texture->h >= 16)
    {
        TextureSwizzle(n3ds_texture);
    }

    //sceGuEnable(GU_TEXTURE_2D);
    //sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    //sceGuTexMode(n3ds_texture->format, 0, 0, n3ds_texture->swizzled);
    //sceGuTexFilter(scaleMode, scaleMode); /* GU_NEAREST good for tile-map */
                                          /* GU_LINEAR good for scaling */
    //sceGuTexImage(0, n3ds_texture->textureWidth, n3ds_texture->textureHeight, n3ds_texture->textureWidth, n3ds_texture->data);
    //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
}


static int
N3DS_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                   const SDL_Rect * rect, const void *pixels, int pitch)
{
/*  N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata; */
    const Uint8 *src;
    Uint8 *dst;
    int row, length,dpitch;
    src = pixels;

    N3DS_LockTexture(renderer, texture,rect,(void **)&dst, &dpitch);
    length = rect->w * SDL_BYTESPERPIXEL(texture->format);
    if (length == pitch && length == dpitch) {
        SDL_memcpy(dst, src, length*rect->h);
    } else {
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += dpitch;
        }
    }

    //sceKernelDcacheWritebackAll();
    return 0;
}

static int
N3DS_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, void **pixels, int *pitch)
{
    N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) n3ds_texture->data + rect->y * n3ds_texture->pitch +
                  rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = n3ds_texture->pitch;
    return 0;
}

static void
N3DS_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;
    SDL_Rect rect;

    /* We do whole texture updates, at least for now */
    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    N3DS_UpdateTexture(renderer, texture, &rect, n3ds_texture->data, n3ds_texture->pitch);
}

static int
N3DS_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{

    return 0;
}

static int
N3DS_UpdateViewport(SDL_Renderer * renderer)
{

    return 0;
}


static void
N3DS_SetBlendMode(SDL_Renderer * renderer, int blendMode)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    if (blendMode != data-> currentBlendMode) {
        switch (blendMode) {
        case SDL_BLENDMODE_NONE:
                //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
                //sceGuDisable(GU_BLEND);
            break;
        case SDL_BLENDMODE_BLEND:
                //sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                //sceGuEnable(GU_BLEND);
                //sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
            break;
        case SDL_BLENDMODE_ADD:
                //sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                //sceGuEnable(GU_BLEND);
                //sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00FFFFFF );
            break;
        case SDL_BLENDMODE_MOD:
                //sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                //sceGuEnable(GU_BLEND);
                //sceGuBlendFunc( GU_ADD, GU_FIX, GU_SRC_COLOR, 0, 0);
            break;
        }
        data->currentBlendMode = blendMode;
    }
}



static int
N3DS_RenderClear(SDL_Renderer *renderer)
{
	N3DS_RenderData *data = (N3DS_RenderData *)renderer->driverdata;
	/* start list */
	StartDrawing(renderer);

	//Clear the screen
	u32 color = RGBA8(renderer->r, renderer->g, renderer->b, renderer->a);

	GX_SetMemoryFill(NULL, data->gpu_fb_addr, color, &data->gpu_fb_addr[0x2EE00],
		0x201, data->gpu_depth_fb_addr, 0x00000000, &data->gpu_depth_fb_addr[0x2EE00], 0x201);
	gspWaitForPSC0();

    return 0;
}

static int
N3DS_RenderDrawPoints(SDL_Renderer * renderer, const SDL_FPoint * points,
                      int count)
{
    N3DS_RenderData *data = (N3DS_RenderData *)renderer->driverdata;
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;
    StartDrawing(renderer);
    VertV* vertices = (VertV*)N3DS_pool_malloc(data, count*sizeof(VertV));

    for (i = 0; i < count; ++i) {
            vertices[i].x = points[i].x;
            vertices[i].y = points[i].y;
            vertices[i].z = 0.0f;
    }
    //sceGuDisable(GU_TEXTURE_2D);
    //sceGuColor(color);
    //sceGuShadeModel(GU_FLAT);
    //sceGuDrawArray(GU_POINTS, GU_VERTEX_32BITF|GU_TRANSFORM_2D, count, 0, vertices);
    //sceGuShadeModel(GU_SMOOTH);
    //sceGuEnable(GU_TEXTURE_2D);

    return 0;
}

static int
N3DS_RenderDrawLines(SDL_Renderer * renderer, const SDL_FPoint * points,
                     int count)
{
    N3DS_RenderData *data = (N3DS_RenderData *)renderer->driverdata;
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;
    StartDrawing(renderer);
    VertV* vertices = (VertV*)N3DS_pool_malloc(data, count*sizeof(VertV));

    for (i = 0; i < count; ++i) {
            vertices[i].x = points[i].x;
            vertices[i].y = points[i].y;
            vertices[i].z = 0.0f;
    }

    //sceGuDisable(GU_TEXTURE_2D);
    //sceGuColor(color);
    //sceGuShadeModel(GU_FLAT);
    //sceGuDrawArray(GU_LINE_STRIP, GU_VERTEX_32BITF|GU_TRANSFORM_2D, count, 0, vertices);
    //sceGuShadeModel(GU_SMOOTH);
    //sceGuEnable(GU_TEXTURE_2D);

    return 0;
}

static int
N3DS_RenderFillRects(SDL_Renderer * renderer, const SDL_FRect * rects,
                     int count)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;
    StartDrawing(renderer);

    for (i = 0; i < count; ++i) {
        const SDL_FRect *rect = &rects[i];
        VertV* vertices = (VertV*)N3DS_pool_malloc(data, (sizeof(VertV)<<1));
        vertices[0].x = rect->x;
        vertices[0].y = rect->y;
        vertices[0].z = 0.0f;

        vertices[1].x = rect->x + rect->w;
        vertices[1].y = rect->y + rect->h;
        vertices[1].z = 0.0f;

        //sceGuDisable(GU_TEXTURE_2D);
        //sceGuColor(color);
        //sceGuShadeModel(GU_FLAT);
        //sceGuDrawArray(GU_SPRITES, GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
        //sceGuShadeModel(GU_SMOOTH);
        //sceGuEnable(GU_TEXTURE_2D);
    }

    return 0;
}


#define PI   3.14159265358979f

#define radToDeg(x) ((x)*180.f/PI)
#define degToRad(x) ((x)*PI/180.f)

float MathAbs(float x)
{
    /*float result;

    __asm__ volatile (
        "mtv      %1, S000\n"
        "vabs.s   S000, S000\n"
        "mfv      %0, S000\n"
    : "=r"(result) : "r"(x));*/

    return fabs(x);
}

void MathSincos(float r, float *s, float *c)
{
	/*
    __asm__ volatile (
        "mtv      %2, S002\n"
        "vcst.s   S003, VFPU_2_PI\n"
        "vmul.s   S002, S002, S003\n"
        "vrot.p   C000, S002, [s, c]\n"
        "mfv      %0, S000\n"
        "mfv      %1, S001\n"
    : "=r"(*s), "=r"(*c): "r"(r));*/
    *s = sinf(r);
    *c = cosf(r);
}

void Swap(float *a, float *b)
{
    float n=*a;
    *a = *b;
    *b = n;
}

static int
N3DS_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    float x, y, width, height;
    float u0, v0, u1, v1;
    unsigned char alpha;

    x = dstrect->x;
    y = dstrect->y;
    width = dstrect->w;
    height = dstrect->h;

    u0 = srcrect->x;
    v0 = srcrect->y;
    u1 = srcrect->x + srcrect->w;
    v1 = srcrect->y + srcrect->h;

    alpha = texture->a;

    StartDrawing(renderer);
    TextureActivate(texture);
    N3DS_SetBlendMode(renderer, renderer->blendMode);

    if(alpha != 255)
    {
        //sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        //sceGuColor(GU_RGBA(255, 255, 255, alpha));
    }else{
        //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        //sceGuColor(0xFFFFFFFF);
    }

    if((MathAbs(u1) - MathAbs(u0)) < 64.0f)
    {
        VertTV* vertices = (VertTV*)N3DS_pool_malloc(data, (sizeof(VertTV))<<1);

        vertices[0].u = u0;
        vertices[0].v = v0;
        vertices[0].x = x;
        vertices[0].y = y;
        vertices[0].z = 0;

        vertices[1].u = u1;
        vertices[1].v = v1;
        vertices[1].x = x + width;
        vertices[1].y = y + height;
        vertices[1].z = 0;

        //sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
    }
    else
    {
        float start, end;
        float curU = u0;
        float curX = x;
        float endX = x + width;
        float slice = 64.0f;
        float ustep = (u1 - u0)/width * slice;

        if(ustep < 0.0f)
            ustep = -ustep;

        for(start = 0, end = width; start < end; start += slice)
        {
            VertTV* vertices = (VertTV*)N3DS_pool_malloc(data, (sizeof(VertTV))<<1);

            float polyWidth = ((curX + slice) > endX) ? (endX - curX) : slice;
            float sourceWidth = ((curU + ustep) > u1) ? (u1 - curU) : ustep;

            vertices[0].u = curU;
            vertices[0].v = v0;
            vertices[0].x = curX;
            vertices[0].y = y;
            vertices[0].z = 0;

            curU += sourceWidth;
            curX += polyWidth;

            vertices[1].u = curU;
            vertices[1].v = v1;
            vertices[1].x = curX;
            vertices[1].y = (y + height);
            vertices[1].z = 0;

            //sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
        }
    }

    //if(alpha != 255) 3DS STUB
        //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    return 0;
}

static int
N3DS_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch)

{
        return 0;
}


static int
N3DS_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    float x, y, width, height;
    float u0, v0, u1, v1;
    unsigned char alpha;
    float centerx, centery;

    x = dstrect->x;
    y = dstrect->y;
    width = dstrect->w;
    height = dstrect->h;

    u0 = srcrect->x;
    v0 = srcrect->y;
    u1 = srcrect->x + srcrect->w;
    v1 = srcrect->y + srcrect->h;

    centerx = center->x;
    centery = center->y;

    alpha = texture->a;

    StartDrawing(renderer);
    TextureActivate(texture);
    N3DS_SetBlendMode(renderer, renderer->blendMode);

    if(alpha != 255)
    {
        //sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        //sceGuColor(GU_RGBA(255, 255, 255, alpha));
    }else{
        //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        //sceGuColor(0xFFFFFFFF);
    }

/*      x += width * 0.5f; */
/*      y += height * 0.5f; */
    x += centerx;
    y += centery;

    float c, s;

    MathSincos(degToRad(angle), &s, &c);

/*      width *= 0.5f; */
/*      height *= 0.5f; */
    width  -= centerx;
    height -= centery;


    float cw = c*width;
    float sw = s*width;
    float ch = c*height;
    float sh = s*height;

    VertTV* vertices = (VertTV*)N3DS_pool_malloc(data, sizeof(VertTV)<<2);

    vertices[0].u = u0;
    vertices[0].v = v0;
    vertices[0].x = x - cw + sh;
    vertices[0].y = y - sw - ch;
    vertices[0].z = 0;

    vertices[1].u = u0;
    vertices[1].v = v1;
    vertices[1].x = x - cw - sh;
    vertices[1].y = y - sw + ch;
    vertices[1].z = 0;

    vertices[2].u = u1;
    vertices[2].v = v1;
    vertices[2].x = x + cw - sh;
    vertices[2].y = y + sw + ch;
    vertices[2].z = 0;

    vertices[3].u = u1;
    vertices[3].v = v0;
    vertices[3].x = x + cw + sh;
    vertices[3].y = y + sw - ch;
    vertices[3].z = 0;

    if (flip & SDL_FLIP_HORIZONTAL) {
                Swap(&vertices[0].v, &vertices[2].v);
                Swap(&vertices[1].v, &vertices[3].v);
    }
    if (flip & SDL_FLIP_VERTICAL) {
                Swap(&vertices[0].u, &vertices[2].u);
                Swap(&vertices[1].u, &vertices[3].u);
    }

    //sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 4, 0, vertices);

    //if(alpha != 255)  3DS STUB
        //sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    return 0;
}

static void
N3DS_RenderPresent(SDL_Renderer * renderer)
{
	N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
	if (!data->displayListAvail)
		return;

	data->displayListAvail = SDL_FALSE;

	GPU_FinishDrawing();
	GPUCMD_Finalize();
	GPUCMD_FlushAndRun(NULL);
	gspWaitForP3D();

	//Copy the GPU rendered FB to the screen FB
	GX_SetDisplayTransfer(NULL, data->gpu_fb_addr, GX_BUFFER_DIM(240, 400),
		(u32 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL),
		GX_BUFFER_DIM(240, 400), 0x1000);

	gspWaitForPPF();


	/* Swap buffers */
	gfxSwapBuffersGpu();
	if (data->vsync) {
		gspWaitForEvent(GSPEVENT_VBlank0, true);
	}

	//sceGuFinish();
	//sceGuSync(0,0);
	//sceGuSwapBuffers();
}

static void
N3DS_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_RenderData *renderdata = (N3DS_RenderData *) renderer->driverdata;
    N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;

    if (renderdata == 0)
        return;

    if(n3ds_texture == 0)
        return;

    SDL_free(n3ds_texture->data);
    SDL_free(n3ds_texture);
    texture->driverdata = NULL;
}

static void
N3DS_DestroyRenderer(SDL_Renderer * renderer)
{
	N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
	if (data) {
		if (!data->initialized)
			return;

		StartDrawing(renderer);

		gfxExit();
		//shaderProgramFree(&data->shader);
		//DVLB_Free(data->dvlb);

		linearFree(data->pool_addr);
		linearFree(data->gpu_cmd);
		vramFree(data->gpu_fb_addr);
		vramFree(data->gpu_depth_fb_addr);


		//sceGuTerm();
		/*      vfree(data->backbuffer); */
		/*      vfree(data->frontbuffer); */

		data->initialized = SDL_FALSE;
		data->displayListAvail = SDL_FALSE;
		SDL_free(data);
	}
	SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_3DS */

/* vi: set ts=4 sw=4 expandtab: */

