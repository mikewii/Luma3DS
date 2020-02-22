#pragma once

#include <3ds/types.h>
#include "menus.h"

extern Menu MMMenu;
extern Menu MMSelectedProcessMenu;
extern Menu NFCMenu;

typedef struct ProcessInfo {
    u32 pid;
    u64 titleId;
    char name[8];
    bool isZombie;
} ProcessInfo;

void	MM__MenuCallback(void);
void	MM__UpdateMenu(void);
bool	MM__IsEnabled(void);

// Draw functions:
void	MM__ShowFreeMemory(void);
void	MM__ShowMemoryRegions(const ProcessInfo *info);
void	MM__ShowProcessList(void);

// Tools functions:
void	MM__GetFreeMemory(char* out);
s32 	MM__GetMemRegions(Handle handle);
Handle	MM__GetProcessHandle(u32 pid);
void	MM__MapProcessMemory(const ProcessInfo *info);
int		MM__MemorySegmentFormatInfoLine(char *out, const MemInfo *info);
void	MM__UnlockMemoryRegion(u32 pid, const MemInfo *info);

void	MM__MapToRosalina(u32 pid, s32 SegmentsAmount);
void	MM__UnMapFromRosalina(u32 pid, s32 SegmentsAmount);

int 	MM__ProcessListFormatInfoLine(char *out, const ProcessInfo *info);
s32 	MM__FetchProcessInfo(void);

void	MM__nfcInit(void);
void	MM__nfcExit(void);