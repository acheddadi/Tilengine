/*
* Tilengine - The 2D retro graphics engine with raster effects
* Copyright (C) 2015-2019 Marc Palacios Domenech <mailto:megamarc@hotmail.com>
* All rights reserved
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
* */

#ifndef _ENGINE_H
#define _ENGINE_H

#define NUM_PALETTES	8
#define INTERNAL_FPS	60

#include "Tilengine.h"
#include "Sprite.h"
#include "Layer.h"
#include "Animation.h"
#include "Bitmap.h"
#include "Blitters.h"
#include "List.h"

typedef struct
{
	int32_t index;
	int16_t y1;
	int16_t y2;
	int16_t x1;
	int16_t w;
	uint8_t is_fast_normal;
	uint8_t pad;
	uint16_t flags;
	int32_t pitch;
	const uint8_t* src_row_base;
	TLN_Palette palette;
	ScanBlitPtr blitter;
	uint8_t* blend;
}
ActiveSpriteEntry;

/* motor */
typedef struct Engine
{
	uint32_t	header;			/* object signature to identify as engine context */
	uint32_t*	priority;		/* buffer receiving tiles with priority */
	uint16_t*	collision;		/* buffer with sprite coverage IDs for per-pixel collision */
	ActiveSpriteEntry* active_sprites;		/* per-frame cached active sprite list */
	uint16_t*	line_sprite_indices;		/* per-scanline sprite index buckets */
	uint16_t*	line_sprite_counts;			/* sprite count per scanline bucket */
	int			active_sprite_count;		/* number of active sprites in current frame */
	int			last_sprite_cache_line;		/* last scanline processed for sprite cache */
	bool		use_active_sprite_cache;	/* true if active sprite cache is valid */
	uint32_t*	linebuffer;		/* buffer for intermediate scanline output  */
	int			numsprites;		/* number of sprites */
	Sprite*		sprites;		/* pointer to sprite buffer */
	int			numlayers;		/* number of layers */
	Layer*		layers;			/* pointer to layer buffer */
	int			numanimations;	/* number of animations */
	Animation*	animations;		/* pointer to animation buffer */
	bool		dopriority;		/* there is some data in "priority" buffer that need blitting */
	TLN_Error	error;			/* last error code */
	TLN_LogLevel log_level;		/* logging level */

	uint32_t	bgcolor;		/* background color */
	TLN_Bitmap	bgbitmap;		/* background bitmap */
	TLN_Palette	bgpalette;		/* background bitmap palette */
	TLN_Palette palettes[NUM_PALETTES];	/* optional global palettes */
	ScanBlitPtr	blit_fast;		/* blitter for background bitmap */
	uint8_t*	blend_table;	/* current blending table */
	void		(*cb_raster)(int);	/* raster callback */
	void		(*cb_frame)(int);	/* frame callback */
	int			frame;			/* current frame number */
	int			line;			/* current scanline */
	int			target_fps;

	List list_sprites;			/* linked list active of sprites */
	List list_animations;		/* linked list active of animations */
	int sprite_mask_top;		/* top scanline for sprite masking */
	int sprite_mask_bottom;		/* bottom scanline for sprite masking */
	int xworld, yworld;			/* world coordinates with TLN_SetWorldPosition() */
	bool dirty;					/* world position updated since last draw */

	struct
	{
		int		width;
		int		height;
		int		pitch;
		uint8_t*	data;
	}
	framebuffer;
}
Engine;

extern Engine* engine;

extern void tln_trace(TLN_LogLevel log_level, const char* format, ...);

#define GetFramebufferLine(line) \
	(uint32_t*)(engine->framebuffer.data + (line*engine->framebuffer.pitch))

#endif
