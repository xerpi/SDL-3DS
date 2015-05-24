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

#if SDL_VIDEO_DRIVER_3DS

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"



/* 3DS declarations */
#include "SDL_3dsvideo.h"
#include "SDL_3dsevents_c.h"
//#include "SDL_pspgl_c.h"

/* unused
static SDL_bool N3DS_initialized = SDL_FALSE;
*/
static int
N3DS_Available(void)
{
    return 1;
}

static void
N3DS_Destroy(SDL_VideoDevice * device)
{
/*    SDL_VideoData *phdata = (SDL_VideoData *) device->driverdata; */

    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
N3DS_Create()
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
    //SDL_GLDriverData *gldata;
    int status;

    /* Check if 3DS could be initialized */
    status = N3DS_Available();
    if (status == 0) {
        /* 3DS could not be used */
        return NULL;
    }

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal 3DS specific data */
    phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    /*gldata = (SDL_GLDriverData *) SDL_calloc(1, sizeof(SDL_GLDriverData)); 3DS STUB
    if (gldata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }*/
    device->gl_data = NULL;//gldata; 3DS STUB

    device->driverdata = phdata;

    phdata->egl_initialized = SDL_TRUE;


    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = N3DS_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = N3DS_VideoInit;
    device->VideoQuit = N3DS_VideoQuit;
    device->GetDisplayModes = N3DS_GetDisplayModes;
    device->SetDisplayMode = N3DS_SetDisplayMode;
    device->CreateWindow = N3DS_CreateWindow;
    device->CreateWindowFrom = N3DS_CreateWindowFrom;
    device->SetWindowTitle = N3DS_SetWindowTitle;
    device->SetWindowIcon = N3DS_SetWindowIcon;
    device->SetWindowPosition = N3DS_SetWindowPosition;
    device->SetWindowSize = N3DS_SetWindowSize;
    device->ShowWindow = N3DS_ShowWindow;
    device->HideWindow = N3DS_HideWindow;
    device->RaiseWindow = N3DS_RaiseWindow;
    device->MaximizeWindow = N3DS_MaximizeWindow;
    device->MinimizeWindow = N3DS_MinimizeWindow;
    device->RestoreWindow = N3DS_RestoreWindow;
    device->SetWindowGrab = N3DS_SetWindowGrab;
    device->DestroyWindow = N3DS_DestroyWindow;
    device->GetWindowWMInfo = N3DS_GetWindowWMInfo;
    /*device->GL_LoadLibrary = N3DS_GL_LoadLibrary; 3DS STUB
    device->GL_GetProcAddress = N3DS_GL_GetProcAddress;
    device->GL_UnloadLibrary = N3DS_GL_UnloadLibrary;
    device->GL_CreateContext = N3DS_GL_CreateContext;
    device->GL_MakeCurrent = N3DS_GL_MakeCurrent;
    device->GL_SetSwapInterval = N3DS_GL_SetSwapInterval;
    device->GL_GetSwapInterval = N3DS_GL_GetSwapInterval;
    device->GL_SwapWindow = N3DS_GL_SwapWindow;
    device->GL_DeleteContext = N3DS_GL_DeleteContext;*/
    device->HasScreenKeyboardSupport = N3DS_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = N3DS_ShowScreenKeyboard;
    device->HideScreenKeyboard = N3DS_HideScreenKeyboard;
    device->IsScreenKeyboardShown = N3DS_IsScreenKeyboardShown;

    device->PumpEvents = N3DS_PumpEvents;

    return device;
}

VideoBootStrap N3DS_bootstrap = {
    "3DS",
    "3DS Video Driver",
    N3DS_Available,
    N3DS_Create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
N3DS_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    SDL_zero(current_mode);

    current_mode.w = 400;
    current_mode.h = 240;

    current_mode.refresh_rate = 60;
    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;

    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;

    SDL_AddVideoDisplay(&display);

    return 1;
}

void
N3DS_VideoQuit(_THIS)
{

}

void
N3DS_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{

}

int
N3DS_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}
#define EGLCHK(stmt)                            \
    do {                                        \
        EGLint err;                             \
                                                \
        stmt;                                   \
        err = eglGetError();                    \
        if (err != EGL_SUCCESS) {               \
            SDL_SetError("EGL error %d", err);  \
            return 0;                           \
        }                                       \
    } while (0)

int
N3DS_CreateWindow(_THIS, SDL_Window * window)
{
	SDL_WindowData *wdata;

	/* Allocate window internal data */
	wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
		if (wdata == NULL) {
		return SDL_OutOfMemory();
	}

	/* Setup driver data for this window */
	window->driverdata = wdata;

	/* Set the keyboard focus */
	SDL_SetKeyboardFocus(window);

	/* Window has been successfully created */
	return 0;
}

int
N3DS_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
N3DS_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
N3DS_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
N3DS_SetWindowPosition(_THIS, SDL_Window * window)
{
}
void
N3DS_SetWindowSize(_THIS, SDL_Window * window)
{
}
void
N3DS_ShowWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_HideWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
N3DS_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}
void
N3DS_DestroyWindow(_THIS, SDL_Window * window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
N3DS_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}


/* TO Write Me */
SDL_bool N3DS_HasScreenKeyboardSupport(_THIS)
{
    return SDL_FALSE;
}
void N3DS_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
}
void N3DS_HideScreenKeyboard(_THIS, SDL_Window *window)
{
}
SDL_bool N3DS_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    return SDL_FALSE;
}


#endif /* SDL_VIDEO_DRIVER_3DS */

/* vi: set ts=4 sw=4 expandtab: */
