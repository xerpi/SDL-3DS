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

#ifndef SDL_POWER_DISABLED
#if SDL_POWER_3DS

#include "SDL_power.h"
#include <3ds.h>


SDL_bool
SDL_GetPowerInfo_3DS(SDL_PowerState * state, int *seconds,
                            int *percent)
{
	u8 batteryLevel = 0;
	u8 charging = 0;

	PTMU_GetBatteryLevel(&batteryLevel);
	PTMU_GetBatteryChargeState(&charging);

	*state = SDL_POWERSTATE_UNKNOWN;
	*seconds = -1;
	*percent = -1;

	if (charging) {
		*state = SDL_POWERSTATE_CHARGING;
		//*percent = scePowerGetBatteryLifePercent();
		//*seconds = scePowerGetBatteryLifeTime()*60;
	} else if (batteryLevel == 5) {
		*state = SDL_POWERSTATE_CHARGED;
		//*percent = scePowerGetBatteryLifePercent();
		//*seconds = scePowerGetBatteryLifeTime()*60;
	} else {
		*state = SDL_POWERSTATE_ON_BATTERY;
		//*percent = scePowerGetBatteryLifePercent();
		//*seconds = scePowerGetBatteryLifeTime()*60;
	}

    return SDL_TRUE;	/* always the definitive answer on 3DS. */
}

#endif /* SDL_POWER_3DS */
#endif /* SDL_POWER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
