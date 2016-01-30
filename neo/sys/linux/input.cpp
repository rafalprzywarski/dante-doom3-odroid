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
#include "../posix/posix_public.h"
#include "local.h"

#include <pthread.h>
#include <termios.h>
#include <SDL/SDL.h>

idCVar in_mouse("in_mouse", "1", CVAR_SYSTEM | CVAR_ARCHIVE, "");
idCVar in_dgamouse("in_dgamouse", "1", CVAR_SYSTEM | CVAR_ARCHIVE, "");
idCVar in_nograb("in_nograb", "0", CVAR_SYSTEM | CVAR_NOCHEAT, "");

// have a working xkb extension
static bool have_xkb = false;

// toggled by grab calls - decides if we ignore MotionNotify events
static bool mouse_active = false;

// time mouse was last reset, we ignore the first 50ms of the mouse to allow settling of events
static int mouse_reset_time = 0;
#define MOUSE_RESET_DELAY 50

// backup original values for pointer grab/ungrab
static int mouse_accel_numerator;
static int mouse_accel_denominator;
static int mouse_threshold;

static byte s_scantokey[128] = {
	/*  0 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*  8 */ 0, 27, '1', '2', '3', '4', '5', '6', // 27 - ESC
	/* 10 */ '7', '8', '9', '0', '-', '=', K_BACKSPACE, 9, // 9 - TAB
	/* 18 */ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	/* 20 */ 'o', 'p', '[', ']', K_ENTER, K_CTRL, 'a', 's',
	/* 28 */ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	/* 30 */ '\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',
	/* 38 */ 'b', 'n', 'm', ',', '.', '/', K_SHIFT, K_KP_STAR,
	/* 40 */ K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,
	/* 48 */ K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
	/* 50 */ K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5, K_RIGHTARROW, K_KP_PLUS, K_END,
	/* 58 */ K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, '\\', K_F11,
	/* 60 */ K_F12, K_HOME, K_UPARROW, K_PGUP, K_LEFTARROW, 0, K_RIGHTARROW, K_END,
	/* 68 */ K_DOWNARROW, K_PGDN, K_INS, K_DEL, K_ENTER, K_CTRL, K_PAUSE, 0,
	/* 70 */ '/', K_ALT, 0, 0, 0, 0, 0, 0,
	/* 78 */ 0, 0, 0, 0, 0, 0, 0, 0
};

/*
=================
IN_Clear_f
=================
*/
void IN_Clear_f(const idCmdArgs &args)
{
	idKeyInput::ClearStates();
}

static int kbhit( void )
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
	select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

#define NB_DISABLE 	0
#define NB_ENABLE	1

