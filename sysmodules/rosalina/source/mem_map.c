#include <3ds.h>
#include <3ds/os.h>
#include "mem_map.h"
#include "miscellaneous.h"
#include "fmt.h"
//#include "utils.h"

#include "draw.h"
#include "gdb/remote_command.h"
//#include "menus/process_list.h"
#define CPU_TICKS_PER_MSEC (SYSCLOCK_ARM11 / 1000.0)


#define CUR_PROCESS_HANDLE 0xFFFF8001
#define DISPLAY_BUF 0x1024
#define MEM_LIMIT 0x40000000
#define _GDB_BUF_LEN 4096

#define PROCESSES_PER_MENU_PAGE 18
#define MEMSEGMENTS_PER_MENU_PAGE 17


static bool mapped = false;
static ProcessInfo 	infos[0x40] = {0}, infosPrev[0x40] = {0};
static MemInfo		memi[0x40] = {0};//, memiPrev[0x40] = {0};


Menu MMMenu = {
    "Mem Map - menu",
    .nbItems = 2,
    {
        { "Get free memory.", METHOD, .method = &MM__ShowFreeMemory },
        { "Process List.", METHOD, .method = &MM__ShowProcessList },
        //{ "NFC Tests.", MENU, .menu = &NFCMenu },
    }
};

Menu PGSelectedProcessMenu = {
	"Mem Map -- Process -- Actions",
	.nbItems = 2,
	{
		//{ "Get Memory Regions", METHOD, .method = &MM__ShowMemoryRegions },
		//{ "Unlock Memory Regions", METHOD, .method = &MM__UnlockMemoryRegions },
		//{ "Map Playground", METHOD, .method = &MM__MapProcessMemory },
	}
};

void 	MM__GetFreeMemory(char* out) {
	extern bool isN3DS;
	MemOp memSys = MEMOP_REGION_SYSTEM;
	MemOp memBase = MEMOP_REGION_BASE;
	MemOp memApp = MEMOP_REGION_APP;

	u32 sysFree = (u32)osGetMemRegionFree(memSys);
	u32 baseFree = (u32)osGetMemRegionFree(memBase);
	u32 appFree = (u32)osGetMemRegionFree(memApp);

	
	sprintf(out, "IsN3DS: %d\nSysFree : %lX\nBaseFree: %lX\nAppFree : %lX\n", 
		isN3DS, 
		sysFree, 
		baseFree, 
		appFree);
}

