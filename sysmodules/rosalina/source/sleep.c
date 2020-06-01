#include <3ds.h>
#include "plugin/plgldr.h"

static bool         g_isSleeping = false;
static LightEvent   g_onWakeUpEvent;

void    PLG__NotifyEvent(PLG_Event event, bool signal); ///< See in plgloader.c
void    IR__HandleSleep(bool isSleeping); ///< See in input_redirection.c

void    Sleep__Init(void)
{
	// No need anymore, handled by ServiceManager
    // srvSubscribe(0x214); ///< Sleep entry
    // srvSubscribe(0x213); ///< Sleep exit
	LightEvent_Init(&g_onWakeUpEvent, RESET_STICKY);
}

void    Sleep__HandleNotification(u32 notifId)
{
	if (notifId == 0x214) ///< Sleep entry
		{
			LightEvent_Clear(&g_onWakeUpEvent);
			g_isSleeping = true;
			// IR patch creates sleep issue, so we need to handle it
        IR__HandleSleep(g_isSleeping);
        // Plugins do not receive 0x214 notifications, so we send it via our custom service
        PLG__NotifyEvent(PLG_SLEEP_ENTRY, false);
		}
	else if (notifId == 0x213) ///< Sleep exit
		{
			g_isSleeping = false;
			LightEvent_Signal(&g_onWakeUpEvent);
			// IR patch creates sleep issue, so we need to handle it
        IR__HandleSleep(g_isSleeping);
        // Plugins actually receives 0x213 notifications, but since we send sleep entry, let's do sleep exit as well
        PLG__NotifyEvent(PLG_SLEEP_EXIT, false);
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
