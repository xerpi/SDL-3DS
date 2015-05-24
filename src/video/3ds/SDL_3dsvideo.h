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

#ifndef _SDL_3dsvideo_h
#define _SDL_3dsvideo_h

//#include <GLES/egl.h>

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

typedef struct SDL_VideoData
{
    SDL_bool egl_initialized;   /* OpenGL ES device initialization status */
    uint32_t egl_refcount;      /* OpenGL ES reference count              */



} SDL_VideoData;


typedef struct SDL_DisplayData
{

} SDL_DisplayData;


typedef struct SDL_WindowData
{
    SDL_bool uses_gles;         /* if true window must support OpenGL ES */

} SDL_WindowData;




/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int N3DS_VideoInit(_THIS);
void N3DS_VideoQuit(_THIS);
void N3DS_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
int N3DS_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
int N3DS_CreateWindow(_THIS, SDL_Window * window);
int N3DS_CreateWindowFrom(_THIS, SDL_Window * window, const void *data);
void N3DS_SetWindowTitle(_THIS, SDL_Window * window);
void N3DS_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
void N3DS_SetWindowPosition(_THIS, SDL_Window * window);
void N3DS_SetWindowSize(_THIS, SDL_Window * window);
void N3DS_ShowWindow(_THIS, SDL_Window * window);
void N3DS_HideWindow(_THIS, SDL_Window * window);
void N3DS_RaiseWindow(_THIS, SDL_Window * window);
void N3DS_MaximizeWindow(_THIS, SDL_Window * window);
void N3DS_MinimizeWindow(_THIS, SDL_Window * window);
void N3DS_RestoreWindow(_THIS, SDL_Window * window);
void N3DS_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
void N3DS_DestroyWindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool N3DS_GetWindowWMInfo(_THIS, SDL_Window * window,
                             struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int N3DS_GL_LoadLibrary(_THIS, const char *path);
void *N3DS_GL_GetProcAddress(_THIS, const char *proc);
void N3DS_GL_UnloadLibrary(_THIS);
SDL_GLContext N3DS_GL_CreateContext(_THIS, SDL_Window * window);
int N3DS_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
int N3DS_GL_SetSwapInterval(_THIS, int interval);
int N3DS_GL_GetSwapInterval(_THIS);
void N3DS_GL_SwapWindow(_THIS, SDL_Window * window);
void N3DS_GL_DeleteContext(_THIS, SDL_GLContext context);

/* N3DS on screen keyboard */
SDL_bool N3DS_HasScreenKeyboardSupport(_THIS);
void N3DS_ShowScreenKeyboard(_THIS, SDL_Window *window);
void N3DS_HideScreenKeyboard(_THIS, SDL_Window *window);
SDL_bool N3DS_IsScreenKeyboardShown(_THIS, SDL_Window *window);

#endif /* _SDL_3dsvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
