/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../../idlib/precompiled.h"
#include "../sys_local.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <signal.h>
#include <fcntl.h>

#include "posix_public.h"

#if defined(__ANDROID__)
#include <android/log.h>
#define vprintf(msg, argptr)	__android_log_vprint(ANDROID_LOG_INFO, "Dante", msg, argptr);
#endif

#define					MAX_OSPATH 256
#define					COMMAND_HISTORY 64

static int				input_hide = 0;

idEditField				input_field;
static char				input_ret[256];

static idStr			history[ COMMAND_HISTORY ];	// cycle buffer
static int				history_count = 0;			// buffer fill up
static int				history_start = 0;			// current history start
static int				history_current = 0;			// goes back in history
idEditField				history_backup;				// the base edit line

// pid - useful when you attach to gdb..
idCVar com_pid("com_pid", "0", CVAR_INTEGER | CVAR_INIT | CVAR_SYSTEM, "process id");

// exit - quit - error --------------------------------------------------------

static int set_exit = 0;
static char exit_spawn[ 1024 ];

/*
================
Posix_Exit
================
*/
void Posix_Exit(int ret)
{
	// at this point, too late to catch signals
	Posix_ClearSigs();

	if (asyncThread.threadHandle) {
		Sys_DestroyThread(asyncThread);
	}

	// process spawning. it's best when it happens after everything has shut down
	if (exit_spawn[0]) {
		Sys_DoStartProcess(exit_spawn, false);
	}

	// in case of signal, handler tries a common->Quit
	// we use set_exit to maintain a correct exit code
	if (set_exit) {
		exit(set_exit);
	}

	exit(ret);
}

/*
================
Posix_SetExit
================
*/
void Posix_SetExit(int ret)
{
	set_exit = 0;
}

/*
===============
Posix_SetExitSpawn
set the process to be spawned when we quit
===============
*/
void Posix_SetExitSpawn(const char *exeName)
{
	idStr::Copynz(exit_spawn, exeName, 1024);
}

/*
==================
idSysLocal::StartProcess
if !quit, start the process asap
otherwise, push it for execution at exit
(i.e. let complete shutdown of the game and freeing of resources happen)
NOTE: might even want to add a small delay?
==================
*/
void idSysLocal::StartProcess(const char *exeName, bool quit)
{
	if (quit) {
		common->DPrintf("Sys_StartProcess %s (delaying until final exit)\n", exeName);
		Posix_SetExitSpawn(exeName);
		cmdSystem->BufferCommandText(CMD_EXEC_APPEND, "quit\n");
		return;
	}

	common->DPrintf("Sys_StartProcess %s\n", exeName);
	Sys_DoStartProcess(exeName);
}

/*
================
Sys_Quit
================
*/
void Sys_Quit(void)
{
	Posix_Exit(EXIT_SUCCESS);
}

/*
================
Sys_Milliseconds
================
*/
/* base time in seconds, that's our origin
   timeval:tv_sec is an int:
   assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038
   using unsigned long data type to work right with Sys_XTimeToSysTime */
unsigned long sys_timeBase = 0;
/* current time in ms, using sys_timeBase as origin
   NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
     0x7fffffff ms - ~24 days
		 or is it 48 days? the specs say int, but maybe it's casted from unsigned int?
*/
int Sys_Milliseconds(void)
{
	int curtime;
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if (!sys_timeBase) {
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec / 1000;
	}

	curtime = (tp.tv_sec - sys_timeBase) * 1000 + tp.tv_usec / 1000;

	return curtime;
}

uint64_t Sys_Microseconds()
{
    struct timeval tp;

    gettimeofday(&tp, NULL);

    return tp.tv_sec * uint64_t(1000000) + tp.tv_usec;
}

/*
================
Sys_Mkdir
================
*/
void Sys_Mkdir(const char *path)
{
	mkdir(path, 0777);
}