static void nonblock(int state)
{
	struct termios ttystate;

	//get the terminal state
	tcgetattr(STDIN_FILENO, &ttystate);

	if (state==NB_ENABLE)
	{
		//turn off canonical mode
		ttystate.c_lflag &= ~ICANON;
		//minimum of number input read.
		ttystate.c_cc[VMIN] = 1;
	}
	else if (state==NB_DISABLE)
	{
		//turn on canonical mode
		ttystate.c_lflag |= ICANON;
	}
	//set the terminal attributes.
	tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/*
=================
Sys_InitInput
=================
*/
void Sys_InitInput(void)
{
	common->Printf("\n------- Input Initialization -------\n");
	cmdSystem->AddCommand("in_clear", IN_Clear_f, CMD_FL_SYSTEM, "reset the input keys");
  nonblock(NB_ENABLE);
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WarpMouse((glConfig.vidWidth/2),(glConfig.vidHeight/2));
}

//#define XEVT_DBG
//#define XEVT_DBG2

void Sys_GrabMouseCursor(bool)
{
}

/**
 * Intercept a KeyRelease-KeyPress sequence and ignore
 */

				//Posix_QueEvent(SE_CHAR, buf[ 0 ], 0, 0, NULL);
				// shouldn't we be doing a release/press in this order rather?
				// ( doesn't work .. but that's what I would have expected to do though )
				//Posix_QueEvent(SE_KEY, s_scantokey[peekevent.xkey.keycode], true, 0, NULL);
				//Posix_QueEvent(SE_KEY, s_scantokey[peekevent.xkey.keycode], false, 0, NULL);

/*
==========================
Posix_PollInput
==========================
*/
void Posix_PollInput()
{
    // from Quake3 sdl_input.c
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
    	switch(event.type)
    	{
            case SDL_MOUSEMOTION:
                common->Printf("SDL_MOUSEMOTION: xrel: %d yrel: %d\n", (int) event.motion.xrel, (int) event.motion.yrel);
                if (!in_dgamouse.GetBool())
                    SDL_WarpMouse((glConfig.vidWidth/2),(glConfig.vidHeight/2));

                int dx = event.motion.xrel;
                int dy = event.motion.yrel;
                Posix_QueEvent(SE_MOUSE, dx, dy, 0, NULL);
                Posix_AddMousePollEvent(M_DELTAX, dx);
                Posix_AddMousePollEvent(M_DELTAY, dy);
                break;
        }
    }


  /*
	static char buf[16];
	static XEvent event;
	static XKeyEvent *key_event = (XKeyEvent *)&event;
	int lookupRet;
	int b, dx, dy;
	KeySym keysym;

	if (!dpy) {
		return;
	}

	// NOTE: Sys_GetEvent only calls when there are no events left
	// but here we pump all X events that have accumulated
	// pump one by one? or use threaded input?
	while (XPending(dpy)) {
		XNextEvent(dpy, &event);

		switch (event.type) {
			case KeyPress:
#ifdef XEVT_DBG

				if (key_event->keycode > 0x7F)
					common->DPrintf("WARNING: KeyPress keycode > 0x7F");

#endif
				key_event->keycode &= 0x7F;
#ifdef XEVT_DBG2
				printf("SE_KEY press %d\n", key_event->keycode);
#endif
				Posix_QueEvent(SE_KEY, s_scantokey[key_event->keycode], true, 0, NULL);
				lookupRet = XLookupString(key_event, buf, sizeof(buf), &keysym, NULL);

				if (lookupRet > 0) {
					char s = buf[0];
#ifdef XEVT_DBG

					if (buf[1]!=0)
						common->DPrintf("WARNING: got XLookupString buffer '%s' (%d)\n", buf, strlen(buf));

#endif
#ifdef XEVT_DBG2
					printf("SE_CHAR %s\n", buf);
#endif
					Posix_QueEvent(SE_CHAR, s, 0, 0, NULL);
				}

				if (!Posix_AddKeyboardPollEvent(s_scantokey[key_event->keycode], true))
					return;

				break;

			case KeyRelease:

				if (Sys_XRepeatPress(&event)) {
#ifdef XEVT_DBG2
					printf("RepeatPress\n");
#endif
					continue;
				}

#ifdef XEVT_DBG

				if (key_event->keycode > 0x7F)
					common->DPrintf("WARNING: KeyRelease keycode > 0x7F");

#endif
				key_event->keycode &= 0x7F;
#ifdef XEVT_DBG2
				printf("SE_KEY release %d\n", key_event->keycode);
#endif
				Posix_QueEvent(SE_KEY, s_scantokey[key_event->keycode], false, 0, NULL);

				if (!Posix_AddKeyboardPollEvent(s_scantokey[key_event->keycode], false))
					return;

				break;

			case ButtonPress:

				if (event.xbutton.button == 4) {
					Posix_QueEvent(SE_KEY, K_MWHEELUP, true, 0, NULL);

					if (!Posix_AddMousePollEvent(M_DELTAZ, 1))
						return;
				} else if (event.xbutton.button == 5) {
					Posix_QueEvent(SE_KEY, K_MWHEELDOWN, true, 0, NULL);

					if (!Posix_AddMousePollEvent(M_DELTAZ, -1))
						return;
				} else {
					b = -1;

					if (event.xbutton.button == 1) {
						b = 0;		// K_MOUSE1
					} else if (event.xbutton.button == 2) {
						b = 2;		// K_MOUSE3
					} else if (event.xbutton.button == 3) {
						b = 1;		// K_MOUSE2
					} else if (event.xbutton.button == 6) {
						b = 3;		// K_MOUSE4
					} else if (event.xbutton.button == 7) {
						b = 4;		// K_MOUSE5
					}

					if (b == -1 || b > 4) {
						common->DPrintf("X ButtonPress %d not supported\n", event.xbutton.button);
					} else {
						Posix_QueEvent(SE_KEY, K_MOUSE1 + b, true, 0, NULL);

						if (!Posix_AddMousePollEvent(M_ACTION1 + b, true))
							return;
					}
				}

				break;

			case ButtonRelease:

				if (event.xbutton.button == 4) {
					Posix_QueEvent(SE_KEY, K_MWHEELUP, false, 0, NULL);
				} else if (event.xbutton.button == 5) {
					Posix_QueEvent(SE_KEY, K_MWHEELDOWN, false, 0, NULL);
				} else {
					b = -1;

					if (event.xbutton.button == 1) {
						b = 0;
					} else if (event.xbutton.button == 2) {
						b = 2;
					} else if (event.xbutton.button == 3) {
						b = 1;
					} else if (event.xbutton.button == 6) {
						b = 3;		// K_MOUSE4
					} else if (event.xbutton.button == 7) {
						b = 4;		// K_MOUSE5
					}

					if (b == -1 || b > 4) {
						common->DPrintf("X ButtonRelease %d not supported\n", event.xbutton.button);
					} else {
						Posix_QueEvent(SE_KEY, K_MOUSE1 + b, false, 0, NULL);

						if (!Posix_AddMousePollEvent(M_ACTION1 + b, false))
							return;
					}
				}

				break;

			case MotionNotify:

				if (!mouse_active)
					break;

				if (in_dgamouse.GetBool()) {
					dx = event.xmotion.x_root;
					dy = event.xmotion.y_root;

					Posix_QueEvent(SE_MOUSE, dx, dy, 0, NULL);

					// if we overflow here, we'll get a warning, but the delta will be completely processed anyway
					Posix_AddMousePollEvent(M_DELTAX, dx);

					if (!Posix_AddMousePollEvent(M_DELTAY, dy))
						return;
				} else {
					// if it's a center motion, we've just returned from our warp
					// FIXME: we generate mouse delta on wrap return, but that lags us quite a bit from the initial event..
					if (event.xmotion.x == glConfig.vidWidth / 2 &&
					    event.xmotion.y == glConfig.vidHeight / 2) {
						mwx = glConfig.vidWidth / 2;
						mwy = glConfig.vidHeight / 2;

						Posix_QueEvent(SE_MOUSE, mx, my, 0, NULL);

						Posix_AddMousePollEvent(M_DELTAX, mx);

						if (!Posix_AddMousePollEvent(M_DELTAY, my))
							return;

						mx = my = 0;
						break;
					}

					dx = ((int) event.xmotion.x - mwx);
					dy = ((int) event.xmotion.y - mwy);
					mx += dx;
					my += dy;

					mwx = event.xmotion.x;
					mwy = event.xmotion.y;
					XWarpPointer(dpy,None,win,0,0,0,0, (glConfig.vidWidth/2),(glConfig.vidHeight/2));
				}

				break;
		}
	}*/
}

/*
=================
Sys_ShutdownInput
=================
*/
void Sys_ShutdownInput(void) { }

/*
===============
Sys_MapCharForKey
===============
*/
unsigned char Sys_MapCharForKey(int _key)
{
	return (unsigned char)_key;
}
