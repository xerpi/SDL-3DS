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
#include "shader_vsh_shbin.h"

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
	//Shader stuff
	DVLB_s *dvlb;
	shaderProgram_s shader;
	u32 projection_desc;
	//Matrix
	float ortho_matrix_top[4*4];
	float ortho_matrix_bot[4*4];



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


typedef struct {
	float u;
	float v;
} vector_2f;

typedef struct {
	float x;
	float y;
	float z;
} vector_3f;

typedef struct {
	float r;
	float g;
	float b;
	float a;
} vector_4f;

typedef struct {
	vector_3f position;
	vector_4f color;
} vertex_pos_col;

typedef struct {
	vector_3f position;
	vector_2f texcoord;
} vertex_pos_tex;


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

static void vector_mult_matrix4x4(const float *msrc, const vector_3f *vsrc, vector_3f *vdst);
static void matrix_gpu_set_uniform(const float *m, u32 startreg);
static void matrix_copy(float *dst, const float *src);
static void matrix_identity4x4(float *m);
static void matrix_mult4x4(const float *src1, const float *src2, float *dst);
static void matrix_set_z_rotation(float *m, float rad);
static void matrix_rotate_z(float *m, float rad);
static void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale);
static void matrix_swap_xy(float *m);
static void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far);

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


// Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 morton_interleave(u32 x, u32 y)
{
	u32 i = (x & 7) | ((y & 7) << 8); // ---- -210
	i = (i ^ (i << 2)) & 0x1313;      // ---2 --10
	i = (i ^ (i << 1)) & 0x1515;      // ---2 -1-0
	i = (i | (i >> 7)) & 0x3F;
	return i;
}

//Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 get_morton_offset(u32 x, u32 y, u32 bytes_per_pixel)
{
    u32 i = morton_interleave(x, y);
    unsigned int offset = (x & ~7) * 8;
    return (i + offset) * bytes_per_pixel;
}