/*
================
Sys_ListFiles
================
*/
int Sys_ListFiles(const char *directory, const char *extension, idStrList &list)
{
	struct dirent *d;
	DIR *fdir;
	bool dironly = false;
	char search[MAX_OSPATH];
	struct stat st;
	bool debug;

	list.Clear();

	debug = cvarSystem->GetCVarBool("fs_debug");

	if (!extension)
		extension = "";

	// passing a slash as extension will find directories
	if (extension[0] == '/' && extension[1] == 0) {
		extension = "";
		dironly = true;
	}

	// search
	// NOTE: case sensitivity of directory path can screw us up here
	if ((fdir = opendir(directory)) == NULL) {
		if (debug) {
			common->Printf("Sys_ListFiles: opendir %s failed\n", directory);
		}

		return -1;
	}

	while ((d = readdir(fdir)) != NULL) {
		idStr::snPrintf(search, sizeof(search), "%s/%s", directory, d->d_name);

		if (stat(search, &st) == -1)
			continue;

		if (!dironly) {
			idStr look(search);
			idStr ext;
			look.ExtractFileExtension(ext);

			if (extension[0] != '\0' && ext.Icmp(&extension[1]) != 0) {
				continue;
			}
		}

		if ((dironly && !(st.st_mode & S_IFDIR)) ||
		    (!dironly && (st.st_mode & S_IFDIR)))
			continue;

		list.Append(d->d_name);
	}

	closedir(fdir);

	if (debug) {
		common->Printf("Sys_ListFiles: %d entries in %s\n", list.Num(), directory);
	}

	return list.Num();
}

/*
============================================================================
EVENT LOOP
============================================================================
*/

#define	MAX_QUED_EVENTS		256
#define	MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

static sysEvent_t eventQue[MAX_QUED_EVENTS];
static int eventHead, eventTail;

/*
================
Posix_QueEvent

ptr should either be null, or point to a block of data that can be freed later
================
*/
void Posix_QueEvent(sysEventType_t type, int value, int value2,
                    int ptrLength, void *ptr)
{
	sysEvent_t *ev;

	ev = &eventQue[eventHead & MASK_QUED_EVENTS];

	if (eventHead - eventTail >= MAX_QUED_EVENTS) {
		common->Printf("Posix_QueEvent: overflow\n");

		// we are discarding an event, but don't leak memory
		// TTimo: verbose dropped event types?
		if (ev->evPtr) {
			Mem_Free(ev->evPtr);
			ev->evPtr = NULL;
		}

		eventTail++;
	}

	eventHead++;

	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;

#if 0
	common->Printf("Event %d: %d %d\n", ev->evType, ev->evValue, ev->evValue2);
#endif
}

/*
================
Sys_GetEvent
================
*/
sysEvent_t Sys_GetEvent(void)
{
	static sysEvent_t ev;

	// return if we have data
	if (eventHead > eventTail) {
		eventTail++;
		return eventQue[(eventTail - 1) & MASK_QUED_EVENTS];
	}

	// return the empty event with the current time
	memset(&ev, 0, sizeof(ev));

	return ev;
}

/*
================
Sys_ClearEvents
================
*/
void Sys_ClearEvents(void)
{
	eventHead = eventTail = 0;
}

