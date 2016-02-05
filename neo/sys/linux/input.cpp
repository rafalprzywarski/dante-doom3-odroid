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

/*
=================
Sys_InitInput
=================
*/
void Sys_InitInput(void)
{
	common->Printf("\n------- Input Initialization -------\n");
	cmdSystem->AddCommand("in_clear", IN_Clear_f, CMD_FL_SYSTEM, "reset the input keys");
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WarpMouse((glConfig.vidWidth/2),(glConfig.vidHeight/2));
  SDL_WM_GrabInput(SDL_GRAB_ON);
  SDL_EnableUNICODE( 1 );
  SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
}

//#define XEVT_DBG
//#define XEVT_DBG2

void Sys_GrabMouseCursor(bool)
{
}

static char LookupCharacter(SDL_keysym& keysym)
{
    if (keysym.sym == SDLK_DELETE)
        return 8;

    if (!keysym.unicode || (keysym.unicode & 0xFF00))
        return 0;

    return (char)(keysym.unicode & 0xFF);
}
/**
 * Intercept a KeyRelease-KeyPress sequence and ignore
 */

				//Posix_QueEvent(SE_CHAR, buf[ 0 ], 0, 0, NULL);
				// shouldn't we be doing a release/press in this order rather?
				// ( doesn't work .. but that's what I would have expected to do though )
				//Posix_QueEvent(SE_KEY, s_scantokey[peekevent.xkey.keycode], true, 0, NULL);
				//Posix_QueEvent(SE_KEY, s_scantokey[peekevent.xkey.keycode], false, 0, NULL);

//


/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static const char *IN_TranslateSDLToQ3Key(SDL_keysym *keysym,
	int *key, bool down)
{
	static unsigned char buf[ 2 ] = { '\0', '\0' };

	*buf = '\0';
	*key = 0;

	if( keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE )
	{
		// These happen to match the ASCII chars
		*key = keysym->sym;
	}
	else
	{
		switch( keysym->sym )
		{
			case SDLK_PAGEUP:       *key = K_PGUP;          break;
			case SDLK_KP9:          *key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     *key = K_PGDN;          break;
			case SDLK_KP3:          *key = K_KP_PGDN;       break;
			case SDLK_KP7:          *key = K_KP_HOME;       break;
			case SDLK_HOME:         *key = K_HOME;          break;
			case SDLK_KP1:          *key = K_KP_END;        break;
			case SDLK_END:          *key = K_END;           break;
			case SDLK_KP4:          *key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         *key = K_LEFTARROW;     break;
			case SDLK_KP6:          *key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        *key = K_RIGHTARROW;    break;
			case SDLK_KP2:          *key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         *key = K_DOWNARROW;     break;
			case SDLK_KP8:          *key = K_KP_UPARROW;    break;
			case SDLK_UP:           *key = K_UPARROW;       break;
			case SDLK_ESCAPE:       *key = K_ESCAPE;        break;
			case SDLK_KP_ENTER:     *key = K_KP_ENTER;      break;
			case SDLK_RETURN:       *key = K_ENTER;         break;
			case SDLK_TAB:          *key = K_TAB;           break;
			case SDLK_F1:           *key = K_F1;            break;
			case SDLK_F2:           *key = K_F2;            break;
			case SDLK_F3:           *key = K_F3;            break;
			case SDLK_F4:           *key = K_F4;            break;
			case SDLK_F5:           *key = K_F5;            break;
			case SDLK_F6:           *key = K_F6;            break;
			case SDLK_F7:           *key = K_F7;            break;
			case SDLK_F8:           *key = K_F8;            break;
			case SDLK_F9:           *key = K_F9;            break;
			case SDLK_F10:          *key = K_F10;           break;
			case SDLK_F11:          *key = K_F11;           break;
			case SDLK_F12:          *key = K_F12;           break;
			case SDLK_F13:          *key = K_F13;           break;
			case SDLK_F14:          *key = K_F14;           break;
			case SDLK_F15:          *key = K_F15;           break;

			case SDLK_BACKSPACE:    *key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    *key = K_KP_DEL;        break;
			case SDLK_DELETE:       *key = K_DEL;           break;
			case SDLK_PAUSE:        *key = K_PAUSE;         break;

			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       *key = K_SHIFT;         break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:        *key = K_CTRL;          break;

			case SDLK_RMETA:
			case SDLK_LMETA:        *key = K_COMMAND;       break;

			case SDLK_RALT:
			case SDLK_LALT:         *key = K_ALT;           break;

			case SDLK_KP5:          *key = K_KP_5;          break;
			case SDLK_INSERT:       *key = K_INS;           break;
			case SDLK_KP0:          *key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  *key = K_KP_STAR;       break;
			case SDLK_KP_PLUS:      *key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     *key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    *key = K_KP_SLASH;      break;

			case SDLK_MENU:         *key = K_MENU;          break;
			case SDLK_POWER:        *key = K_POWER;         break;
			case SDLK_SCROLLOCK:    *key = K_SCROLL;     break;
			case SDLK_NUMLOCK:      *key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     *key = K_CAPSLOCK;      break;

			default:
				break;
		}
	}

	if( down && keysym->unicode && !( keysym->unicode & 0xFF00 ) )
	{
		unsigned char ch = (unsigned char)keysym->unicode & 0xFF;

		switch( ch )
		{
			case 127: // ASCII delete
				if( *key != K_DEL )
				{
					// ctrl-h
					*buf = '\b';
					break;
				}
				// fallthrough

			default: *buf = ch; break;
		}
	}

	// Keys that have ASCII names but produce no character are probably
	// dead keys -- ignore them
    int keynum = *key;
	if( down && (keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';') &&
		keysym->unicode == 0 )
	{
		*key = 0;
	}

	// Don't allow extended ASCII to generate characters
	if( *buf & 0x80 )
		*buf = '\0';

	return (char *)buf;
}


