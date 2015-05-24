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

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_3dsvideo.h"
#include "SDL_3dsevents_c.h"
#include "SDL_thread.h"
#include "SDL_keyboard.h"
//#include <psphprm.h>


//static enum PspHprmKeys hprm = 0;
static SDL_sem *event_sem = NULL;
static SDL_Thread *thread = NULL;
static int running = 0;

int EventUpdate(void *data)
{
    while (running) {
                SDL_SemWait(event_sem);
		//sceHprmPeekCurrentKey(&hprm);
                SDL_SemPost(event_sem);
                /* Delay 1/60th of a second */
                //sceKernelDelayThread(1000000 / 60); 3DS STUB
        }
        return 0;
}


void N3DS_PumpEvents(_THIS)
{
#if 0
    int i;
    enum PspHprmKeys keys;
    enum PspHprmKeys changed;
    static enum PspHprmKeys old_keys = 0;
    SDL_Keysym sym;

    SDL_SemWait(event_sem);
    keys = hprm;
    SDL_SemPost(event_sem);

    /* HPRM Keyboard */
    changed = old_keys ^ keys;
    old_keys = keys;
    if(changed) {
        for(i=0; i<sizeof(keymap_psp)/sizeof(keymap_psp[0]); i++) {
            if(changed & keymap_psp[i].id) {
                sym.scancode = keymap_psp[i].id;
                sym.sym = keymap_psp[i].sym;

                /* out of date
                SDL_PrivateKeyboard((keys & keymap_psp[i].id) ?
                            SDL_PRESSED : SDL_RELEASED,
                            &sym);
        */
                SDL_SendKeyboardKey((keys & keymap_psp[i].id) ?
                                    SDL_PRESSED : SDL_RELEASED, SDL_GetScancodeFromKey(keymap_psp[i].sym));
            }
        }
    }
#endif
    return;
}


void N3DS_EventInit(_THIS)
{
    /* Start thread to read data */
    if((event_sem =  SDL_CreateSemaphore(1)) == NULL) {
        SDL_SetError("Can't create input semaphore\n");
        return;
    }
    running = 1;
    if((thread = SDL_CreateThread(EventUpdate, "3DSInputThread",NULL)) == NULL) {
        SDL_SetError("Can't create input thread\n");
        return;
    }
}

void N3DS_EventQuit(_THIS)
{
    running = 0;
    SDL_WaitThread(thread, NULL);
    SDL_DestroySemaphore(event_sem);
}

/* end of SDL_3dsevents.c ... */

#endif /* SDL_VIDEO_DRIVER_3DS */

/* vi: set ts=4 sw=4 expandtab: */
