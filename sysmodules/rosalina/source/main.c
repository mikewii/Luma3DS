/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include "memory.h"
#include "menu.h"
#include "service_manager.h"
#include "errdisp.h"
#include "hbloader.h"
#include "3dsx.h"
#include "utils.h"
#include "sleep.h"
#include "MyThread.h"
#include "menus/miscellaneous.h"
#include "menus/debugger.h"
#include "menus/screen_filters.h"
#include "menus/cheats.h"
#include "menus/sysconfig.h"
#include "input_redirection.h"
#include "minisoc.h"
#include "draw.h"
#include "task_runner.h"
#include "plugin.h"

bool isN3DS;

Result __sync_init(void);
Result __sync_fini(void);
void __libc_init_array(void);
void __libc_fini_array(void);

void __ctru_exit(int rc) { (void)rc; } // needed to avoid linking error

// this is called after main exits
void __wrap_exit(int rc)
{
    (void)rc;
    // TODO: make pm terminate rosalina
    __libc_fini_array();

    // Kernel will take care of it all
    /*
    pmDbgExit();
    fsExit();
    svcCloseHandle(*fsRegGetSessionHandle());
    srvExit();
    __sync_fini();*/

    svcExitProcess();
}

// this is called before main
void initSystem(void)
{
    s64 out;
    Result res;
    __sync_init();

    isN3DS = svcGetSystemInfo(&out, 0x10001, 0) == 0;

    svcGetSystemInfo(&out, 0x10000, 0x100);
    HBLDR_3DSX_TID = out == 0 ? HBLDR_DEFAULT_3DSX_TID : (u64)out;

    svcGetSystemInfo(&out, 0x10000, 0x101);
    menuCombo = out == 0 ? DEFAULT_MENU_COMBO : (u32)out;

    miscellaneousMenu.items[0].title = HBLDR_3DSX_TID == HBLDR_DEFAULT_3DSX_TID ? "Switch the hb. title to the current app." :
                                                                                  "Switch the hb. title to hblauncher_loader";

    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL))
    {
        res = srvInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            svcBreak(USERBREAK_PANIC);
    }

    if (R_FAILED(pmAppInit()) || R_FAILED(pmDbgInit()))
        svcBreak(USERBREAK_PANIC);

    if (R_FAILED(fsInit()))
        svcBreak(USERBREAK_PANIC);

    if (R_FAILED(FSUSER_SetPriority(-16)))
        svcBreak(USERBREAK_PANIC);

    // **** DO NOT init services that don't come from KIPs here ****
    // Instead, init the service only where it's actually init (then deinit it).

    __libc_init_array();

    // ROSALINA HACKJOB END

    // Rosalina specific:
    u32 *tls = (u32 *)getThreadLocalStorage();
    memset(tls, 0, 0x80);
    tls[0] = 0x21545624;

    // ROSALINA HACKJOB BEGIN
    // NORMAL APPS SHOULD NOT DO THIS, EVER
    srvSetBlockingPolicy(true); // GetServiceHandle nonblocking if service port is full
}

bool terminationRequest = false;
Handle terminationRequestEvent;

static void handleTermNotification(u32 notificationId)
{
    (void)notificationId;
    // Termination request
    terminationRequest = true;
    svcSignalEvent(terminationRequestEvent);
}

static void handleSleepNotification(u32 notificationId)
{
    ptmSysmInit();
    s32 ackValue = ptmSysmGetNotificationAckValue(notificationId);
    switch (notificationId)
    {
        case PTMNOTIFID_SLEEP_REQUESTED:
            PTMSYSM_ReplyToSleepQuery(miniSocEnabled); // deny sleep request if we have network stuff running
            break;
        case PTMNOTIFID_GOING_TO_SLEEP:
        case PTMNOTIFID_SLEEP_ALLOWED:
        case PTMNOTIFID_FULLY_WAKING_UP:
        case PTMNOTIFID_HALF_AWAKE:
            PTMSYSM_NotifySleepPreparationComplete(ackValue);
            break;
        case PTMNOTIFID_SLEEP_DENIED:
        case PTMNOTIFID_FULLY_AWAKE:
        default:
            break;
    }
    ptmSysmExit();
}

