/*
 *  Copyright (C) 2002-2007  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: sdlmain.cpp,v 1.134 2007/08/26 18:03:25 qbix79 Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef WIN32
#include <signal.h>
#endif

#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "vga.h"
#include "keyboard.h"

#define DISABLE_JOYSTICK

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef WIN32
#include <gles/gl.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifdef WIN32
#define NVIDIA_PixelDataRange 1

#ifndef WGL_NV_allocate_memory
#define WGL_NV_allocate_memory 1
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC) (int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);
#endif

PFNWGLALLOCATEMEMORYNVPROC db_glAllocateMemoryNV = NULL;
PFNWGLFREEMEMORYNVPROC db_glFreeMemoryNV = NULL;

#else 
#undef NVIDIA_PixelDataRange

#endif

#if defined(NVIDIA_PixelDataRange)

#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1
#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
typedef void (APIENTRYP PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (APIENTRYP PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
#endif

PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV = NULL;

#endif

#endif //C_OPENGL

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if (HAVE_DDRAW_H)
#include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
//#define DEFAULT_CONFIG_FILE "/.dosboxrc"
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

enum SCREEN_TYPES	{ 
	SCREEN_SURFACE,
	SCREEN_SURFACE_DDRAW,
	SCREEN_OVERLAY,
	SCREEN_OPENGL
};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

struct SDL_Block {
	bool active;							//If this isn't set don't draw
	bool updating;
	struct {
		Bit32u width;
		Bit32u height;
		Bit32u bpp;
		Bitu flags;
		double scalex,scaley;
		GFX_CallBack_t callback;
	} draw;
	bool wait_on_error;
	struct {
		struct {
			Bit16u width, height;
			bool fixed;
		} full;
		struct {
			Bit16u width, height;
		} window;
		Bit8u bpp;
		bool fullscreen;
		bool doublebuf;
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
	} desktop;
#if C_OPENGL
	struct {
		Bitu pitch;
		void * framebuf;
		GLuint textures[48];
		GLuint displaylist;
		GLint max_texsize;
		bool bilinear;
		bool packed_pixel;
		bool paletted_texture;
#if defined(NVIDIA_PixelDataRange)
		bool pixel_data_range;
#endif
	} opengl;
#endif
	struct {
		SDL_Surface * surface;
#if (HAVE_DDRAW_H) && defined(WIN32)
		RECT rect;
#endif
	} blit;
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	SDL_cond *cond;
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		Bitu sensitivity;
	} mouse;
	SDL_Rect updateRects[1024];
	Bitu num_joysticks;
#if defined (WIN32)
	bool using_windib;
#endif
};

static SDL_Block sdl;


// by JOCO
// confgigurables
int						g_nDevSizeX	= 320;
int 					g_nDevSizeY	= 240;
int						g_nClipX, g_nClipY, g_nClipW, g_nClipH;
int*					g_lutScaleX				= NULL;
int*					g_lutScaleY				= NULL;
int*					g_lutSrcXStart			= NULL;
int*					g_lutSrcXCount			= NULL;
int*					g_lutSrcYStart			= NULL;
int*					g_lutSrcYCount			= NULL;
int						g_nIntegerScaleX		= 0;
int						g_nIntegerScaleY		= 0;
int						g_nJPageStepX			= 320;
int						g_nJPageStepY			= 240;
SDL_Surface* 			g_pJOutputScreen		= NULL;	
SDL_Surface*			g_pVirtKeyboard			= NULL;
int 					g_nJVirtualScreenMode 	= 0; 	// 0 for none, 1 for shrink, 2 for tile
int						g_nJOversizeMode		= 1; 	// 1 or 2
Bit8u 					g_nJInputOrientation	= 0; 	// !!!obsoleted!!! 0 for normal(landscape), 1 for ccw, 2 for cw  
SDL_Rect				g_rcJVirtualWindow;
bool 					g_bJHighResShell		= true;

bool					g_bRotateScreen			= false;


int texsize = 128;
int texcols = 0;
int texrows = 0;
		

// pre-mapper

#define MAXS60MAPS	32

enum s60special
	{
	VMOUSEUP	= 0,
	VMOUSEDOWN	= 1,
	VMOUSELEFT	= 2,
	VMOUSERIGHT	= 3,
	VMOUSE0		= 4,
	VMOUSE1		= 5,
	VMOUSE2		= 6,
	PANUP		= 7,
	PANDOWN		= 8,
	PANLEFT		= 9,
	PANRIGHT	= 10
	};

enum s60role
	{
	NORMAL		= 0,
	SPECIAL		= 1,
	TOGGLE		= 2,
	MODIFY		= 3
	};

class s60MapEntry
	{
public:
	SDLKey 	uDeviceSym;
	int 	x1,y1,x2,y2;
	s60role	role;
	SDLKey	uTransmitSym;
	s60MapEntry(SDLKey dc, s60role r, SDLKey tc)
		{
		x1=-1;
		uDeviceSym = dc;
		role = r;
		uTransmitSym = tc;
		}
	s60MapEntry(int _x1, int _y1, int _x2, int _y2, s60role r, SDLKey tc)
		{
		x1=_x1; y1=_y1; x2=_x2; y2=_y2;
		uDeviceSym = SDLK_UNKNOWN;
		role = r;
		uTransmitSym = tc;
		}
	};

class s60Map
	{
	friend class s60MapManager; 
public:
	bool			bVirtKeyboard;
private:
	int 			nNumberOfEntries;
	s60MapEntry** 	pEntries;
	SDL_Surface* 	pOverlay;
public:
	s60Map(int nMaxNum)
		{
			nNumberOfEntries 	= 0;
			pEntries 			= new s60MapEntry*[nMaxNum];
			pOverlay 			= NULL;
			bVirtKeyboard		= false;
		}
	~s60Map()
		{
		for(int i=0; i<nNumberOfEntries; i++)
			delete pEntries[i];
		delete[] pEntries;
		if(pOverlay) SDL_FreeSurface(pOverlay);
		}
	void SetOverlay(SDL_Surface* p)
		{
		pOverlay = p;
		}
	s60MapEntry* EntryByCode( SDLKey uCode )
		{
		for(int i=0; i<nNumberOfEntries; i++)
			{
			if(pEntries[i]->uDeviceSym==uCode) return pEntries[i];
			}
		return NULL;
		}
	s60MapEntry* EntryByClick( int x, int y )
		{
		for(int i=0; i<nNumberOfEntries; i++)
			{
			if(pEntries[i]->x1>=0)
				{
				if( (x>=pEntries[i]->x1) && (x<pEntries[i]->x2) && 
					(y>=pEntries[i]->y1) && (y<pEntries[i]->y2) )
					return pEntries[i];
				}
			}
		return NULL;
		}
	};

//forward decl
void s60HandleSpecialKey(bool bPressed, s60special action);

class s60MapManager
	{
private:
	int 	nMaps;
	int 	nCurrentMap;
	int 	nLastMap;
	SDLKey	uActiveModifierKey;
	s60Map* pMaps[MAXS60MAPS];
	SDL_Surface* prevVirtKeyboard;
public:
	s60MapManager()
		{
		nMaps = 0;
		}
	~s60MapManager()
		{
		for(int i=0;i<nMaps;i++) delete pMaps[i];
		}
	void LoadS60Maps(FILE* f)
		{
		for(int i=0;i<nMaps;i++) delete pMaps[i];
		if(g_pVirtKeyboard) 
			{
			g_pVirtKeyboard=NULL;
			GFX_ResetScreen();
			}
		char	pLine[128];
		int 	nCurrentEntry	= 0;
		nCurrentMap = -1;
		while( fgets(pLine,128,f) )
			{
			if(pLine[0]=='#') continue;
			Bitu code;
			s60role role;
			Bitu action;
			int mapid; // ignored!!!
			int maxentries;
			int x1,y1,x2,y2;
			if( sscanf(pLine,"%u %u %u",&code,&role,&action) == 3 )
				{
					if(nCurrentEntry<maxentries)
						{
						pMaps[nCurrentMap]->pEntries[nCurrentEntry] = new s60MapEntry((SDLKey)code,role,(SDLKey)action);
						nCurrentEntry++;
						}
				}
			else if( sscanf(pLine,"vk %d %d %d %d %u %u",&x1,&y1,&x2,&y2,&role,&action) == 6)
				{
					if(nCurrentEntry<maxentries)
						{
						pMaps[nCurrentMap]->pEntries[nCurrentEntry] = new s60MapEntry(x1,y1,x2,y2,role,(SDLKey)action);
						pMaps[nCurrentMap]->bVirtKeyboard = true;
						nCurrentEntry++;
						}
				
				}
			else if( sscanf(pLine,"map %d %d", &mapid, &maxentries) == 2)
				{
				if(nCurrentMap != -1) pMaps[nCurrentMap]->nNumberOfEntries = nCurrentEntry;
				nCurrentMap++; 
				pMaps[nCurrentMap] = new s60Map(maxentries);
				nCurrentEntry = 0;
				}
			else if( strstr(pLine,"vko") )
				{
				if(pMaps[nCurrentMap]->pOverlay==NULL)
					{
					SDL_Surface* bmp = SDL_LoadBMP(pLine);
					if(bmp)
						{
						SDL_SetColorKey( bmp, SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(bmp->format,0x0,0x0,0x0) );
						pMaps[nCurrentMap]->SetOverlay(bmp);
						}
					}
				}
			}
		if(nCurrentMap != -1) pMaps[nCurrentMap]->nNumberOfEntries = nCurrentEntry;
		nMaps = nCurrentMap+1;
		nCurrentMap = 0;
		if(nMaps) g_pVirtKeyboard = pMaps[0]->pOverlay;
		if(g_pVirtKeyboard) GFX_ResetScreen();
		nLastMap = -1;
		}
	bool WontHandleClick(int x, int y)
		{
		if(nMaps==0) return true;
		if(!pMaps[nCurrentMap]->bVirtKeyboard) return true;
		s60MapEntry* pEntry = pMaps[nCurrentMap]->EntryByClick(x,y);
		if(!pEntry) return true;
		return false;
		}
	/* return true if action is to be forwarded to sdl_mapper or handle_mouse*/
	bool MouseClicked(int x, int y, SDLKey& code)
		{
		code = SDLK_UNKNOWN;
		if(nMaps==0) return true;
		if(!pMaps[nCurrentMap]->bVirtKeyboard) return true;
		s60MapEntry* pEntry = pMaps[nCurrentMap]->EntryByClick(x,y);
		if(!pEntry) return true;
		switch(pEntry->role)
			{
			case NORMAL:
				code = pEntry->uTransmitSym;
				return false;
			case SPECIAL:
				s60HandleSpecialKey(true, (s60special)  (pEntry->uTransmitSym));
				return false;
			case TOGGLE:
				return false;
			case MODIFY:
				/*
				nLastMap = nCurrentMap;
				uActiveModifierKey = code;
				nCurrentMap = (unsigned)(pEntry->uTransmitSym);
				g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
				*/
				return true;
			}
		return true;
		}
	bool MouseReleased(int x, int y, SDLKey &code)
		{
		code = SDLK_UNKNOWN;
		if(nMaps==0) return true;
		if(!pMaps[nCurrentMap]->bVirtKeyboard) return true;
		/*
		if( (nLastMap>=0) && (code==uActiveModifierKey) )
			{
			nCurrentMap = nLastMap;
			g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
			nLastMap = -1;
			return false; 
			}
		*/
		s60MapEntry* pEntry = pMaps[nCurrentMap]->EntryByClick(x,y);
		if(!pEntry) return true;
		switch(pEntry->role)
			{
			case NORMAL:
				code = pEntry->uTransmitSym;
				return false;
			case SPECIAL:
				s60HandleSpecialKey(false, (s60special)  (pEntry->uTransmitSym));
				return false;
			case TOGGLE:
				nCurrentMap = (unsigned)(pEntry->uTransmitSym);
				prevVirtKeyboard = g_pVirtKeyboard;
				g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
				if( prevVirtKeyboard != g_pVirtKeyboard) GFX_ResetScreen();
				nLastMap = -1;
				return false;
			case MODIFY:
				// should not happen
				return true;
			}
		return true;
		}
	bool KeyPressed(SDLKey &code)
		{
		if(nMaps==0) return true;
		s60MapEntry* pEntry = pMaps[nCurrentMap]->EntryByCode(code);
		if(!pEntry) 
			{
			return true;
			}
		switch(pEntry->role)
			{
			case NORMAL:
				code = pEntry->uTransmitSym;
				return true;
			case SPECIAL:
				s60HandleSpecialKey(true, (s60special)  (pEntry->uTransmitSym));
				return false;
			case TOGGLE:
				return false;
			case MODIFY:
				nLastMap = nCurrentMap;
				uActiveModifierKey = code;
				nCurrentMap = (unsigned)(pEntry->uTransmitSym);
				prevVirtKeyboard = g_pVirtKeyboard;
				g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
				if( prevVirtKeyboard != g_pVirtKeyboard) GFX_ResetScreen();
				return false;
			}
		return true;
		}
	/* return true if action is to be forwarded to sdl_mapper */
	bool KeyReleased(SDLKey &code)
		{
		if(nMaps==0) return true;
		if( (nLastMap>=0) && (code==uActiveModifierKey) )
			{
			nCurrentMap = nLastMap;
			g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
			nLastMap = -1;
			return false; 
			}
		s60MapEntry* pEntry = pMaps[nCurrentMap]->EntryByCode(code);
		if(!pEntry) return true;
		switch(pEntry->role)
			{
			case NORMAL:
				code = pEntry->uTransmitSym;
				return true;
			case SPECIAL:
				s60HandleSpecialKey(false, (s60special)  (pEntry->uTransmitSym));
				return false;
			case TOGGLE:
				nCurrentMap = (unsigned)(pEntry->uTransmitSym);
				prevVirtKeyboard = g_pVirtKeyboard;
				g_pVirtKeyboard = pMaps[nCurrentMap]->pOverlay;
				if( prevVirtKeyboard != g_pVirtKeyboard) GFX_ResetScreen();
				nLastMap = -1;
			case MODIFY:
				// should not happen
				return false;
			}
		return true;
		}
	};