static bool keyRepeatEnabled = true;

/*
==========================
Posix_PollInput
==========================
*/
void Posix_PollInput()
{
    // from Quake3 sdl_input.c
/*
    if (keyRepeatEnabled)
    {
        SDL_EnableKeyRepeat( 0, 0 );
        keyRepeatEnabled = false;
    }
    else if( !keyRepeatEnabled )
    {
        SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
        keyRepeatEnabled = true;
    }
*/
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
    	switch(event.type)
    	{
            case SDL_KEYDOWN: {

                const char *character = NULL;
	            int key = 0;
                character = IN_TranslateSDLToQ3Key(&event.key.keysym, &key, true);

                if (key)
                    Posix_QueEvent(SE_KEY, key, true, 0, NULL);

                if (character)
                    Posix_QueEvent(SE_CHAR, *character, 0, 0, NULL);

                if (!Posix_AddKeyboardPollEvent(key, true))
                    return;

                break;
            }

            case SDL_KEYUP: {

                //if (Sys_XRepeatPress(&event)) {
            //        continue;
            //    }
                int key = 0;
                IN_TranslateSDLToQ3Key( &event.key.keysym, &key, false );

                if(key)
                    Posix_QueEvent(SE_KEY, key, false, 0, NULL);

                if (!Posix_AddKeyboardPollEvent(key, false))
                    return;

                break;
            }

            case SDL_MOUSEMOTION: {

                if (event.motion.x == (glConfig.vidWidth/2) && event.motion.y == (glConfig.vidHeight/2))
                    break;

                SDL_WarpMouse((glConfig.vidWidth/2), (glConfig.vidHeight/2));

                int dx = event.motion.xrel;
                int dy = event.motion.yrel;

                Posix_QueEvent(SE_MOUSE, dx, dy, 0, NULL);
                Posix_AddMousePollEvent(M_DELTAX, dx);
                Posix_AddMousePollEvent(M_DELTAY, dy);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == 4) {
                    Posix_QueEvent(SE_KEY, K_MWHEELUP, true, 0, NULL);

                    if (!Posix_AddMousePollEvent(M_DELTAZ, 1))
                        return;
                } else if (event.button.button == 5) {
                    Posix_QueEvent(SE_KEY, K_MWHEELDOWN, true, 0, NULL);

                    if (!Posix_AddMousePollEvent(M_DELTAZ, -1))
                        return;
                } else {
                    int b = -1;

                    if (event.button.button == 1) {
                        b = 0;		// K_MOUSE1
                    } else if (event.button.button == 2) {
                        b = 2;		// K_MOUSE3
                    } else if (event.button.button == 3) {
                        b = 1;		// K_MOUSE2
                    } else if (event.button.button == 6) {
                        b = 3;		// K_MOUSE4
                    } else if (event.button.button == 7) {
                        b = 4;		// K_MOUSE5
                    }

                    if (b == -1 || b > 4) {
                        common->DPrintf("X ButtonPress %d not supported\n", event.button.button);
                    } else {
                        Posix_QueEvent(SE_KEY, K_MOUSE1 + b, true, 0, NULL);

                        if (!Posix_AddMousePollEvent(M_ACTION1 + b, true))
                            return;
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == 4) {
                    Posix_QueEvent(SE_KEY, K_MWHEELUP, false, 0, NULL);
                } else if (event.button.button == 5) {
                    Posix_QueEvent(SE_KEY, K_MWHEELDOWN, false, 0, NULL);
                } else {
                    int b = -1;

                    if (event.button.button == 1) {
                        b = 0;
                    } else if (event.button.button == 2) {
                        b = 2;
                    } else if (event.button.button == 3) {
                        b = 1;
                    } else if (event.button.button == 6) {
                        b = 3;		// K_MOUSE4
                    } else if (event.button.button == 7) {
                        b = 4;		// K_MOUSE5
                    }

                    if (b == -1 || b > 4) {
                        common->DPrintf("X ButtonRelease %d not supported\n", event.button.button);
                    } else {
                        Posix_QueEvent(SE_KEY, K_MOUSE1 + b, false, 0, NULL);

                        if (!Posix_AddMousePollEvent(M_ACTION1 + b, false))
                            return;
                    }
                }

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