static void handlePreTermNotification(u32 notificationId)
{
    (void)notificationId;
    // Might be subject to a race condition, but heh.

    // Disable input redirection
    InputRedirection_Disable(100 * 1000 * 1000LL);

    // Ask the debugger to terminate in approx 2 * 100ms
    debuggerDisable(100 * 1000 * 1000LL);

    // Kill the ac session if needed
    if(isConnectionForced)
    {
        acExit();
        isConnectionForced = false;
        SysConfigMenu_UpdateStatus(true);
    }
}

static void handleNextApplicationDebuggedByForce(u32 notificationId)
{
    (void)notificationId;
    // Following call needs to be async because pm -> Loader depends on rosalina hb:ldr, handled in this very thread.
    TaskRunner_RunTask(debuggerFetchAndSetNextApplicationDebugHandleTask, NULL, 0);
}

static void handleRestartHbAppNotification(u32 notificationId)
{
    (void)notificationId;
    TaskRunner_RunTask(HBLDR_RestartHbApplication, NULL, 0);
}

static const ServiceManagerServiceEntry services[] = {
    { "hb:ldr", 2, HBLDR_HandleCommands, true },
    { "plg:ldr", 1, PluginLoader__HandleCommands, true },
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
<<<<<<< HEAD
    { 0x100 , handleTermNotification                },
    //{ 0x103 , relinquishConnectionSessions          }, // Sleep mode entry <=== causes issues
    { 0x214, Sleep__HandleNotification              },
    { 0x213, Sleep__HandleNotification              },
    { 0x1000, handleNextApplicationDebuggedByForce  },
    { 0x1001, PluginLoader__HandleKernelEvent       },
    { 0x2000, relinquishConnectionSessions          },
    { 0x3000, handleRestartHbAppNotification        },
=======
    { 0x100 ,                       handleTermNotification                  },
    { PTMNOTIFID_SLEEP_REQUESTED,   handleSleepNotification                 },
    { PTMNOTIFID_SLEEP_DENIED,      handleSleepNotification                 },
    { PTMNOTIFID_SLEEP_ALLOWED,     handleSleepNotification                 },
    { PTMNOTIFID_GOING_TO_SLEEP,    handleSleepNotification                 },
    { PTMNOTIFID_FULLY_WAKING_UP,   handleSleepNotification                 },
    { PTMNOTIFID_FULLY_AWAKE,       handleSleepNotification                 },
    { PTMNOTIFID_HALF_AWAKE,        handleSleepNotification                 },
    { 0x1000,                       handleNextApplicationDebuggedByForce    },
    { 0x2000,                       handlePreTermNotification               },
    { 0x3000,                       handleRestartHbAppNotification          },
>>>>>>> 2d58ec4... rosalina: prevent sleep mode entry if debugger/input redir is enabled to prevent lockup
    { 0x000, NULL },
};

int main(void)
{
    static u8 ipcBuf[0x100] = {0};  // used by both err:f and hb:ldr

    Sleep__Init();
    PluginLoader__Init();

    // Set up static buffers for IPC
    u32* bufPtrs = getThreadStaticBuffers();
    memset(bufPtrs, 0, 16 * 2 * 4);
    bufPtrs[0] = IPC_Desc_StaticBuffer(sizeof(ipcBuf), 0);
    bufPtrs[1] = (u32)ipcBuf;
    bufPtrs[2] = IPC_Desc_StaticBuffer(sizeof(ldrArgvBuf), 1);
    bufPtrs[3] = (u32)ldrArgvBuf;

    if(R_FAILED(svcCreateEvent(&terminationRequestEvent, RESET_STICKY)))
        svcBreak(USERBREAK_ASSERT);

    Draw_Init();
    Cheat_SeedRng(svcGetSystemTick());

    MyThread *menuThread = menuCreateThread();
    MyThread *taskRunnerThread = taskRunnerCreateThread();
    MyThread *errDispThread = errDispCreateThread();

    if (R_FAILED(ServiceManager_Run(services, notifications, NULL)))
        svcBreak(USERBREAK_PANIC);

    TaskRunner_Terminate();

    MyThread_Join(menuThread, -1LL);
    MyThread_Join(taskRunnerThread, -1LL);
    MyThread_Join(errDispThread, -1LL);

    return 0;
}
