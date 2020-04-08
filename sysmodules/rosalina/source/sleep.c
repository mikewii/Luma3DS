#include <3ds.h>

static bool         g_isSleeping = false;
static LightEvent   g_onWakeUpEvent;

void    Sleep__Init(void)
{
	srvSubscribe(0x214); ///< Sleep entry
	srvSubscribe(0x213); ///< Sleep exit
	LightEvent_Init(&g_onWakeUpEvent, RESET_STICKY);
}

void    Sleep__HandleNotification(u32 notifId)
{
	if (notifId == 0x214) ///< Sleep entry
		{
			LightEvent_Clear(&g_onWakeUpEvent);
			g_isSleeping = true;
		}
	else if (notifId == 0x213) ///< Sleep exit
		{
			g_isSleeping = false;
			LightEvent_Signal(&g_onWakeUpEvent);
		}
}

bool    Sleep__Status(void)
{
	if (g_isSleeping)
	{
		LightEvent_Wait(&g_onWakeUpEvent);
		return true;
	}
	return false;
}