int
TextureSwizzle(N3DS_TextureData *n3ds_texture)
{
	if(n3ds_texture->swizzled)
		return 1;

	// TODO: add support for non-RGBA8 textures
	u8 *data = linearAlloc(n3ds_texture->textureWidth * n3ds_texture->textureHeight * 4);

	int i, j;
	for (j = 0; j < n3ds_texture->textureHeight; j++) {
		for (i = 0; i < n3ds_texture->textureWidth; i++) {
			u32 coarse_y = j & ~7;
			u32 dst_offset = get_morton_offset(i, j, 4) + coarse_y * n3ds_texture->textureWidth * 4;
			u32 v = ((u32 *)n3ds_texture->data)[i + (n3ds_texture->textureHeight - 1 - j)*n3ds_texture->textureWidth];
			*(u32 *)(data + dst_offset) = v;
		}
	}

	linearFree(n3ds_texture->data);
	n3ds_texture->data = data;
	n3ds_texture->swizzled = SDL_TRUE;

	return 1;
}
int TextureUnswizzle(N3DS_TextureData *n3ds_texture)
{
    if(!n3ds_texture->swizzled)
        return 1;

    //n3ds_texture->swizzled = SDL_FALSE;

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

	data->gpu_fb_addr       = vramMemAlign(400*240*4, 0x100);
	data->gpu_depth_fb_addr = vramMemAlign(400*240*2, 0x100);
	data->gpu_cmd           = linearAlloc(N3DS_GPU_FIFO_SIZE);
	data->pool_addr         = linearAlloc(N3DS_TEMPPOOL_SIZE);
	data->pool_size         = N3DS_TEMPPOOL_SIZE;

	gfxInitDefault();
	GPU_Init(NULL);
	gfxSet3D(false);
	GPU_Reset(NULL, data->gpu_cmd, N3DS_GPU_FIFO_SIZE);

	//Setup the shader
	data->dvlb = DVLB_ParseFile((u32 *)shader_vsh_shbin, shader_vsh_shbin_size);
	shaderProgramInit(&data->shader);
	shaderProgramSetVsh(&data->shader, &data->dvlb->DVLE[0]);

	//Get shader uniform descriptors
	data->projection_desc = shaderInstanceGetUniformLocation(data->shader.vertexShader, "projection");

	shaderProgramUse(&data->shader);

	matrix_init_orthographic(data->ortho_matrix_top, 0.0f, 400.0f, 0.0f, 240.0f, 0.0f, 1.0f);
	matrix_init_orthographic(data->ortho_matrix_bot, 0.0f, 320.0f, 0.0f, 240.0f, 0.0f, 1.0f);
	matrix_gpu_set_uniform(data->ortho_matrix_top, data->projection_desc);

	GPU_SetViewport((u32 *)osConvertVirtToPhys((u32)data->gpu_depth_fb_addr),
		(u32 *)osConvertVirtToPhys((u32)data->gpu_fb_addr),
		0, 0, 240, 400);

	GPU_DepthMap(-1.0f, 0.0f);
	GPU_SetFaceCulling(GPU_CULL_NONE);
	GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
	GPU_SetStencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
	GPU_SetBlendingColor(0,0,0,0);
	GPU_SetDepthTestAndWriteMask(true, GPU_GEQUAL, GPU_WRITE_ALL);
	GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);

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
	GPUCMD_FlushAndRun();
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
    n3ds_texture->data = linearAlloc(n3ds_texture->size);

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

	/* We must swizzle (Z-order) textures on the 3DS */
	TextureSwizzle(n3ds_texture);

	GPU_SetTextureEnable(GPU_TEXUNIT0);

	GPU_SetTexEnv(
		0,
		GPU_TEVSOURCES(GPU_TEXTURE0, GPU_TEXTURE0, GPU_TEXTURE0),
		GPU_TEVSOURCES(GPU_TEXTURE0, GPU_TEXTURE0, GPU_TEXTURE0),
		GPU_TEVOPERANDS(0, 0, 0),
		GPU_TEVOPERANDS(0, 0, 0),
		GPU_REPLACE, GPU_REPLACE,
		0xFFFFFFFF
	);

	GPU_SetTexture(
		GPU_TEXUNIT0,
		(u32 *)osConvertVirtToPhys((u32)n3ds_texture->data),
		n3ds_texture->textureWidth,
		n3ds_texture->textureHeight,
		GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST),
		n3ds_texture->format
	);

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
	u32 color = COL8888(renderer->r, renderer->g, renderer->b, renderer->a);

	GX_MemoryFill(data->gpu_fb_addr, color, &data->gpu_fb_addr[0x2EE00],
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
    /*StartDrawing(renderer);
    vector_3f* vertices = (vector_3f*)N3DS_pool_malloc(data, count*sizeof(vector_3f));

    for (i = 0; i < count; ++i) {
            vertices[i].position.x = points[i].position.x;
            vertices[i].position.y = points[i].position.y;
            vertices[i].position.z = 0.0f;
    }*/
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
    /*StartDrawing(renderer);
    vector_3f* vertices = (vector_3f*)N3DS_pool_malloc(data, count*sizeof(vector_3f));

    for (i = 0; i < count; ++i) {
            vertices[i].position.x = points[i].position.x;
            vertices[i].position.y = points[i].position.y;
            vertices[i].position.z = 0.0f;
    }*/

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
	int i;
	//StartDrawing(renderer);

	for (i = 0; i < count; ++i) {
		const SDL_FRect *rect = &rects[i];
		vertex_pos_col *vertices = (vertex_pos_col*)N3DS_pool_malloc(data, (sizeof(vertex_pos_col)*4));

		vertices[0].position = (vector_3f){(float)rect->x,         (float)rect->y,         0.0f};
		vertices[1].position = (vector_3f){(float)rect->x+rect->w, (float)rect->y,         0.0f};
		vertices[2].position = (vector_3f){(float)rect->x,         (float)rect->y+rect->h, 0.0f};
		vertices[3].position = (vector_3f){(float)rect->x+rect->w, (float)rect->y+rect->h, 0.0f};

		vertices[0].color = (vector_4f){renderer->r/255.0f,  renderer->g/255.0f, renderer->b/255.0f, renderer->a/255.0f};
		vertices[1].color = vertices[0].color;
		vertices[2].color = vertices[0].color;
		vertices[3].color = vertices[0].color;

		GPU_SetTexEnv(
			0,
			GPU_TEVSOURCES(GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR),
			GPU_TEVSOURCES(GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR),
			GPU_TEVOPERANDS(0, 0, 0),
			GPU_TEVOPERANDS(0, 0, 0),
			GPU_REPLACE, GPU_REPLACE,
			0xFFFFFFFF
		);

		GPU_SetAttributeBuffers(
			2, // number of attributes
			(u32*)osConvertVirtToPhys((u32)vertices),
			GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 4, GPU_FLOAT),
			0xFFFC, //0b1100
			0x10,
			1, //number of buffers
			(u32[]){0x0}, // buffer offsets (placeholders)
			(u64[]){0x10}, // attribute permutations for each buffer
			(u8[]){2} // number of attributes for each buffer
		);

		GPU_DrawArray(GPU_TRIANGLE_STRIP, 0, 4);
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
	N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;
	float x, y, width, height;
	float u0, v0, u1, v1;
	unsigned char alpha;

	x = dstrect->x;
	y = dstrect->y;
	width = dstrect->w;
	height = dstrect->h;

	u0 = (srcrect->x)/n3ds_texture->textureWidth;
	v0 = (srcrect->y)/n3ds_texture->textureHeight;
	u1 = (srcrect->x + srcrect->w)/n3ds_texture->textureWidth;
	v1 = (srcrect->y + srcrect->h)/n3ds_texture->textureHeight;

	alpha = texture->a;

	//StartDrawing(renderer);
	TextureActivate(texture);
	N3DS_SetBlendMode(renderer, renderer->blendMode);

	if (alpha != 255) {
		//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		//sceGuColor(GU_RGBA(255, 255, 255, alpha));
	} else {
		//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		//sceGuColor(0xFFFFFFFF);
	}

	vertex_pos_tex* vertices = (vertex_pos_tex*)N3DS_pool_malloc(data, (sizeof(vertex_pos_tex))*4);

	vertices[0].position = (vector_3f){(float)x,       (float)y,        0.0f};
	vertices[1].position = (vector_3f){(float)x+width, (float)y,        0.0f};
	vertices[2].position = (vector_3f){(float)x,       (float)y+height, 0.0f};
	vertices[3].position = (vector_3f){(float)x+width, (float)y+height, 0.0f};

	//float u = texture->width/(float)texture->textureWidth;
	//float v = texture->height/(float)texture->textureHeight;

	vertices[0].texcoord = (vector_2f){u0, v0};
	vertices[1].texcoord = (vector_2f){u1, v0};
	vertices[2].texcoord = (vector_2f){u0, v1};
	vertices[3].texcoord = (vector_2f){u1, v1};

	GPU_SetAttributeBuffers(
		2, // number of attributes
		(u32*)osConvertVirtToPhys((u32)vertices),
		GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 4, GPU_FLOAT),
		0xFFFC, //0b1100
		0x10,
		1, //number of buffers
		(u32[]){0x0}, // buffer offsets (placeholders)
		(u64[]){0x10}, // attribute permutations for each buffer
		(u8[]){2} // number of attributes for each buffer
	);

	GPU_DrawArray(GPU_TRIANGLE_STRIP, 0, 4);

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
	N3DS_TextureData *n3ds_texture = (N3DS_TextureData *) texture->driverdata;
	float x, y, width, height;
	float u0, v0, u1, v1;
	unsigned char alpha;
	float centerx, centery;

	x = dstrect->x;
	y = dstrect->y;
	width = dstrect->w;
	height = dstrect->h;

	u0 = (srcrect->x)/n3ds_texture->textureWidth;
	v0 = (srcrect->y)/n3ds_texture->textureHeight;
	u1 = (srcrect->x + srcrect->w)/n3ds_texture->textureWidth;
	v1 = (srcrect->y + srcrect->h)/n3ds_texture->textureHeight;


	centerx = center->x;
	centery = center->y;

	alpha = texture->a;

	//StartDrawing(renderer);
	TextureActivate(texture);
	N3DS_SetBlendMode(renderer, renderer->blendMode);

	if (alpha != 255) {
		//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		//sceGuColor(GU_RGBA(255, 255, 255, alpha));
	} else {
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

	vertex_pos_tex* vertices = (vertex_pos_tex*)N3DS_pool_malloc(data, sizeof(vertex_pos_tex)<<2);

	vertices[0].texcoord.u = u0;
	vertices[0].texcoord.v = v0;
	vertices[0].position.x = x - cw + sh;
	vertices[0].position.y = y - sw - ch;
	vertices[0].position.z = 0;

	vertices[1].texcoord.u = u0;
	vertices[1].texcoord.v = v1;
	vertices[1].position.x = x - cw - sh;
	vertices[1].position.y = y - sw + ch;
	vertices[1].position.z = 0;

	vertices[2].texcoord.u = u1;
	vertices[2].texcoord.v = v1;
	vertices[2].position.x = x + cw - sh;
	vertices[2].position.y = y + sw + ch;
	vertices[2].position.z = 0;

	vertices[3].texcoord.u = u1;
	vertices[3].texcoord.v = v0;
	vertices[3].position.x = x + cw + sh;
	vertices[3].position.y = y + sw - ch;
	vertices[3].position.z = 0;

	if (flip & SDL_FLIP_HORIZONTAL) {
		Swap(&vertices[0].texcoord.v, &vertices[2].texcoord.v);
		Swap(&vertices[1].texcoord.v, &vertices[3].texcoord.v);
	}
	if (flip & SDL_FLIP_VERTICAL) {
		Swap(&vertices[0].texcoord.u, &vertices[2].texcoord.u);
		Swap(&vertices[1].texcoord.u, &vertices[3].texcoord.u);
	}

	GPU_SetAttributeBuffers(
		2, // number of attributes
		(u32*)osConvertVirtToPhys((u32)vertices),
		GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 4, GPU_FLOAT),
		0xFFFC, //0b1100
		0x10,
		1, //number of buffers
		(u32[]){0x0}, // buffer offsets (placeholders)
		(u64[]){0x10}, // attribute permutations for each buffer
		(u8[]){2} // number of attributes for each buffer
	);

	GPU_DrawArray(GPU_TRIANGLE_STRIP, 0, 4);

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
	GPUCMD_FlushAndRun();
	gspWaitForP3D();

	//Copy the GPU rendered FB to the screen FB
	GX_DisplayTransfer(data->gpu_fb_addr, GX_BUFFER_DIM(240, 400),
		(u32 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL),
		GX_BUFFER_DIM(240, 400), 0x1000);

	gspWaitForPPF();


	/* Swap buffers */
	gfxSwapBuffersGpu();
	if (data->vsync) {
		gspWaitForEvent(GSPGPU_EVENT_VBlank0, true);
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

	// Texture Data allocated in the Linear Heap
	linearFree(n3ds_texture->data);

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

		//StartDrawing(renderer);

		gfxExit();
		shaderProgramFree(&data->shader);
		DVLB_Free(data->dvlb);

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


/* Utils */

void vector_mult_matrix4x4(const float *msrc, const vector_3f *vsrc, vector_3f *vdst)
{
	vdst->x = msrc[0*4 + 0]*vsrc->x + msrc[0*4 + 1]*vsrc->y + msrc[0*4 + 2]*vsrc->z + msrc[0*4 + 3];
	vdst->y = msrc[1*4 + 0]*vsrc->x + msrc[1*4 + 1]*vsrc->y + msrc[1*4 + 2]*vsrc->z + msrc[1*4 + 3];
	vdst->z = msrc[2*4 + 0]*vsrc->x + msrc[2*4 + 1]*vsrc->y + msrc[2*4 + 2]*vsrc->z + msrc[2*4 + 3];
}

void matrix_gpu_set_uniform(const float *m, u32 startreg)
{
	float mu[4*4];

	int i, j;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			mu[i*4 + j] = m[i*4 + (3-j)];
		}
	}

	GPU_SetFloatUniform(GPU_VERTEX_SHADER, startreg, (u32 *)mu, 4);
}