s60MapManager premapper;

void GFX_JPanVirtualWindow(int dir); // 0-up 1-down 2-left 3-right 
Bit8u	s60_MouseDir		= 0;

void s60HandleSpecialKey(bool bPressed, s60special action)
	{
	switch( action )
		{
		case VMOUSEUP:
			bPressed ? (s60_MouseDir |= 1) : (s60_MouseDir &= (0xff-1));
			break;
		case VMOUSEDOWN:
			bPressed ? (s60_MouseDir |= 2) : (s60_MouseDir &= (0xff-2));
			break;
		case VMOUSELEFT:
			bPressed ? (s60_MouseDir |= 4) : (s60_MouseDir &= (0xff-4));
			break;
		case VMOUSERIGHT:
			bPressed ? (s60_MouseDir |= 8) : (s60_MouseDir &= (0xff-8));
			break;
		case VMOUSE0:
			bPressed ? Mouse_ButtonPressed(0) : Mouse_ButtonReleased(0); 
			break;
		case VMOUSE1:
			bPressed ? Mouse_ButtonPressed(1) : Mouse_ButtonReleased(1);
			break;
		case VMOUSE2:
			bPressed ? Mouse_ButtonPressed(2) : Mouse_ButtonReleased(2);
			break;
		case PANUP:
			if(bPressed) GFX_JPanVirtualWindow(0);
			break;
		case PANDOWN:
			if(bPressed) GFX_JPanVirtualWindow(1);
			break;
		case PANLEFT:
			if(bPressed) GFX_JPanVirtualWindow(2);
			break;
		case PANRIGHT:
			if(bPressed) GFX_JPanVirtualWindow(3);
			break;
		default:
			break;
		}
	}

/* called periodically to move the virtual pointer if keys are down */
#define MOUSERATE	20
void S60_MouseMove(Bit32u ticks)
{
	static Bit32u lastMouseTick = 0;
	if(s60_MouseDir==0) return;
	if( ticks-lastMouseTick < MOUSERATE ) return;
	lastMouseTick = ticks;
	float dx=0;
	float dy=0;
	if( s60_MouseDir & 1 ) dy-= 1.0f;
	if( s60_MouseDir & 2 ) dy+= 1.0f;
	if( s60_MouseDir & 4 ) dx-= 1.0f;
	if( s60_MouseDir & 8 ) dx+= 1.0f;
	dx *= sdl.mouse.sensitivity/100.0f;
	dy *= sdl.mouse.sensitivity/100.0f;
	Mouse_CursorMoved(dx,dy,0,0,true);
}

#if 0
// virtual mouse and window panning
SDLKey	S60_MouseU			= SDLK_UP;
SDLKey	S60_MouseD			= SDLK_DOWN;
SDLKey	S60_MouseL			= SDLK_LEFT;
SDLKey	S60_MouseR			= SDLK_RIGHT;
SDLKey	S60_Mouse0			= SDLK_RETURN;
SDLKey 	S60_Mouse0alt		= SDLK_ASTERISK;
SDLKey	S60_Mouse1			= SDLK_HASH;
SDLKey	S60_Mouse2			= SDLK_0;
SDLKey	S60_MouseToggle		= SDLK_HOME;
Bit8u	S60_MouseDir		= 0;
bool	S60_MouseMode		= false;
Bit8u	S60_MouseOrient		= 0;
bool 	S60_MoveToPage    	= false;
bool 	S60_DidSomePaging  	= false;

bool S60_Mouse(SDL_Event * event, Bitu &corrected_arrow_key)
{
	if(event->type == SDL_KEYDOWN)
	{
		if( event->key.keysym.sym == S60_MouseToggle )
			{
			S60_MoveToPage 	= true;
			S60_DidSomePaging 	= false;
			return true;
			}
		if( event->key.keysym.sym == S60_MouseU )
		{
			if(S60_MoveToPage)
				{
				S60_DidSomePaging = true;
				GFX_JPanVirtualWindow(0);
				return true;
				}
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_UP; return false;}
			S60_MouseDir |= 1; return true;
		}
		if( event->key.keysym.sym == S60_MouseD )
		{
			if(S60_MoveToPage)
				{
				S60_DidSomePaging = true;
				GFX_JPanVirtualWindow(1);
				return true;
				}
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_DOWN; return false;}
			S60_MouseDir |= 2; return true;
		}
		if( event->key.keysym.sym == S60_MouseL )
		{
			if(S60_MoveToPage)
				{
				S60_DidSomePaging = true;
				GFX_JPanVirtualWindow(2);
				return true;
				}
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_LEFT; return false;}
			S60_MouseDir |= 4; return true;
		}
		if( event->key.keysym.sym == S60_MouseR )
		{
			if(S60_MoveToPage)
				{
				S60_DidSomePaging = true;
				GFX_JPanVirtualWindow(3);
				return true;
				}
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_RIGHT; return false;}
			S60_MouseDir |= 8; return true;
		}
		if(!S60_MouseMode) return false;
		if( event->key.keysym.sym == S60_Mouse0 )
		{ 
			Mouse_ButtonPressed(0); return true;
		}
		if( event->key.keysym.sym == S60_Mouse0alt )
		{ 
			Mouse_ButtonPressed(0); return true;
		}
		if( event->key.keysym.sym == S60_Mouse1 )
		{ 
			Mouse_ButtonPressed(1); return true;
		}
		if( event->key.keysym.sym == S60_Mouse2 )
		{ 
			Mouse_ButtonPressed(2); return true;
		}	
	}
	if(event->type == SDL_KEYUP)
	{
		if( event->key.keysym.sym == S60_MouseToggle )
		{ 
			S60_MoveToPage = false;
			if(S60_DidSomePaging) 
				{		
				return true;
				}
			S60_MouseDir = 0;
			S60_MouseMode = !S60_MouseMode; 
			return true;
		}
		if( event->key.keysym.sym == S60_MouseU )
		{
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_UP; return false;}
			S60_MouseDir &= (0xff-1); return true;
		}
		if( event->key.keysym.sym == S60_MouseD )
		{
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_DOWN; return false;}
			S60_MouseDir &= (0xff-2); return true;
		}
		if( event->key.keysym.sym == S60_MouseL )
		{
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_LEFT; return false;}
			S60_MouseDir &= (0xff-4); return true;
		}
		if( event->key.keysym.sym == S60_MouseR )
		{
			if(!S60_MouseMode) {corrected_arrow_key=SDLK_RIGHT; return false;}
			S60_MouseDir &= (0xff-8); return true;
		}
		if(!S60_MouseMode) return false;
		if( event->key.keysym.sym == S60_Mouse0 )
		{ 
			Mouse_ButtonReleased(0); return true;
		}
		if( event->key.keysym.sym == S60_Mouse0alt )
		{ 
			Mouse_ButtonReleased(0); return true;
		}
		if( event->key.keysym.sym == S60_Mouse1 )
		{ 
			Mouse_ButtonReleased(1); return true;
		}
		if( event->key.keysym.sym == S60_Mouse2 )
		{ 
			Mouse_ButtonReleased(2); return true;
		}
	}
	return false;
}
#endif //0
// end JOCO




extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;
void GFX_SetTitle(Bit32s cycles,Bits frameskip,bool paused){
	char title[200]={0};
	static Bit32s internal_cycles=0;
	static Bits internal_frameskip=0;
	if(cycles != -1) internal_cycles = cycles;
	if(frameskip != -1) internal_frameskip = frameskip;
	if(CPU_CycleAutoAdjust) {
		if (internal_cycles>=100)
			sprintf(title,"DOSBox %s, Cpu Cycles:      max, Frameskip %2d, Program: %8s",VERSION,internal_frameskip,RunningProgram);
		else
			sprintf(title,"DOSBox %s, Cpu Cycles:   [%3d%%], Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	} else {
		sprintf(title,"DOSBox %s, Cpu Cycles: %8d, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	}

	if(paused) strcat(title," PAUSED");
	SDL_WM_SetCaption(title,VERSION);
}

static void PauseDOSBox(bool pressed) {
	if (!pressed)
		return;
	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}
	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {
		case SDL_QUIT: throw(0); break;
		case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
		case SDL_KEYUP:
			if(event.key.keysym.sym==SDLK_PAUSE){
				paused=false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
		}
	}
}

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void) {
	return sdl.using_windib;
}
#endif

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) {
	Bitu testbpp,gotbpp;
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
check_surface:
		/* Check if we can satisfy the depth it loves */
		if (flags & GFX_LOVE_8) testbpp=8;
		else if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
check_gotbpp:
		if (sdl.desktop.fullscreen) gotbpp=SDL_VideoModeOK(640,480,testbpp,SDL_FULLSCREEN|SDL_HWSURFACE|SDL_HWPALETTE);
		else gotbpp=sdl.desktop.bpp;
		/* If we can't get our favorite mode check for another working one */
		switch (gotbpp) {
		case 8:
			if (flags & GFX_CAN_8) flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (flags & GFX_CAN_15) flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (flags & GFX_CAN_16) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (flags & GFX_CAN_32) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
		}
		flags |= GFX_CAN_RANDOM;
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (!(flags&(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32))) goto check_surface;
		if (flags & GFX_LOVE_15) testbpp=15;
		else if (flags & GFX_LOVE_16) testbpp=16;
		else if (flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
		flags|=GFX_SCALING;
		goto check_gotbpp;
#endif
	case SCREEN_OVERLAY:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#endif
	}
	return flags;
}


void GFX_ResetScreen(void) {
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
}

static int int_log2 (int val) {
    int log = 0;
    while ((val >>= 1) != 0)
	log++;
    return log;
}


static SDL_Surface * GFX_SetupSurfaceScaled(Bit32u sdl_flags, Bit32u bpp) {
	Bit16u fixedWidth;
	Bit16u fixedHeight;

	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
		sdl_flags |= SDL_FULLSCREEN|SDL_HWSURFACE;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
		sdl_flags |= SDL_HWSURFACE;
	}
	if (fixedWidth && fixedHeight) {
		double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
		double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
		if ( ratio_w < ratio_h) {
			sdl.clip.w=fixedWidth;
			sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*ratio_w);
		} else {
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h);
			sdl.clip.h=(Bit16u)fixedHeight;
		}
		if (sdl.desktop.fullscreen) 
			sdl.surface = SDL_SetVideoMode(fixedWidth,fixedHeight,bpp,sdl_flags);
		else
			sdl.surface = SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		if (sdl.surface && sdl.surface->flags & SDL_FULLSCREEN) {
			sdl.clip.x=(Sint16)((sdl.surface->w-sdl.clip.w)/2);
			sdl.clip.y=(Sint16)((sdl.surface->h-sdl.clip.h)/2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}
		return sdl.surface;
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
		sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		return sdl.surface;
	}
}

#define max(a,b) ((a>b)?a:b)

#ifdef C_OPENGL
GLfloat gl_vertices[48*4*2];
GLfloat gl_texcoords[4*2] = 
{
		0.004f, 0.004f,		// to center texels, offset by ( 0.5 * 1/texsize )
		0.996f, 0.004f,
		0.996f, 0.996f,
		0.004f, 0.996f
};
GLfloat gl_coords[4*2] = 
{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
};
#endif

#ifdef WIN32
int s60scale = 0;
#define TInt int
#else
extern TInt s60scale;
#endif

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback) {
	if (sdl.updating) 
		GFX_EndUpdate( 0 );

	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.callback=callback;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	Bitu bpp=0;
	Bitu retFlags = 0;
	
	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
dosurface:
		if (flags & GFX_CAN_8) bpp=8;
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.w=width;
		sdl.clip.h=height;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.full.fixed) {
				sdl.clip.x=(Sint16)((sdl.desktop.full.width-width)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.full.height-height)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.full.width,sdl.desktop.full.height,bpp,
					SDL_FULLSCREEN | ((flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) | 
					(sdl.desktop.doublebuf ? SDL_DOUBLEBUF|SDL_ASYNCBLIT : 0) | SDL_HWPALETTE);
				if (sdl.surface == NULL) E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",sdl.desktop.full.width,sdl.desktop.full.height,bpp,SDL_GetError());
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.surface=SDL_SetVideoMode(width,height,bpp,
					SDL_FULLSCREEN | ((flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE) | 
					(sdl.desktop.doublebuf ? SDL_DOUBLEBUF|SDL_ASYNCBLIT  : 0)|SDL_HWPALETTE);
				if (sdl.surface == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",width,height,bpp,SDL_GetError());
			}
		} else {
			sdl.clip.x=0; 		sdl.clip.y=0;
			sdl.clip.w=width; 	sdl.clip.h=height;
// by JOCO
			/*
			if(g_pVirtKeyboard) 
				{
				SDL_FreeSurface(g_pVirtKeyboard);
				g_pVirtKeyboard=NULL;
				}
			g_pVirtKeyboard = SDL_LoadBMP("c:/Data/bitmap.bmp");
			SDL_SetColorKey(g_pVirtKeyboard,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(g_pVirtKeyboard->format,0x0,0x0,0x0));
			*/
			bool bBigScreen = false;
			SDL_FreeSurface(sdl.surface);
			if(g_pJOutputScreen)
				{
				SDL_FreeSurface(g_pJOutputScreen);
				g_pJOutputScreen = NULL;
				}
			if(g_lutScaleX)
				{
				delete[] g_lutScaleX; g_lutScaleX = NULL;
				delete[] g_lutScaleY;
				delete[] g_lutSrcXStart;
				delete[] g_lutSrcXCount;
				delete[] g_lutSrcYStart;
				delete[] g_lutSrcYCount;
				}
			if( (height>g_nDevSizeY) || (width>g_nDevSizeX) )
				{
				bBigScreen = true;
				if( (height/2>g_nDevSizeY) || (width/2>g_nDevSizeX) )
					E_Exit("Screen size too big!");
				}
			if( ((s60scale==0) && bBigScreen) || 
				(s60scale>2) || 
				g_bRotateScreen )
				{
				g_pJOutputScreen = SDL_SetVideoMode(
						((s60scale==1)||(s60scale==2)) ? width : g_nDevSizeX,
						((s60scale==1)||(s60scale==2)) ? height : g_nDevSizeY,
						bpp,
						//SDL_HWPALETTE | SDL_SWSURFACE | SDL_FULLSCREEN | SDL_RESIZABLE 
#ifdef WIN32
						SDL_SWSURFACE | SDL_RESIZABLE
#else
						SDL_SWSURFACE | SDL_RESIZABLE
#endif					
						//(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE
						);
				sdl.surface = SDL_CreateRGBSurface(
						SDL_SWSURFACE,
						//(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE,
						//SDL_SWSURFACE,
						width, height,
						g_pJOutputScreen->format->BitsPerPixel,
						g_pJOutputScreen->format->Rmask,
						g_pJOutputScreen->format->Gmask,
						g_pJOutputScreen->format->Bmask,
						0
						);
				if((s60scale==0) && bBigScreen )
					{
					g_rcJVirtualWindow.x = 0;
					g_rcJVirtualWindow.y = 0;
					g_rcJVirtualWindow.w = g_nDevSizeX;
					g_rcJVirtualWindow.h = g_nDevSizeY;
					g_nJVirtualScreenMode = g_nJOversizeMode;
					}
				else
					g_nJVirtualScreenMode = 0;
				}
			else
				{
				sdl.surface = SDL_SetVideoMode(
					(s60scale==0) ? g_nDevSizeX : width,
					(s60scale==0) ? g_nDevSizeY : height,
					bpp,
					//SDL_HWPALETTE | SDL_SWSURFACE | SDL_FULLSCREEN | SDL_RESIZABLE 
#ifdef WIN32
					SDL_SWSURFACE | SDL_RESIZABLE
#else
					SDL_SWSURFACE | SDL_RESIZABLE
#endif
					//(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE
					);
				g_nJVirtualScreenMode = 0;
				}
			switch( s60scale )
				{
				case 4:
					{
					float h,w,x,y;
					h=w=2.0f; x=y=-1.0f;
					float bufaspect = (float)width/(float)height;
					float scraspect = (float)g_nDevSizeX/(float)g_nDevSizeY;
					if( bufaspect>scraspect ) 
						{
						h = scraspect/bufaspect*2.0f;
						y = -1.0f + (1.0f - h/2.0f);
						}
					else
						{
						w = bufaspect/scraspect*2.0f;
						x = -1.0f + (1.0f - w/2.0f);
						}
					g_nClipX = (int) ((x+1.0f)/2.0f*(float)g_nDevSizeX);
					g_nClipY = (int) ((y+1.0f)/2.0f*(float)g_nDevSizeY);
					g_nClipW = (int) (w/2.0f*(float)g_nDevSizeX);
					g_nClipH = (int) (h/2.0f*(float)g_nDevSizeY);
					}
					break;
				case 3:
					g_nClipX = g_nClipY = 0;
					g_nClipW = g_nDevSizeX;
					g_nClipH = g_nDevSizeY;
					break;
				case 0:
				case 1:
				case 2:
				default:
					g_nClipX = g_nClipY = 0;
					g_nClipW = width;
					g_nClipH = height;
					break;
				}
			if( (s60scale==4) || (s60scale==3) )
				{
				g_lutScaleX = new int[g_nClipW];
				g_lutScaleY = new int[g_nClipH];
				g_lutSrcXStart = new int[width];
				g_lutSrcXCount = new int[width];
				g_lutSrcYStart = new int[height];
				g_lutSrcYCount = new int[height];
				for(int x=0; x<g_nClipW; x++)
					g_lutScaleX[x] = (int) ( (float)x / (float)g_nClipW * (float)width  );
				for(int x=0; x<width; x++)
					{
					g_lutSrcXStart[x] = -1;
					g_lutSrcXCount[x] = 0;
					int i=0;
					while(i<g_nClipW)
						{
						if(g_lutScaleX[i]==x)
							{
							g_lutSrcXStart[x] = i;
							while( (g_lutScaleX[i]==x) && (i<g_nClipW) )
								{
								g_lutSrcXCount[x]++;
								i++;
								}
							break;
							}
						i++;
						}
					}
				for(int y=0; y<g_nClipH; y++)
					g_lutScaleY[y] = (int) ( (float)y / (float)g_nClipH * (float)height );
				for(int y=0; y<height; y++)
					{
					g_lutSrcYStart[y] = -1;
					g_lutSrcYCount[y] = 0;
					int i=0;
					while(i<g_nClipH)
						{
						if(g_lutScaleY[i]==y)
							{
							g_lutSrcYStart[y] = i;
							while( (g_lutScaleY[i]==y) && (i<g_nClipH) )
								{
								g_lutSrcYCount[y]++;
								i++;
								}
							break;
							}
						i++;
						}
					}
				}
					
			//sdl.surface=SDL_SetVideoMode(width,height,bpp,(flags & GFX_CAN_RANDOM) ? SDL_SWSURFACE : SDL_HWSURFACE);
// end JOCO
			
#ifdef WIN32
			if (sdl.surface == NULL) {
				SDL_QuitSubSystem(SDL_INIT_VIDEO);
				if (!sdl.using_windib) {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with windib enabled.");
					putenv("SDL_VIDEODRIVER=windib");
					sdl.using_windib=true;
				} else {
					LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with directx enabled.");
					putenv("SDL_VIDEODRIVER=directx");
					sdl.using_windib=false;
				}
				SDL_InitSubSystem(SDL_INIT_VIDEO);
				sdl.surface = SDL_SetVideoMode(width,height,bpp,SDL_HWSURFACE);
			}
#endif
			if (sdl.surface == NULL) 
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",width,height,bpp,SDL_GetError());
		}
		if (sdl.surface) {
			switch (sdl.surface->format->BitsPerPixel) {
			case 8:
				retFlags = GFX_CAN_8;
                break;
			case 15:
				retFlags = GFX_CAN_15;
				break;
			case 16:
				retFlags = GFX_CAN_16;
                break;
			case 32:
				retFlags = GFX_CAN_32;
                break;
			}
			if (retFlags && (sdl.surface->flags & SDL_HWSURFACE))
				retFlags |= GFX_HARDWARE;
			if (retFlags && (sdl.surface->flags & SDL_DOUBLEBUF)) {
				sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,
					sdl.draw.width, sdl.draw.height,
					sdl.surface->format->BitsPerPixel,
					sdl.surface->format->Rmask,
					sdl.surface->format->Gmask,
					sdl.surface->format->Bmask,
				0);
				/* If this one fails be ready for some flickering... */
			}
		}
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (flags & GFX_CAN_15) bpp=15;
		if (flags & GFX_CAN_16) bpp=16;
		if (flags & GFX_CAN_32) bpp=32;
		if (!GFX_SetupSurfaceScaled((sdl.desktop.doublebuf && sdl.desktop.fullscreen) ? SDL_DOUBLEBUF : 0,bpp)) goto dosurface;
		sdl.blit.rect.top=sdl.clip.y;
		sdl.blit.rect.left=sdl.clip.x;
		sdl.blit.rect.right=sdl.clip.x+sdl.clip.w;
		sdl.blit.rect.bottom=sdl.clip.y+sdl.clip.h;
		sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,sdl.draw.width,sdl.draw.height,
				sdl.surface->format->BitsPerPixel,
				sdl.surface->format->Rmask,
				sdl.surface->format->Gmask,
				sdl.surface->format->Bmask,
				0);
		if (!sdl.blit.surface || (!sdl.blit.surface->flags&SDL_HWSURFACE)) {
			if (sdl.blit.surface) {
				SDL_FreeSurface(sdl.blit.surface);
				sdl.blit.surface=0;
			}
			LOG_MSG("Failed to create ddraw surface, back to normal surface.");
			goto dosurface;
		}
		switch (sdl.surface->format->BitsPerPixel) {
		case 15:
			retFlags = GFX_CAN_15 | GFX_SCALING | GFX_HARDWARE;
			break;
		case 16:
			retFlags = GFX_CAN_16 | GFX_SCALING | GFX_HARDWARE;
               break;
		case 32:
			retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
               break;
		}
		sdl.desktop.type=SCREEN_SURFACE_DDRAW;
		break;
