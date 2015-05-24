/*
    SDL_psp_main.c, placed in the public domain by Sam Lantinga  3/13/14
*/
#include "SDL_config.h"

#ifdef __3DS__

#include "SDL_main.h"
/*#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspthreadman.h>
#include <stdlib.h>
#include <stdio.h>*/

/* If application's main() is redefined as SDL_main, and libSDLmain is
   linked, then this file will create the standard exit callback,
   define the PSP_MODULE_INFO macro, and exit back to the browser when
   the program is finished.

   You can still override other parameters in your own code if you
   desire, such as PSP_HEAP_SIZE_KB, PSP_MAIN_THREAD_ATTR,
   PSP_MAIN_THREAD_STACK_SIZE, etc.
*/

int main(int argc, char *argv[])
{
    //pspDebugScreenInit();

    /* Register sceKernelExitGame() to be called when we exit */
    atexit(sceKernelExitGame);

    SDL_SetMainReady();

    (void)SDL_main(argc, argv);
    return 0;
}

#endif /* __3DS__ */

/* vi: set ts=4 sw=4 expandtab: */