void matrix_copy(float *dst, const float *src)
{
	memcpy(dst, src, sizeof(float)*4*4);
}

void matrix_identity4x4(float *m)
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	m[1] = m[2] = m[3] = 0.0f;
	m[4] = m[6] = m[7] = 0.0f;
	m[8] = m[9] = m[11] = 0.0f;
	m[12] = m[13] = m[14] = 0.0f;
}

void matrix_mult4x4(const float *src1, const float *src2, float *dst)
{
	int i, j, k;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			dst[i*4 + j] = 0.0f;
			for (k = 0; k < 4; k++) {
				dst[i*4 + j] += src1[i*4 + k]*src2[k*4 + j];
			}
		}
	}
}

void matrix_set_z_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[0] = c;
	m[1] = -s;
	m[4] = s;
	m[5] = c;
}

void matrix_rotate_z(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_z_rotation(mr, rad);
	matrix_mult4x4(mr, m, mt);
	matrix_copy(m, mt);
}

void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale)
{
	matrix_identity4x4(m);
	m[0] = x_scale;
	m[5] = y_scale;
	m[10] = z_scale;
}

void matrix_swap_xy(float *m)
{
	float ms[4*4], mt[4*4];
	matrix_identity4x4(ms);

	ms[0] = 0.0f;
	ms[1] = 1.0f;
	ms[4] = 1.0f;
	ms[5] = 0.0f;

	matrix_mult4x4(ms, m, mt);
	matrix_copy(m, mt);
}