/*
================
Posix_Cwd
================
*/
const char *Posix_Cwd(void)
{
	static char cwd[MAX_OSPATH];

	getcwd(cwd, sizeof(cwd) - 1);
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
=================
Sys_GetMemoryStatus
=================
*/
void Sys_GetMemoryStatus(sysMemoryStats_t &stats)
{
	common->Printf("FIXME: Sys_GetMemoryStatus stub\n");
}

void Sys_GetCurrentMemoryStatus(sysMemoryStats_t &stats)
{
	common->Printf("FIXME: Sys_GetCurrentMemoryStatus\n");
}

void Sys_GetExeLaunchMemoryStatus(sysMemoryStats_t &stats)
{
	common->Printf("FIXME: Sys_GetExeLaunchMemoryStatus\n");
}

/*
=================
Sys_Init
Posix_EarlyInit/Posix_LateInit is better
=================
*/
void Sys_Init(void) { }

/*
=================
Posix_Shutdown
=================
*/
void Posix_Shutdown(void)
{
	for (int i = 0; i < COMMAND_HISTORY; i++) {
		history[ i ].Clear();
	}
}

/*
=================
Sys_DLL_Load
TODO: OSX - use the native API instead? NSModule
=================
*/
intptr_t Sys_DLL_Load(const char *path)
{
	void *handle = dlopen(path, RTLD_NOW);

	if (!handle) {
		Sys_Printf("dlopen '%s' failed: %s\n", path, dlerror());
	}

	return (intptr_t)handle;
}

/*
=================
Sys_DLL_GetProcAddress
=================
*/
void *Sys_DLL_GetProcAddress(intptr_t handle, const char *sym)
{
	const char *error;
	void *ret = dlsym((void *)handle, sym);

	if ((error = dlerror()) != NULL)  {
		Sys_Printf("dlsym '%s' failed: %s\n", sym, error);
	}

	return ret;
}

/*
=================
Sys_DLL_Unload
=================
*/
void Sys_DLL_Unload(intptr_t handle)
{
	dlclose((void *)handle);
}

/*
================
Sys_ShowConsole
================
*/
void Sys_ShowConsole(int visLevel, bool quitOnClose) { }

// ---------------------------------------------------------------------------

// only relevant when specified on command line
const char *Sys_DefaultCDPath(void)
{
	return "";
}

long Sys_FileTimeStamp(FILE *fp)
{
	struct stat st;
	fstat(fileno(fp), &st);
	return st.st_mtime;
}

void Sys_Sleep(int msec)
{
	if (msec < 20) {
		static int last = 0;
		int now = Sys_Milliseconds();

		if (now - last > 1000) {
			Sys_Printf("WARNING: Sys_Sleep - %d < 20 msec is not portable\n", msec);
			last = now;
		}

		// ignore that sleep call, keep going
		return;
	}

	// use nanosleep? keep sleeping if signal interrupt?
	if (usleep(msec * 1000) == -1)
		Sys_Printf("usleep: %s\n", strerror(errno));
}

char *Sys_GetClipboardData(void)
{
	Sys_Printf("TODO: Sys_GetClipboardData\n");
	return NULL;
}

void Sys_SetClipboardData(const char *string)
{
	Sys_Printf("TODO: Sys_SetClipboardData\n");
}


// stub pretty much everywhere - heavy calling
void Sys_FlushCacheMemory(void *base, int bytes)
{
//  Sys_Printf("Sys_FlushCacheMemory stub\n");
}

bool Sys_FPU_StackIsEmpty(void)
{
	return true;
}

void Sys_FPU_ClearStack(void)
{
}

const char *Sys_FPU_GetState(void)
{
	return "";
}

void Sys_FPU_SetPrecision(int precision)
{
}

/*
================
Sys_LockMemory
================
*/
bool Sys_LockMemory(void *ptr, int bytes)
{
	return true;
}

/*
================
Sys_UnlockMemory
================
*/
bool Sys_UnlockMemory(void *ptr, int bytes)
{
	return true;
}

/*
================
Sys_SetPhysicalWorkMemory
================
*/
void Sys_SetPhysicalWorkMemory(int minBytes, int maxBytes)
{
	common->DPrintf("TODO: Sys_SetPhysicalWorkMemory\n");
}

/*
===========
Sys_GetDriveFreeSpace
return in MegaBytes
===========
*/
int Sys_GetDriveFreeSpace(const char *path)
{
	common->DPrintf("TODO: Sys_GetDriveFreeSpace\n");
	return 1000 * 1024;
}

/*
================
Sys_AlreadyRunning
return true if there is a copy of D3 running already
================
*/
bool Sys_AlreadyRunning(void)
{
	return false;
}

/*
===============
Posix_EarlyInit
===============
*/
void Posix_EarlyInit(void)
{
	memset(&asyncThread, 0, sizeof(asyncThread));
	exit_spawn[0] = '\0';
	Posix_InitSigs();
	// set the base time
	Sys_Milliseconds();
	Posix_InitPThreads();
}

/*
===============
Posix_LateInit
===============
*/
void Posix_LateInit(void)
{
	Posix_InitConsoleInput();
	com_pid.SetInteger(getpid());
	common->Printf("pid: %d\n", com_pid.GetInteger());
	common->Printf("%d MB System Memory\n", Sys_GetSystemRam());
#ifndef ID_DEDICATED
	common->Printf("%d MB Video Memory\n", Sys_GetVideoRam());
#endif
	Posix_StartAsyncThread();
}

/*
===============
Posix_InitConsoleInput
===============
*/
void Posix_InitConsoleInput(void)
{
	Sys_Printf("terminal support disabled\n");
}

/*
called during frame loops, pacifier updates etc.
this is only for console input polling and misc mouse grab tasks
the actual mouse and keyboard input is in the Sys_Poll logic
*/
void Sys_GenerateEvents(void)
{
}

/*
===============
low level output
===============
*/

void Sys_DebugPrintf(const char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

void Sys_DebugVPrintf(const char *fmt, va_list arg)
{
	vprintf(fmt, arg);
}

void Sys_Printf(const char *msg, ...)
{
	va_list argptr;

	va_start(argptr, msg);
	vprintf(msg, argptr);
	va_end(argptr);
}

void Sys_VPrintf(const char *msg, va_list arg)
{
	vprintf(msg, arg);
}

/*
================
Sys_Error
================
*/
void Sys_Error(const char *error, ...)
{
	va_list argptr;

	Sys_Printf("Sys_Error: ");
	va_start(argptr, error);
	Sys_DebugVPrintf(error, argptr);
	va_end(argptr);
	Sys_Printf("\n");

	Posix_Exit(EXIT_FAILURE);
}

/*
===============
Sys_FreeOpenAL
===============
*/
void Sys_FreeOpenAL(void) { }