#endif
	case SCREEN_OVERLAY:
		if (sdl.overlay) {
			SDL_FreeYUVOverlay(sdl.overlay);
			sdl.overlay=0;
		}
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		if (!GFX_SetupSurfaceScaled(0,0)) goto dosurface;
		sdl.overlay=SDL_CreateYUVOverlay(width*2,height,SDL_UYVY_OVERLAY,sdl.surface);
		if (!sdl.overlay) {
			LOG_MSG("SDL:Failed to create overlay, switching back to surface");
			goto dosurface;
		}
		sdl.desktop.type=SCREEN_OVERLAY;
		retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
	{
		if (sdl.opengl.framebuf) {
#if defined(NVIDIA_PixelDataRange)
			if (sdl.opengl.pixel_data_range) db_glFreeMemoryNV(sdl.opengl.framebuf);
			else
#endif
			free(sdl.opengl.framebuf);
		}
		sdl.opengl.framebuf=0;
		/*
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface;
		int texsize = 2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		*/
		
				
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
#if defined (WIN32) && SDL_VERSION_ATLEAST(1, 2, 11)
		SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
#endif
		/*
		GFX_SetupSurfaceScaled(SDL_OPENGL,0);
		if (!sdl.surface || sdl.surface->format->BitsPerPixel<15) {
			LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}
		*/
		/* Create the texture and display list */
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range) {
			sdl.opengl.framebuf=db_glAllocateMemoryNV(width*height*4,0.0,1.0,1.0);
			glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,width*height*4,sdl.opengl.framebuf);
			glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
		}
#endif 
			
		glViewport(0,0,g_nDevSizeX,g_nDevSizeY);
		glMatrixMode (GL_PROJECTION);
		if(texcols*texrows>0)
			glDeleteTextures(texcols*texrows,sdl.opengl.textures);
		
		texcols = width/texsize;
		texrows = height/texsize;
		if(texcols*texsize<width) 	texcols++;
		if(texrows*texsize<height) 	texrows++;
		
		sdl.opengl.framebuf=malloc(texcols*texsize*texrows*texsize*4);		//32 bit color
		sdl.opengl.pitch=texcols*texsize*4;
				
 		glGenTextures(texcols*texrows,sdl.opengl.textures);
 		for(int cTex=0; cTex<texcols*texrows; cTex++)
 			{
 			glBindTexture(GL_TEXTURE_2D,sdl.opengl.textures[cTex]);
 			// No borders
 			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 			if (1)//sdl.opengl.bilinear) 
 				{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 				} 
 			else 
				{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				}
 			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texsize, texsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
 			}
		glClearColor (0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapBuffers();
		glClear(GL_COLOR_BUFFER_BIT);
		glShadeModel (GL_FLAT); 
		glDisable (GL_DEPTH_TEST);
		glDisable (GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();

		glEnableClientState( GL_VERTEX_ARRAY        );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		
		float glx,gly,glw,glh;
		glx = gly = -1.0f;
		glw = glh = 2.0f;
		g_nClipX = g_nClipY = 0;
		g_nClipW = g_nDevSizeX;
		g_nClipH = g_nDevSizeY;
		if( s60scale == 6 )		// glaspect
			{
			float bufaspect = (float)width/(float)height;
			float scraspect = (float)g_nDevSizeX/(float)g_nDevSizeY;
			if( bufaspect>scraspect ) 
				{
				glh = scraspect/bufaspect*2.0f;
				gly = -1.0f + (1.0f - glh/2.0f);
				}
			else
				{
				glw = bufaspect/scraspect*2.0f;
				glx = -1.0f + (1.0f - glw/2.0f);
				}
			g_nClipX = (int) ((glx+1.0f)/2.0f*(float)g_nDevSizeX);
			g_nClipY = (int) ((gly+1.0f)/2.0f*(float)g_nDevSizeY);
			g_nClipW = (int) (glw/2.0f*(float)g_nDevSizeX);
			g_nClipH = (int) (glh/2.0f*(float)g_nDevSizeY);
			}
		int cTile = 0;
		float tilew = glw / texcols;
		float tileh = glh / texrows;
		tilew *= (float)(texcols*texsize)/(float)width;
		tileh *= (float)(texrows*texsize)/(float)height;	
		float offx = glx;
		float offy = gly;
		for(int cRow=0; cRow<texcols; cRow++)
			{
			offx = glx;
			for(int cCol=0; cCol<texcols; cCol++)
				{
				float* coords = & ( gl_vertices[cTile*4*2] );
				for(int cVert = 0; cVert<4; cVert++)
					{
					coords[cVert*2+0] = (gl_coords[cVert*2+0] * tilew + offx);
					coords[cVert*2+1] = (gl_coords[cVert*2+1] * tileh + offy) * (-1.0f);
					if( g_bRotateScreen )
						{
						coords[cVert*2+0] *= -1.0f;
						coords[cVert*2+1] *= -1.0f;
						}
					}
				offx += tilew;
				cTile++;
				}
			offy += tileh;
			}
		
/*
		if (glIsList(sdl.opengl.displaylist)) glDeleteLists(sdl.opengl.displaylist, 1);
		sdl.opengl.displaylist = glGenLists(1);
		glNewList(sdl.opengl.displaylist, GL_COMPILE);
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		glBegin(GL_QUADS);
		// lower left
		glTexCoord2f(0,tex_height); glVertex2f(-1.0f,-1.0f);
		// lower right
		glTexCoord2f(tex_width,tex_height); glVertex2f(1.0f, -1.0f);
		// upper right
		glTexCoord2f(tex_width,0); glVertex2f(1.0f, 1.0f);
		// upper left
		glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
		glEnd();
		glEndList();
*/
		sdl.desktop.type=SCREEN_OPENGL;
		retFlags = GFX_CAN_32 | GFX_SCALING;
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range)
			retFlags |= GFX_HARDWARE;
#endif
	break;
		}//OPENGL