void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far)
{
	float mo[4*4], mp[4*4];

	mo[0x0] = 2.0f/(right-left);
	mo[0x1] = 0.0f;
	mo[0x2] = 0.0f;
	mo[0x3] = -(right+left)/(right-left);

	mo[0x4] = 0.0f;
	mo[0x5] = 2.0f/(top-bottom);
	mo[0x6] = 0.0f;
	mo[0x7] = -(top+bottom)/(top-bottom);

	mo[0x8] = 0.0f;
	mo[0x9] = 0.0f;
	mo[0xA] = -2.0f/(far-near);
	mo[0xB] = (far+near)/(far-near);

	mo[0xC] = 0.0f;
	mo[0xD] = 0.0f;
	mo[0xE] = 0.0f;
	mo[0xF] = 1.0f;

	matrix_identity4x4(mp);
	mp[0xA] = 0.5;
	mp[0xB] = -0.5;

	//Convert Z [-1, 1] to [-1, 0] (PICA shiz)
	matrix_mult4x4(mp, mo, m);
	// Rotate 180 degrees
	matrix_rotate_z(m, M_PI);
	// Swap X and Y axis
	matrix_swap_xy(m);
}

#endif /* SDL_VIDEO_RENDER_3DS */

/* vi: set ts=4 sw=4 expandtab: */