void	MM__ShowFreeMemory(void) {
	Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    char out[DISPLAY_BUF];
    MM__GetFreeMemory(out);
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Memory Map -- Free Memory");
        Draw_DrawString(10, 30, COLOR_WHITE, out);
   		Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void	MM__ShowMemoryRegions(const ProcessInfo *info) {
	Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    Handle 	handle;
    char 	title[128];
    char 	runtimeBuf[64];
    s32 memSegmentsAmount;
    s32 selected = 0, page = 0, pagePrev = 0;
    

    handle = MM__GetProcessHandle(info->pid);
    //MM__MapProcessMemory(info);
    //u64 tStart = svcGetSystemTick();
    memSegmentsAmount = MM__GetMemRegions(handle);
    //u64 tEnd = svcGetSystemTick();
    //u64 runtime = (tEnd - tStart);
    //u32 handleN = handle;
    svcCloseHandle(handle);


    sprintf(title, "Mem Map -- %s -- Memory Regions\nPID: %ld TitleID: %llX | MemSegments: %ld",
    	info->name,
    	info->pid,
    	info->titleId,
    	memSegmentsAmount);

    //sprintf(runtimeBuf, "Runtime: %lld / 33513.982 = ? | Handle: %lX", runtime, handleN);
    sprintf(runtimeBuf, "Choose segment and press A to change perm to rw- ");

    do
    {
        Draw_Lock();
        if(page != pagePrev)
        	Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, title);
        Draw_DrawString(10, 230, COLOR_TITLE, runtimeBuf);

        for(s32 i = 0;
        	i < MEMSEGMENTS_PER_MENU_PAGE && page * MEMSEGMENTS_PER_MENU_PAGE + i < memSegmentsAmount;
        	i++)
        {
        	char buf[65] = {0};
        	MM__MemorySegmentFormatInfoLine(buf, &memi[page * MEMSEGMENTS_PER_MENU_PAGE + i]);

        	Draw_DrawString(30, 40 + i * SPACING_Y, COLOR_WHITE, buf);
        	Draw_DrawCharacter(10, 40 + i * SPACING_Y, COLOR_TITLE, page * MEMSEGMENTS_PER_MENU_PAGE + i == selected ? '>': ' ');
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        if(terminationRequest)
	        break;

	    u32 pressed;
	    do
	    {
	        pressed = waitInputWithTimeout(50);
	        if(pressed != 0)
	            break;
	    }
	    while(pressed == 0 && !terminationRequest);

	    if(pressed & BUTTON_B)
	        break;
	    else if(pressed & BUTTON_A) {
	        MM__UnlockMemoryRegion(info->pid, &memi[selected]);

	        Draw_Lock();
		    Draw_ClearFramebuffer();
		    Draw_FlushFramebuffer();
		    Draw_Unlock();
	    }
	    else if(pressed & BUTTON_DOWN)
	        selected++;
	    else if(pressed & BUTTON_UP)
	        selected--;
	    else if(pressed & BUTTON_LEFT)
	        selected -= MEMSEGMENTS_PER_MENU_PAGE;
	    else if(pressed & BUTTON_RIGHT)
	    {
	        if(selected + MEMSEGMENTS_PER_MENU_PAGE < memSegmentsAmount)
	            selected += MEMSEGMENTS_PER_MENU_PAGE;
	        else if((memSegmentsAmount - 1) / MEMSEGMENTS_PER_MENU_PAGE == page)
	            selected %= MEMSEGMENTS_PER_MENU_PAGE;
	        else
	            selected = memSegmentsAmount - 1;
	    }

	    if(selected < 0)
            selected = memSegmentsAmount - 1;
        else if(selected >= memSegmentsAmount)
            selected = 0;

	        pagePrev = page;
	        page = selected / MEMSEGMENTS_PER_MENU_PAGE;
    }
    while(!terminationRequest);

}



s32 	MM__GetMemRegions(Handle handle) {
	u32 		address = 0;
	PageInfo 	pagei;
	MemInfo		memi_t;
	s32			i = 0;
	
	
	while (address < MEM_LIMIT ///< Limit to check for regions
        && R_SUCCEEDED(svcQueryProcessMemory(&memi_t, &pagei, handle, address))
        && (i < 0x40))
		{
			// Update the address for next region
			address = memi_t.base_addr + memi_t.size;
			if (memi_t.state != MEMSTATE_FREE) {
				// copy
				memi[i] = memi_t;
				if(memi[i].base_addr == 0x00100000 && (mapped != true)) {
					mapped = true;
					//svcMapProcessMemoryEx(handle, memi.base_addr + 0x20000000, memi.base_addr, memi.size);
				}
                i++;
			}
		}
		return i;
}

Handle	MM__GetProcessHandle(u32 pid) {
	/* Obtain current process handle: */
	Handle handle;

	//svcGetProcessId(&pid, CUR_PROCESS_HANDLE);
	svcOpenProcess(&handle, pid);

	return handle;
}


void	MM__ShowProcessList(void) {
	s32 processAmount = MM__FetchProcessInfo();
    s32 selected = 0, page = 0, pagePrev = 0;

    do
    {
	    memcpy(infosPrev, infos, sizeof(infos));

	    Draw_Lock();
	    if(page != pagePrev)
	        Draw_ClearFramebuffer();
	    Draw_DrawString(10, 10, COLOR_TITLE, "Mem Map -- Process list");

	    for(s32 i = 0; i < PROCESSES_PER_MENU_PAGE && page * PROCESSES_PER_MENU_PAGE + i < processAmount; i++)
	    {
	        char buf[65] = {0};
	        MM__ProcessListFormatInfoLine(buf, &infos[page * PROCESSES_PER_MENU_PAGE + i]);

	        Draw_DrawString(30, 30 + i * SPACING_Y, COLOR_WHITE, buf);
	        Draw_DrawCharacter(10, 30 + i * SPACING_Y, COLOR_TITLE, page * PROCESSES_PER_MENU_PAGE + i == selected ? '>' : ' ');
	    }

	    Draw_FlushFramebuffer();
	    Draw_Unlock();

	    if(terminationRequest)
	        break;

	    u32 pressed;
	    do
	    {
	        pressed = waitInputWithTimeout(50);
	        if(pressed != 0)
	            break;
	        processAmount = MM__FetchProcessInfo();
	        if(memcmp(infos, infosPrev, sizeof(infos)) != 0)
	            break;
	    }
	    while(pressed == 0 && !terminationRequest);

	    if(pressed & BUTTON_B)
	        break;
	    else if(pressed & BUTTON_A) {
	        MM__ShowMemoryRegions(&infos[selected]);

	        Draw_Lock();
		    Draw_ClearFramebuffer();
		    Draw_FlushFramebuffer();
		    Draw_Unlock();
	    }
	    else if(pressed & BUTTON_DOWN)
	        selected++;
	    else if(pressed & BUTTON_UP)
	        selected--;
	    else if(pressed & BUTTON_LEFT)
	        selected -= PROCESSES_PER_MENU_PAGE;
	    else if(pressed & BUTTON_RIGHT)
	    {
	        if(selected + PROCESSES_PER_MENU_PAGE < processAmount)
	            selected += PROCESSES_PER_MENU_PAGE;
	        else if((processAmount - 1) / PROCESSES_PER_MENU_PAGE == page)
	            selected %= PROCESSES_PER_MENU_PAGE;
	        else
	            selected = processAmount - 1;
	    }

	    if(selected < 0)
            selected = processAmount - 1;
        else if(selected >= processAmount)
            selected = 0;

	        pagePrev = page;
	        page = selected / PROCESSES_PER_MENU_PAGE;
    }
    while(!terminationRequest);
}

int 	MM__ProcessListFormatInfoLine(char *out, const ProcessInfo *info) {
    return sprintf(out, "%-4lu    %-8.8s", info->pid, info->name); // Theoritically PIDs are 32-bit ints, but we'll only justify 4 digits
}

int		MM__MemorySegmentFormatInfoLine(char *out, const MemInfo *info) {
	u32 addr1 = info->base_addr + info->size;
	const char *perm = FormatMemPerm(info->perm);
    const char *state = FormatMemState(info->state);

    u32 PA = svcConvertVAToPA((const void *)info->base_addr, false);
    
	return sprintf(out, "%08lX - %08lX %s %s\t| PA: %lX\n",
        info->base_addr, addr1, perm, state, PA);
}

s32 	MM__FetchProcessInfo(void) {
    u32 pidList[0x40];
    s32 processAmount;

    svcGetProcessList(&processAmount, pidList, 0x40);

    for(s32 i = 0; i < processAmount; i++)
    {
        Handle processHandle;
        Result res = svcOpenProcess(&processHandle, pidList[i]);
        if(R_FAILED(res))
            continue;

        infos[i].pid = pidList[i];
        svcGetProcessInfo((s64 *)&infos[i].name, processHandle, 0x10000);
        svcGetProcessInfo((s64 *)&infos[i].titleId, processHandle, 0x10001);
        infos[i].isZombie = svcWaitSynchronization(processHandle, 0) == 0;
        svcCloseHandle(processHandle);
    }

    return processAmount;
}

void	MM__UnlockMemoryRegion(u32 pid, const MemInfo *info) {
	Handle handle = MM__GetProcessHandle(pid);
	svcControlProcessMemory(
        handle,
        info->base_addr,
        info->base_addr,
        info->size,
        MEMOP_PROT,
        MEMPERM_READ | MEMPERM_WRITE);
	svcCloseHandle(handle);
}