#endif	//C_OPENGL
	}//CASE
	if (retFlags) 
		GFX_Start();
	if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
	return retFlags;
}

void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

void GFX_SwitchFullScreen(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if (!sdl.mouse.locked) GFX_CaptureMouse();
	} else {
		if (sdl.mouse.locked) GFX_CaptureMouse();
	}
	GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed) {
	if (!pressed)
		return;
	GFX_SwitchFullScreen();
}


bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
	if (!sdl.active || sdl.updating) 
		return false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (sdl.blit.surface) {
			if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
				return false;
			pixels=(Bit8u *)sdl.blit.surface->pixels;
			pitch=sdl.blit.surface->pitch;
		} else {
			if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
				return false;
			pixels=(Bit8u *)sdl.surface->pixels;
			pixels+=sdl.clip.y*sdl.surface->pitch;
			pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
			pitch=sdl.surface->pitch;
		}
		sdl.updating=true;
		return true;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		if (SDL_LockSurface(sdl.blit.surface)) {
//			LOG_MSG("SDL Lock failed");
			return false;
		}
		pixels=(Bit8u *)sdl.blit.surface->pixels;
		pitch=sdl.blit.surface->pitch;
		sdl.updating=true;
		return true;
#endif
	case SCREEN_OVERLAY:
		SDL_LockYUVOverlay(sdl.overlay);
		pixels=(Bit8u *)*(sdl.overlay->pixels);
		pitch=*(sdl.overlay->pitches);
		sdl.updating=true;
		return true;
#if C_OPENGL
	case SCREEN_OPENGL:
		pixels=(Bit8u *)sdl.opengl.framebuf;
		pitch=sdl.opengl.pitch;
		sdl.updating=true;
		return true;
#endif
	}
	return false;
}

#define PANSTEPX g_nJPageStepX
#define PANSTEPY g_nJPageStepY

// by JOCO

void GFX_JPanVirtualWindow(int dir) // 0-up 1-down 2-left 3-right 
	{
	if(g_nJVirtualScreenMode != 2) return;
	switch(dir)
		{
		case 0: g_rcJVirtualWindow.y -= PANSTEPY; break;
		case 1: g_rcJVirtualWindow.y += PANSTEPY; break;
		case 2: g_rcJVirtualWindow.x -= PANSTEPX; break;
		case 3: g_rcJVirtualWindow.x += PANSTEPX; break;
		break;
		}
	if(g_rcJVirtualWindow.x<0) g_rcJVirtualWindow.x = 0;
	if(g_rcJVirtualWindow.y<0) g_rcJVirtualWindow.y = 0;
	if(g_rcJVirtualWindow.x+g_rcJVirtualWindow.w >= sdl.surface->w ) g_rcJVirtualWindow.x = sdl.surface->w - 1 - g_rcJVirtualWindow.w;
	if(g_rcJVirtualWindow.y+g_rcJVirtualWindow.h >= sdl.surface->h ) g_rcJVirtualWindow.y = sdl.surface->h - 1 - g_rcJVirtualWindow.h;
	
	if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))	
		return;
	if (SDL_MUSTLOCK(g_pJOutputScreen) && SDL_LockSurface(g_pJOutputScreen))
		return;	
	SDL_BlitSurface(sdl.surface, &g_rcJVirtualWindow, g_pJOutputScreen, NULL);
	if (SDL_MUSTLOCK(sdl.surface)) 		SDL_UnlockSurface(sdl.surface);
	if (SDL_MUSTLOCK(g_pJOutputScreen)) SDL_UnlockSurface(g_pJOutputScreen);
	SDL_Flip( g_pJOutputScreen );
	}


#define tex(u,v) (GLbyte)( (u) - 128 ) , (GLbyte)( (v) - 128 )

Bit8u tempbuff[128*128*4];



void GFX_EndUpdate( const Bit16u *changedLines ) {
	if (!sdl.updating) 
		return;
	sdl.updating=false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (SDL_MUSTLOCK(sdl.surface)) {
			if (sdl.blit.surface) {
				SDL_UnlockSurface(sdl.blit.surface);
				int Blit = SDL_BlitSurface( sdl.blit.surface, 0, sdl.surface, &sdl.clip );
				LOG(LOG_MISC,LOG_WARN)("BlitSurface returned %d",Blit);
			} else {
				SDL_UnlockSurface(sdl.surface);
			}
			SDL_Flip(sdl.surface);
		} else if (changedLines) {
			Bitu y = 0, index = 0, rectCount = 0;
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					SDL_Rect *rect = &sdl.updateRects[rectCount++];
					rect->x = sdl.clip.x;
					rect->y = sdl.clip.y + y;
					rect->w = (Bit16u)sdl.draw.width;
					rect->h = changedLines[index];
#if 0
					if (rect->h + rect->y > sdl.surface->h) {
						LOG_MSG("WTF");
					}
#endif
					y += changedLines[index];
				}
				index++;
			}
			if (rectCount)
				{
// by JOCO		
				if( g_pJOutputScreen == NULL )
					{
					if(g_pVirtKeyboard)
						SDL_BlitSurface(g_pVirtKeyboard,NULL,sdl.surface,NULL);
					}
				SDL_UpdateRects( sdl.surface, rectCount, sdl.updateRects );
				if( (g_bRotateScreen)           ||
					(g_nJVirtualScreenMode > 0) ||
					(s60scale>2) )
					{
					if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
						return;
					if (SDL_MUSTLOCK(g_pJOutputScreen) && SDL_LockSurface(g_pJOutputScreen))
						return;
					Bit8u * srcpixel = (Bit8u*)sdl.surface->pixels;
					int srcBpp = sdl.surface->format->BytesPerPixel;
					int srcpitch = sdl.surface->pitch;
					Bit8u * dstpixel = (Bit8u*)g_pJOutputScreen->pixels;
					int dstBpp = g_pJOutputScreen->format->BytesPerPixel;
					int dstpitch = g_pJOutputScreen->pitch;
					int factor = (g_nJVirtualScreenMode==1) ? 2 : 1;
					int screenx = g_pJOutputScreen->w;
					int screeny = g_pJOutputScreen->h;
					unsigned x,y;
					int srcx,srcy,srcw,srch,dstx,dsty,dstw,dsth;
					for(int i=0;i<rectCount;i++)
						{
						srcx = sdl.updateRects[i].x;
						srcy = sdl.updateRects[i].y;
						srcw = sdl.updateRects[i].w;
						srch = sdl.updateRects[i].h;
						if( (s60scale==3) || (s60scale==4) )
							{
							while( (g_lutSrcXCount[srcx]<1) && (srcw>0) ) { srcx++; srcw--; } 
							while( (g_lutSrcYCount[srcy]<1) && (srch>0) ) { srcy++; srch--; }
							dstx = g_nClipX + g_lutSrcXStart[srcx];
							dsty = g_nClipY + g_lutSrcYStart[srcy];
							while( (g_lutSrcXCount[srcx+srcw-1]<1) && (srcw>0) ) { srcw--; }
							while( (g_lutSrcYCount[srcy+srch-1]<1) && (srch>0) ) { srch--; }
							dstw = (g_nClipX + g_lutSrcXStart[srcx+srcw-1] + g_lutSrcXCount[srcx+srcw-1]) - dstx;							
							dsth = (g_nClipY + g_lutSrcYStart[srcy+srch-1] + g_lutSrcYCount[srcy+srch-1]) - dsty;							
							Bit8u* srcline;
							Bit8u* dstline;
							if( g_bRotateScreen )
								{
								srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
								dstline = dstpixel + (screeny-1-dsty)*dstpitch + (screenx-1-dstx)*dstBpp;
								}
							else
								{
								srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
								dstline = dstpixel + dsty*dstpitch + dstx*dstBpp;
								}
							for(y=0; y<srch; y++)
								{
								int j = g_lutSrcYCount[srcy+y];
								if(j>0)
									{
									Bit8u* dstscan = dstline;
									Bit8u* srcscan = srcline;
									for(x=0; x<srcw; x++)
										{
										int i = g_lutSrcXCount[srcx+x];
										while(i>0)
											{
											memcpy(dstscan,srcscan,dstBpp);
											if( g_bRotateScreen )
												dstscan -= dstBpp;
											else
												dstscan += dstBpp;
											i--;
											}
										srcscan += srcBpp;
										}
									if( g_bRotateScreen )
										dstline -= dstpitch;
									else
										dstline += dstpitch;
									j--;
									}
								while(j>0)
									{
									if( g_bRotateScreen )
										{
										memcpy(dstline-(dstw-1)*dstBpp,dstline+dstpitch-(dstw-1)*dstBpp,dstBpp*dstw);
										dstline-=dstpitch;
										}
									else
										{
										memcpy(dstline,dstline-dstpitch,dstBpp*dstw);
										dstline+=dstpitch;
										}
									j--;
									}
								srcline += srcpitch;
								}
							if( g_bRotateScreen )
								{
								dstx = screenx-dstx-dstw;
								dsty = screeny-dsty-dsth;
								}
							}
						else
							{
							if(factor>1)
								{
								if(srcx%2) {srcx++;srcw--;}
								if(srcy%2) {srcy++;srch--;}
								if((srcx+srcw)%2) srcw++;
								if((srcy+srch)%2) srch++;
								}
							dstx = srcx/factor; dsty = srcy/factor; dstw = srcw/factor; dsth = srch/factor;
							if(g_nJVirtualScreenMode==2)
								{
								dstx-=g_rcJVirtualWindow.x;
								dsty-=g_rcJVirtualWindow.y;
								if( (dstx >= screenx) ||
									(dsty >= screeny) ||
									(dstx+dstw < 0) ||
									(dsty+dsth < 0) )
									{
									sdl.updateRects[i].x=0;
									sdl.updateRects[i].y=0;
									sdl.updateRects[i].w=0;
									sdl.updateRects[i].h=0;
									continue;
									}
								if (dstx < 0) { dstw -= (0-dstx); srcw -= (0-dstx); srcx += (0-dstx); dstx = 0; }
								if (dsty < 0) { dsth -= (0-dsty); srch -= (0-dsty); srcy += (0-dsty); dsty = 0; }
								if (dstx+dstw > screenx-1) { dstw = screenx-1-dstx; srcw = screenx-1-dstx; } 
								if (dsty+dsth > screeny-1) { dsth = screeny-1-dsty; srch = screeny-1-dsty; } 					
								}
							if(factor==1)
								{
								if( g_bRotateScreen )
									{
									Bit8u* srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
									Bit8u* dstline = dstpixel + (screeny-1-dsty)*dstpitch + (screenx-1-dstx)*dstBpp;
									for(y=0; y<dsth; y++)
										{
										Bit8u* dstscan = dstline;
										Bit8u* srcscan = srcline;
										for(x=0; x<dstw; x++)
											{
											memcpy(dstscan,srcscan,dstBpp);
											dstscan -= dstBpp;
											srcscan += srcBpp;
											}
										srcline += srcpitch;
										dstline -= dstpitch;
										}
									dstx = screenx-dstx-dstw;
									dsty = screeny-dsty-dsth;
									}
								else
									{
									Bit8u* srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
									Bit8u* dstline = dstpixel + dsty*dstpitch + dstx*dstBpp;
									for(y=0; y<dsth; y++)
										{
										memcpy(dstline,srcline,dstw*dstBpp);
										srcline += srcpitch;
										dstline += dstpitch;
										}
									}
								}
							else
								{
								Bit8u* srcline;
								Bit8u* dstline;
								if( g_bRotateScreen )
									{
									srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
									dstline = dstpixel + (screeny-1-dsty)*dstpitch + (screenx-1-dstx)*dstBpp;
									}
								else
									{
									srcline = srcpixel + srcy*srcpitch + srcx*srcBpp;
									dstline = dstpixel + dsty*dstpitch + dstx*dstBpp;
									}
								for(y=0; y<dsth; y++)
									{
									Bit8u* dstscan = dstline;
									Bit8u* srcscan = srcline;
									for(x=0; x<dstw; x++)
										{
										memcpy(dstscan,srcscan,dstBpp);
										if( g_bRotateScreen )
											dstscan -= dstBpp;
										else
											dstscan += dstBpp;
										srcscan += factor*srcBpp;
										}
									srcline += factor*srcpitch;
									if( g_bRotateScreen )
										dstline -= dstpitch;
									else
										dstline += dstpitch;
									}
								if( g_bRotateScreen )
									{
									dstx = screenx-dstx-dstw;
									dsty = screeny-dsty-dsth;
									}
								}
							}
						sdl.updateRects[i].x=dstx;
						sdl.updateRects[i].y=dsty;
						sdl.updateRects[i].w=dstw;
						sdl.updateRects[i].h=dsth;
						}
					if (SDL_MUSTLOCK(sdl.surface)) 		SDL_UnlockSurface(sdl.surface);
					if (SDL_MUSTLOCK(g_pJOutputScreen)) SDL_UnlockSurface(g_pJOutputScreen);
					if(g_pVirtKeyboard)
						SDL_BlitSurface(g_pVirtKeyboard, NULL, g_pJOutputScreen, NULL);
					SDL_UpdateRects( g_pJOutputScreen,  rectCount, sdl.updateRects );
					}
				}
		}
// end JOCO		
		break;
#if (HAVE_DDRAW_H) && defined(WIN32)
	case SCREEN_SURFACE_DDRAW:
		SDL_UnlockSurface(sdl.blit.surface);
		ret=IDirectDrawSurface3_Blt(
			sdl.surface->hwdata->dd_writebuf,&sdl.blit.rect,
			sdl.blit.surface->hwdata->dd_surface,0,
			DDBLT_WAIT, NULL);
		switch (ret) {
		case DD_OK:
			break;
		case DDERR_SURFACELOST:
			IDirectDrawSurface3_Restore(sdl.blit.surface->hwdata->dd_surface);
			IDirectDrawSurface3_Restore(sdl.surface->hwdata->dd_surface);
			break;
		default:
			LOG_MSG("DDRAW:Failed to blit, error %X",ret);
		}
		SDL_Flip(sdl.surface);
		break;
#endif
	case SCREEN_OVERLAY:
		SDL_UnlockYUVOverlay(sdl.overlay);
		SDL_DisplayYUVOverlay(sdl.overlay,&sdl.clip);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range) {
            glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
					sdl.draw.width, sdl.draw.height, GL_BGRA_EXT,
					GL_UNSIGNED_INT_8_8_8_8_REV, sdl.opengl.framebuf);
			glCallList(sdl.opengl.displaylist);
			SDL_GL_SwapBuffers();
		} else
#endif
		if (changedLines) {
#if 0
#if 0
			int cTile = 0;
			float dB = 255.0f / ((float)texrows*texcols);
			for(int cRow=0; cRow<texrows; cRow++)
				{
				for(int cCol=0; cCol<texcols; cCol++)
					{
					glBindTexture(GL_TEXTURE_2D, sdl.opengl.textures[cTile]);
					unsigned idx = 0;
					for(int y=0; y<128; y++)
						{
						for(int x=0; x<128; x++)
							{
							tempbuff[idx+0] = x*2;
							tempbuff[idx+1] = y*2;
							tempbuff[idx+2] = (int)(dB*cTile);
							idx+=4;
							}
						}
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
					    		128, 128, GL_RGBA,
					    		GL_UNSIGNED_BYTE, tempbuff );
					cTile++;
					}
				}
#else
			int cTile = 0;
			for(int cRow=0; cRow<texrows; cRow++)
				{
				for(int cCol=0; cCol<texcols; cCol++)
					{
					glBindTexture(GL_TEXTURE_2D, sdl.opengl.textures[cTile]);
					unsigned idx = 0;
					Bit8u* startpix = (Bit8u *)sdl.opengl.framebuf + cRow*texsize*sdl.opengl.pitch + cCol*texsize*4;
					for(int y=0; y<texsize; y++)
						{
						Bit8u* startpix = (Bit8u *)sdl.opengl.framebuf + (cRow*texsize+y)*sdl.opengl.pitch + cCol*texsize*4;
						memcpy( &(tempbuff[y*texsize*4]), startpix, texsize*4 );
						startpix += (texcols*texsize*4);
						}
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
							texsize, texsize, GL_RGBA,
							GL_UNSIGNED_BYTE, tempbuff );
					cTile++;
					}
				}
#endif
#else
			Bitu y = 0, index = 0;
            while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					Bitu height = changedLines[index];
					int start	= y; 
					int end		= start+height;
					while(start<end)
						{
						int startrow	= start / texsize;
						int endrow		= end   / texsize;
						int offsety		= start % texsize;
						int subheight	= (endrow>startrow) ? (texsize-offsety) : (end-start);
						int sy;
						for(int cCol=0; cCol<texcols; cCol++)
							{
							glBindTexture(GL_TEXTURE_2D, sdl.opengl.textures[startrow*texcols+cCol]);
							Bit8u* startpix = (Bit8u *)sdl.opengl.framebuf + 
											  (startrow*texsize+offsety)*sdl.opengl.pitch + 
											  cCol*texsize*4 ;
							for(sy=0;sy<subheight;sy++)
								{
								memcpy( &(tempbuff[sy*texsize*4]), startpix, texsize*4 );
								startpix += (texcols*texsize*4);
								}
							glTexSubImage2D(GL_TEXTURE_2D, 0, 
											0, offsety, 
											texsize, sy, 
											GL_RGBA,
											GL_UNSIGNED_BYTE, tempbuff );
							}
						start += sy;
						}
					y += height;
				}
				index++;
			}
#endif // 0
			// do actual gl drawing here
			//glCallList(sdl.opengl.displaylist);
            glMatrixMode( GL_TEXTURE );
            glLoadIdentity();
                
            int cTex = 0;
            for(int cRow=0; cRow<texrows; cRow++)
            	{
            	for(int cCol=0; cCol<texcols; cCol++)
            		{
            		GLfloat* verts = &(gl_vertices[cTex*4*2]); 
            		glBindTexture(GL_TEXTURE_2D, sdl.opengl.textures[cTex]);
            		glVertexPointer(   2, GL_FLOAT, 0, verts	);
            		glTexCoordPointer( 2, GL_FLOAT, 0, gl_texcoords	);
            		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
            		cTex++;
            		}
            	}
            SDL_GL_SwapBuffers();
		}
		break;
#endif

	}
}

void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
	/* I should probably not change the GFX_PalEntry :) */
	SDL_Surface* surface = g_pJOutputScreen ? g_pJOutputScreen : sdl.surface;
	if (surface->flags & SDL_HWPALETTE) {
		if (!SDL_SetPalette(surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) 
			{
			// by JOCO
			// epoc sdl is stupid and returns zero on success, which is not what's expected here 
			// so just don't exit
			//E_Exit("SDL:Can't set palette");
			}
	} else {
		if (!SDL_SetPalette(surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
	case SCREEN_SURFACE_DDRAW:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_OVERLAY:
		{
			Bit8u y =  ( 9797*(red) + 19237*(green) +  3734*(blue) ) >> 15;
			Bit8u u =  (18492*((blue)-(y)) >> 15) + 128;
			Bit8u v =  (23372*((red)-(y)) >> 15) + 128;
#ifdef WORDS_BIGENDIAN
			return (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
			return (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
		}
	case SCREEN_OPENGL:
		// USE RGBA
		return ((red << 0) | (green << 8) | (blue << 16)) | (255 << 24);
		//USE BGRA
		//return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	}
	return 0;
}

void GFX_Stop() {
	if (sdl.updating) 
		GFX_EndUpdate( 0 );
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

static void GUI_ShutDown(Section * sec) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}

static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}

static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest 
// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus ) && 
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
		break;
#endif
	default:
		break;
	}
}

static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;

#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
#if WORDS_BIGENDIAN
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif
	SDL_WM_SetIcon(logos,NULL);
#endif

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.wait_on_error=section->Get_bool("waitonerror");
	const char * priority=section->Get_string("priority");
	if (priority && priority[0]) {
		Bitu next;
		if (!strncasecmp(priority,"lowest",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_LOWEST;next=6;
		} else if (!strncasecmp(priority,"lower",5)) {
			sdl.priority.focus=PRIORITY_LEVEL_LOWER;next=5;
		} else if (!strncasecmp(priority,"normal",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_NORMAL;next=6;
		} else if (!strncasecmp(priority,"higher",6)) {
			sdl.priority.focus=PRIORITY_LEVEL_HIGHER;next=6;
		} else if (!strncasecmp(priority,"highest",7)) {
			sdl.priority.focus=PRIORITY_LEVEL_HIGHEST;next=7;
		} else {
			next=0;sdl.priority.focus=PRIORITY_LEVEL_HIGHER;
		}
		priority=&priority[next];
		if (next && priority[0]==',' && priority[1]) {
			priority++;
			if (!strncasecmp(priority,"lowest",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;
			} else if (!strncasecmp(priority,"lower",5)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;
			} else if (!strncasecmp(priority,"normal",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
			} else if (!strncasecmp(priority,"higher",6)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;
			} else if (!strncasecmp(priority,"highest",7)) {
				sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST;
			} else if (!strncasecmp(priority,"pause",5)) {
				/* we only check for pause here, because it makes no sense
				 * for DOSBox to be paused while it has focus
				 */
				sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
			} else {
				sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
			}
		} else sdl.priority.nofocus=sdl.priority.focus;
	} else {
		sdl.priority.focus=PRIORITY_LEVEL_HIGHER;
		sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;
	}
	SetPriority(sdl.priority.focus); //Assume focus on startup
	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;
	sdl.desktop.full.fixed=false;
	const char* fullresolution=section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0;
	sdl.desktop.full.height = 0;
	if(fullresolution && *fullresolution) {
		char res[100];
		strncpy( res, fullresolution, sizeof( res ));
		fullresolution = lowcase (res);//so x and X are allowed
		if(strcmp(fullresolution,"original")) {
			sdl.desktop.full.fixed = true;
			char* height = const_cast<char*>(strchr(fullresolution,'x'));
			if(height && * height) {
				*height = 0;
				sdl.desktop.full.height = atoi(height+1);
				sdl.desktop.full.width  = atoi(res);
			}
		}
	}

	sdl.desktop.window.width  = 0;
	sdl.desktop.window.height = 0;
	const char* windowresolution=section->Get_string("windowresolution");
	if(windowresolution && *windowresolution) {
		char res[100];
		strncpy( res,windowresolution, sizeof( res ));
		windowresolution = lowcase (res);//so x and X are allowed
		if(strcmp(windowresolution,"original")) {
			char* height = const_cast<char*>(strchr(windowresolution,'x'));
			if(height && *height) {
				*height = 0;
				sdl.desktop.window.height = atoi(height+1);
				sdl.desktop.window.width  = atoi(res);
			}
		}
	}
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");
	if (!sdl.desktop.full.width) {
#ifdef WIN32
		sdl.desktop.full.width=GetSystemMetrics(SM_CXSCREEN);
#else	
		sdl.desktop.full.width=1024;
#endif
	}
	if (!sdl.desktop.full.height) {
#ifdef WIN32
		sdl.desktop.full.height=GetSystemMetrics(SM_CYSCREEN);
#else	
		sdl.desktop.full.height=768;
#endif
	}
	sdl.mouse.autoenable=section->Get_bool("autolock");
	if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock=false;
	sdl.mouse.sensitivity=section->Get_int("sensitivity");
	// by JOCO
	// output is either sdl surface or opengl
	// depending on the value of s60scale
	sdl.desktop.want_type=SCREEN_SURFACE;
	if( s60scale>4 )
		sdl.desktop.want_type=SCREEN_OPENGL;
#if 0	// by JOCO
	const char * output=section->Get_string("output");
	if (!strcasecmp(output,"surface")) {
		sdl.desktop.want_type=SCREEN_SURFACE;
#if (HAVE_DDRAW_H) && defined(WIN32)
	} else if (!strcasecmp(output,"ddraw")) {
		sdl.desktop.want_type=SCREEN_SURFACE_DDRAW;
#endif
	} else if (!strcasecmp(output,"overlay")) {
		sdl.desktop.want_type=SCREEN_OVERLAY;
#if C_OPENGL
	} else if (!strcasecmp(output,"opengl")) {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=true;
	} else if (!strcasecmp(output,"openglnb")) {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=false;
#endif
	} else {
		LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output);
		sdl.desktop.want_type=SCREEN_SURFACE;
	}
#endif // 0 by JOCO 
	sdl.overlay=0;
	
	g_nDevSizeX=section->Get_int("devicescreenwidth");
	g_nDevSizeY=section->Get_int("devicescreenheight");
	g_bRotateScreen=section->Get_bool("rotatescreen");	
	
#if C_OPENGL
   if(sdl.desktop.want_type==SCREEN_OPENGL){ /* OPENGL is requested */
	sdl.surface=SDL_SetVideoMode(g_nDevSizeX,g_nDevSizeY,0,SDL_OPENGL);
	if (sdl.surface == NULL) {
		LOG_MSG("Could not initialize OpenGL, switching back to surface");
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else {
	sdl.opengl.framebuf=0;
	for(int i=0;i<48;i++)sdl.opengl.textures[i]=0;
	sdl.opengl.displaylist=0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
	
#if defined(WIN32) && defined(NVIDIA_PixelDataRange)
	glPixelDataRangeNV = (PFNGLPIXELDATARANGENVPROC) wglGetProcAddress("glPixelDataRangeNV");
	db_glAllocateMemoryNV = (PFNWGLALLOCATEMEMORYNVPROC) wglGetProcAddress("wglAllocateMemoryNV");
	db_glFreeMemoryNV = (PFNWGLFREEMEMORYNVPROC) wglGetProcAddress("wglFreeMemoryNV");
#endif

	const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
	if(gl_ext && *gl_ext){
		sdl.opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") > 0);
		sdl.opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") > 0);
#if defined(NVIDIA_PixelDataRange)
		sdl.opengl.pixel_data_range=(strstr(gl_ext,"GL_NV_pixel_data_range") >0 ) &&
			glPixelDataRangeNV && db_glAllocateMemoryNV && db_glFreeMemoryNV;
		sdl.opengl.pixel_data_range = 0;					
#endif
    	} else {
		sdl.opengl.packed_pixel=sdl.opengl.paletted_texture=false;
	}
	}
	} /* OPENGL is requested end */
   
#endif	//OPENGL
	/* Initialize screen for first time */
   
// By JOCO 
	/*
	sdl.surface=SDL_SetVideoMode(640,400,0,0);
	if (sdl.surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
	sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
	}*/
	const char* orientation=section->Get_string("orientation");
	g_nJInputOrientation = 0;
	//if (!strcasecmp(orientation,"ccw")) g_nJInputOrientation = 1;
	//else if (!strcasecmp(orientation,"cw")) g_nJInputOrientation = 2;
	g_nJPageStepX=section->Get_int("pagestepx");
	g_nJPageStepY=section->Get_int("pagestepy");
	const char* oversize=section->Get_string("oversize");
	if (!strcasecmp(oversize,"shrink")) 	g_nJOversizeMode 	= 1;
	else if (!strcasecmp(oversize,"page")) 	g_nJOversizeMode 	= 2;

	const char* shellres=section->Get_string("shellres");
	if (!strcasecmp(shellres,"high")) 		g_bJHighResShell 	= true;
	else if (!strcasecmp(shellres,"low")) 	g_bJHighResShell 	= false;

	const char* premapfilename = section->Get_string("premapperfile");
	if(premapfilename)
		{
		FILE* f = fopen(premapfilename,"rt");
		if(f)
			{
			premapper.LoadS60Maps(f);
			fclose(f);
			}
		}
	
	/*if(g_nJInputOrientation>0)
	{
		output_surface = SDL_SetVideoMode(g_nDevSizeY,g_nDevSizeX,32, SDL_SWSURFACE | SDL_RESIZABLE); 
	} 
	else
	{
		output_surface = SDL_SetVideoMode(g_nDevSizeX,g_nDevSizeY,32, SDL_SWSURFACE | SDL_RESIZABLE); 
		//output_surface = SDL_SetVideoMode(g_nDevSizeY,g_nDevSizeX,0,0);
	}
	if (output_surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
	*/
//end JOCO
	GFX_Stop();
/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(PauseDOSBox,MK_pause,MMOD2,"pause","Pause");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDLMod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
	else {
		SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
		sdl.mouse.requestlock=false;
	}
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
#if 0
if (sdl.mouse.locked || !sdl.mouse.autoenable) 
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(float)(sdl.clip.w-1)*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(float)(sdl.clip.h-1)*sdl.mouse.sensitivity/100.0f,
						  sdl.mouse.locked);
#else
	float relx,rely,absx,absy;
	if( g_bRotateScreen )
		{
		motion->xrel *= -1;
		motion->yrel *= -1;
		if( (s60scale==0) || (s60scale>2) )
			{
				motion->x = g_nDevSizeX - motion->x;
				motion->y = g_nDevSizeY - motion->y;
			}
		else
			{
				motion->x = g_nClipW - motion->x;
				motion->y = g_nClipH - motion->y;
			}
		}
	relx = (float)motion->xrel;
	rely = (float)motion->yrel;
	absx = (float)(motion->x-g_nClipX)/(float)(g_nClipW-1);
	absy = (float)(motion->y-g_nClipY)/(float)(g_nClipH-1);
	Mouse_CursorMoved( relx, rely, absx, absy, false );
#endif
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
			break;
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

static Bit8u laltstate = SDL_KEYUP;
static Bit8u raltstate = SDL_KEYUP;

void MAPPER_CheckEvent(SDL_Event * event);

void GFX_Events() {
	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay=0;
	int time=GetTicks();
	if (time-poll_delay>20) {
		poll_delay=time;
		//if (sdl.num_joysticks>0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (event.active.gain) {
					if (sdl.desktop.fullscreen && !sdl.mouse.locked)
						GFX_CaptureMouse();
					SetPriority(sdl.priority.focus);
				} else {
					if (sdl.mouse.locked) {
#ifdef WIN32
						if (sdl.desktop.fullscreen) {
							VGA_KillDrawing();
							sdl.desktop.fullscreen=false;
							GFX_ResetScreen();
						}
#endif
						GFX_CaptureMouse();
					}
					SetPriority(sdl.priority.nofocus);
					MAPPER_LosingFocus();
				}
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt-Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
					KEYBOARD_ClrBuffer();
					SDL_Delay(500);
					while (SDL_PollEvent(&ev)) {
						// flush event queue.
					}

					while (paused) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event. 
						case SDL_ACTIVEEVENT:     // wait until we get window focus back
							if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
								// We've got focus back, so unpause and break out of the loop
								if (ev.active.gain) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
							}
							break;
						}
					}
				}
			}
			break;
		case SDL_MOUSEMOTION:
			if( premapper.WontHandleClick( event.motion.x, event.motion.y ) ) 
				HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
			{
			SDL_Event fake;
			SDLKey key;
			if( premapper.MouseClicked( event.button.x, event.button.y, key) ) 
				{
				HandleMouseButton(&event.button);
				break;
				}
			fake.type = SDL_KEYDOWN;
			fake.key.type = SDL_KEYDOWN;
			fake.key.state = SDL_PRESSED;
			fake.key.keysym.sym = key;
			MAPPER_CheckEvent(&fake);
			break;
			}
		case SDL_MOUSEBUTTONUP:
			{
			SDL_Event fake;
			SDLKey key;
			if( premapper.MouseReleased( event.button.x, event.button.y, key) ) 
				{
				HandleMouseButton(&event.button);
				break;
				}
			fake.type = SDL_KEYUP;
			fake.key.type = SDL_KEYUP;
			fake.key.state = SDL_RELEASED;
			fake.key.keysym.sym = key;
			MAPPER_CheckEvent(&fake);
			break;
			}
		case SDL_VIDEORESIZE:
//			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		case SDL_VIDEOEXPOSE:
			if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
			break;
#ifdef WIN32
			/*
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym==SDLK_LALT) laltstate = event.key.type;
			if (event.key.keysym.sym==SDLK_RALT) raltstate = event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) &&
				((laltstate==SDL_KEYDOWN) || (raltstate==SDL_KEYDOWN))) break;
*/
#endif
		case SDL_KEYDOWN:
			if(premapper.KeyPressed(event.key.keysym.sym)) MAPPER_CheckEvent(&event);
			break;
		case SDL_KEYUP:
			if(premapper.KeyReleased(event.key.keysym.sym)) MAPPER_CheckEvent(&event);
			break;		
		default:
			MAPPER_CheckEvent(&event);
		}
	}
}

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif


/* static variable to show wether there is not a valid stdout.
 * Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;

void GFX_ShowMsg(char const* format,...) {
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
        strcat(buf,"\n");
	va_end(msg);
	// by JOCO if(!no_stdout) printf(buf);       
};

TInt GetS60ScaleOption()
	{
	//look for config file in /Data of all drives starting with c
	char cfg_search_path[128];
	char drive;
	for(drive='c';drive<'z';drive++)
	{
		sprintf(cfg_search_path,"%c:/data/%s",drive,DEFAULT_CONFIG_FILE);
		FILE* test_cfg_path = fopen(cfg_search_path,"rt");
		if(test_cfg_path)
			{
			char line[512]; 
			while( fgets(line,512,test_cfg_path) )
				{
				if(strstr(line,"s60scale=s60full"))
					{
					fclose(test_cfg_path);
					return 1;
					}
				if(strstr(line,"s60scale=s60aspect"))
					{
					fclose(test_cfg_path);
					return 2;
					}
				if(strstr(line,"s60scale=swfull"))
					{
					fclose(test_cfg_path);
					return 3;
					}
				if(strstr(line,"s60scale=swaspect"))
					{
					fclose(test_cfg_path);
					return 4;
					}
				if(strstr(line,"s60scale=glfull"))
					{
					fclose(test_cfg_path);
					return 5;
					}
				if(strstr(line,"s60scale=glaspect"))
					{
					fclose(test_cfg_path);
					return 6;
					}
				}
			fclose(test_cfg_path);
			break;
			}
		}
	return 0;
	}

int main(int argc, char* argv[]) {
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			if(freopen(STDOUT_FILE, "w", stdout) == NULL)
				no_stdout = true; // No stdout so don't write messages
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
			setbuf(stderr, NULL);					/* No buffering */
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","r",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif  //defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-version") || 
		    control->cmdline->FindExist("--version") ) {
			printf("\nDOSBox version %s, copyright 2002-2007 DOSBox Team.\n\n",VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return 0;
		}

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

#ifdef OS2
        PPIB pib;
        PTIB tib;
        DosGetInfoBlocks(&tib, &pib);
        if (pib->pib_ultype == 2) pib->pib_ultype = 3;
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
#endif

	/* Display Welcometext in the console */
	LOG_MSG("DOSBox version %s",VERSION);
	LOG_MSG("Copyright 2002-2007 DOSBox Team, published under GNU GPL.");
	LOG_MSG("---");

	/* Init SDL */
	if ( SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER
		|SDL_INIT_NOPARACHUTE
		) < 0 ) E_Exit("Can't init SDL %s",SDL_GetError());

#ifndef DISABLE_JOYSTICK
	//Initialise Joystick seperately. This way we can warn when it fails instead
	//of exiting the application
	if( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) LOG_MSG("Failed to init joystick support");
#endif

#if defined (WIN32)
#if SDL_VERSION_ATLEAST(1, 2, 10)
		sdl.using_windib=true;
#else
		sdl.using_windib=false;
#endif
		char sdl_drv_name[128];
		if (getenv("SDL_VIDEODRIVER")==NULL) {
			if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
				sdl.using_windib=false;
				if (strcmp(sdl_drv_name,"directx")!=0) {
					SDL_QuitSubSystem(SDL_INIT_VIDEO);
					putenv("SDL_VIDEODRIVER=directx");
					if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) {
						putenv("SDL_VIDEODRIVER=windib");
						if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
						sdl.using_windib=true;
					}
				}
			}
		} else {
			char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
			if (strcmp(sdl_videodrv,"directx")==0) sdl.using_windib = false;
			else if (strcmp(sdl_videodrv,"windib")==0) sdl.using_windib = true;
		}
		if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
			if (strcmp(sdl_drv_name,"windib")==0) LOG_MSG("SDL_Init: Starting up with SDL windib video driver.\n          Try to update your video card and directx drivers!");
		}
#endif
		sdl.num_joysticks=0;// by JOCO SDL_NumJoysticks();
		Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
		sdl_sec->AddInitFunction(&MAPPER_StartUp);
		sdl_sec->Add_bool("fullscreen",false);
		sdl_sec->Add_bool("fulldouble",false);
		sdl_sec->Add_string("fullresolution","original");
		sdl_sec->Add_string("orientation","normal");
		sdl_sec->Add_string("oversize","shrink");
		sdl_sec->Add_string("shellres","high");
		sdl_sec->Add_string("windowresolution","original");
		sdl_sec->Add_string("output","surface");
		sdl_sec->Add_bool("autolock",true);
		sdl_sec->Add_int("sensitivity",100);
		sdl_sec->Add_int("devicescreenwidth",320);
		sdl_sec->Add_int("devicescreenheight",240);
		sdl_sec->Add_bool("rotatescreen",false);
		sdl_sec->Add_int("pagestepx",320);
		sdl_sec->Add_int("pagestepy",240);
		sdl_sec->Add_bool("waitonerror",true);
		sdl_sec->Add_string("priority","higher,normal");
		sdl_sec->Add_string("mapperfile","mapper.txt");
		sdl_sec->Add_string("premapperfile","s60mapper.txt");
		sdl_sec->Add_bool("usescancodes",false /* by JOCO true*/);

		MSG_Add("SDL_CONFIGFILE_HELP",
			"fullscreen -- Start dosbox directly in fullscreen.\n"
			"fulldouble -- Use double buffering in fullscreen.\n"
			"fullresolution -- What resolution to use for fullscreen: original or fixed size (e.g. 1024x768).\n"
			"windowresolution -- Scale the window to this size IF the output device supports hardware scaling.\n"
			"output -- What to use for output: surface,overlay"
#if C_OPENGL
			",opengl,openglnb"
#endif
#if (HAVE_DDRAW_H) && defined(WIN32)
			",ddraw"
#endif
			".\n"
			"autolock -- Mouse will automatically lock, if you click on the screen.\n"
			"sensitiviy -- Mouse sensitivity.\n"
			"waitonerror -- Wait before closing the console if dosbox has an error.\n"
			"priority -- Priority levels for dosbox: lowest,lower,normal,higher,highest,pause (when not focussed).\n"
			"            Second entry behind the comma is for when dosbox is not focused/minimized.\n"
			"mapperfile -- File used to load/save the key/event mappings from.\n"
			"usescancodes -- Avoid usage of symkeys, might not work on all operating systems.\n"
			);
		/* Init all the dosbox subsystems */
		DOSBOX_Init();
		std::string config_file;
		bool parsed_anyconfigfile = false;
		// by JOCO
		//look for config file in /Data of all drives starting with c
		char cfg_search_path[128];
		char drive;
		for(drive='c';drive<'z';drive++)
		{
			sprintf(cfg_search_path,"%c:/data/%s",drive,DEFAULT_CONFIG_FILE);
			FILE* test_cfg_path = fopen(cfg_search_path,"rt");
			if(test_cfg_path)
			{
				fclose(test_cfg_path);
				break;
			}
		}
		if(drive<'z')
		{
			config_file = (std::string)cfg_search_path;
			if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		}
		// First parse the configfile in the $HOME directory
		/*
		if ((getenv("HOME") != NULL)) {
			config_file = (std::string)getenv("HOME") + 
				      (std::string)DEFAULT_CONFIG_FILE;
			if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		}
		*/
		// Add extra settings from dosbox.conf in the local directory if there is no configfile specified at the commandline
		if (!control->cmdline->FindString("-conf",config_file,true)) config_file="dosbox.conf";
		if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		// Add extra settings from additional configfiles at the commandline
		while(control->cmdline->FindString("-conf",config_file,true))
			if (control->ParseConfigFile(config_file.c_str())) parsed_anyconfigfile = true;
		// Give a message if no configfile whatsoever was found.
		if(!parsed_anyconfigfile) LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
	
#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not allready in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_Run(true);

		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif 
		}

	}
	catch (int){ 
		;//nothing pressed killswitch
	}
	catch(...){
		throw;//dunno what happened. rethrow for sdl to catch 
	}
	SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
	return 0;
};


int LoadPremapperFile(const char* premapfilename)
	{
	FILE* f = fopen(premapfilename,"rt");
	if(f)
		{
		premapper.LoadS60Maps(f);
		fclose(f);
		return 1;
		}
	return 0;
	}
