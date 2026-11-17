/*
* Tilengine - The 2D retro graphics engine with raster effects
* Copyright (C) 2015-2019 Marc Palacios Domenech <mailto:megamarc@hotmail.com>
* All rights reserved
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
* */

#include <stdio.h>
#include <string.h>
#include "Tilengine.h"
#include "Spriteset.h"
#include "Palette.h"
#include "Bitmap.h"
#include "crc32.h"

static void compute_sprite_trim(TLN_Spriteset spriteset, int entry)
{
	SpriteEntry* dst_data = &spriteset->data[entry];
	const int w = dst_data->w;
	const int h = dst_data->h;
	const TLN_Bitmap bitmap = spriteset->bitmap;

	dst_data->solid_x1 = 0;
	dst_data->solid_y1 = 0;
	dst_data->solid_x2 = 0;
	dst_data->solid_y2 = 0;

	if (bitmap == NULL || bitmap->data == NULL || w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX)
	{
		dst_data->trim_x1 = 0;
		dst_data->trim_y1 = 0;
		dst_data->trim_x2 = 0;
		dst_data->trim_y2 = 0;
		dst_data->trimmed = false;
		return;
	}

	const uint8_t* pixels = bitmap->data + dst_data->offset;
	const int pitch = bitmap->pitch;
	int min_x = w;
	int min_y = h;
	int max_x = -1;
	int max_y = -1;

	for (int y = 0; y < h; y++)
	{
		const uint8_t* row = pixels + (y * pitch);
		int row_first = -1;
		for (int x = 0; x < w; x++)
		{
			if (row[x] != 0)
			{
				row_first = x;
				break;
			}
		}

		if (row_first != -1)
		{
			if (y < min_y) min_y = y;
			if (y > max_y) max_y = y;
			if (row_first < min_x) min_x = row_first;

			for (int x = w - 1; x >= max_x + 1 && x >= row_first; x--)
			{
				if (row[x] != 0)
				{
					max_x = x;
					break;
				}
			}
			if (max_x < row_first)
				max_x = row_first;
		}
	}

	if (max_x >= min_x && max_y >= min_y)
	{
		dst_data->trim_x1 = (int16_t)min_x;
		dst_data->trim_y1 = (int16_t)min_y;
		dst_data->trim_x2 = (int16_t)(max_x + 1);
		dst_data->trim_y2 = (int16_t)(max_y + 1);

		int best_area = 0;
		int best_x1 = 0, best_y1 = 0, best_x2 = 0, best_y2 = 0;
		int heights[512];
		int col_stack[513];

		for (int win_x = dst_data->trim_x1; win_x < dst_data->trim_x2; win_x += 512)
		{
			const int win_end = (dst_data->trim_x2 - win_x > 512) ? (win_x + 512) : dst_data->trim_x2;
			const int win_w = win_end - win_x;
			memset(heights, 0, sizeof(int) * (size_t)win_w);

			for (int y = dst_data->trim_y1; y < dst_data->trim_y2; y++)
			{
				const uint8_t* row = pixels + (y * pitch) + win_x;
				for (int i = 0; i < win_w; i++)
					heights[i] = (row[i] != 0) ? (heights[i] + 1) : 0;

				int top = 0;
				for (int i = 0; i <= win_w; i++)
				{
					const int cur_h = (i == win_w) ? 0 : heights[i];
					while (top > 0 && cur_h < heights[col_stack[top - 1]])
					{
						const int rect_h = heights[col_stack[--top]];
						const int left_i = (top == 0) ? 0 : (col_stack[top - 1] + 1);
						const int area = (i - left_i) * rect_h;
						if (area > best_area)
						{
							best_area = area;
							best_x1 = win_x + left_i;
							best_x2 = win_x + i;
							best_y2 = y + 1;
							best_y1 = best_y2 - rect_h;
						}
					}
					col_stack[top++] = i;
				}
			}
		}

		if (best_area > 0)
		{
			dst_data->solid_x1 = (int16_t)best_x1;
			dst_data->solid_y1 = (int16_t)best_y1;
			dst_data->solid_x2 = (int16_t)best_x2;
			dst_data->solid_y2 = (int16_t)best_y2;
		}
	}
	else
	{
		dst_data->trim_x1 = 0;
		dst_data->trim_y1 = 0;
		dst_data->trim_x2 = 0;
		dst_data->trim_y2 = 0;
	}
	dst_data->trimmed = true;
}

static void set_sprite_entry (TLN_Spriteset spriteset, int entry, TLN_SpriteData* data)
{
	SpriteEntry* dst_data = &spriteset->data[entry];
	dst_data->w = data->w;
	dst_data->h = data->h;
	dst_data->offset = data->y*spriteset->bitmap->pitch + data->x;
	dst_data->trim_x1 = 0;
	dst_data->trim_y1 = 0;
	dst_data->trim_x2 = (int16_t)data->w;
	dst_data->trim_y2 = (int16_t)data->h;
	dst_data->trimmed = false;
	if (data->name[0] != 0)
		dst_data->hash = _crc32(0, data->name, strlen(data->name));
	else
		dst_data->hash = 0;

	if (spriteset->bitmap != NULL && spriteset->bitmap->data != NULL)
		compute_sprite_trim(spriteset, entry);
}

/*!
 * \brief
 * Creates a new spriteset
 *
 * \param bitmap
 * Bitmap containing the sprite graphics
 *
 * \param data
 * Array of TLN_SpriteData structures with sprite descriptions
 *
 * \param num_entries
 * Number of entries in data[] array
 *
 * \returns
 * Reference to the created spriteset, or NULL if error
 *
 * \see
 * TLN_DeleteSpriteset()
 */
TLN_Spriteset TLN_CreateSpriteset (TLN_Bitmap bitmap, TLN_SpriteData* data, int num_entries)
{
	TLN_Spriteset spriteset = NULL;
	const int size = sizeof(struct Spriteset) + (sizeof(SpriteEntry) * num_entries);
	int c;

	/* crea */
	spriteset = (TLN_Spriteset)CreateBaseObject (OT_SPRITESET, size);
	if (!spriteset)
		return NULL;

	/* copia datos */
	spriteset->bitmap = bitmap;
	spriteset->palette = TLN_GetBitmapPalette (bitmap);
	spriteset->entries = num_entries;
	if (data != NULL)
	{
		for (c=0; c<num_entries; c++)
		{
			set_sprite_entry (spriteset, c, data);
			data++;
		}
	}

	TLN_SetLastError (TLN_ERR_OK);
	return spriteset;
}

/*!
 * \brief
 * Sets attributes and pixels of a given sprite inside a spriteset
 *
 * \param spriteset
 * Spriteset to set the data
 *
 * \param entry
 * The entry index inside the spriteset to modify [0, num_sprites - 1]
 *
 * \param data
 * Pointer to a user-provided TLN_SpriteData structure with sprite description
 *
 * \param pixels
 * Pointer to source pixel data
 *
 * \param pitch
 * Number of bytes per scanline of the source pixel data
 *
 * \see
 * TLN_CreateSpriteset()
 */
bool TLN_SetSpritesetData (TLN_Spriteset spriteset, int entry, TLN_SpriteData* data, void* pixels, int pitch)
{
	if (!CheckBaseObject (spriteset, OT_SPRITESET))
		return false;

	if (entry >= spriteset->entries)
	{
		TLN_SetLastError (TLN_ERR_IDX_SPRITE);
		return false;
	}

	set_sprite_entry (spriteset, entry, data);
	if (pixels != NULL && pitch != 0)
	{
		uint8_t* src = (uint8_t*)pixels;
		uint8_t* dst = TLN_GetBitmapPtr (spriteset->bitmap, data->x, data->y);
		int c;
		for (c=0; c<data->h; c++)
		{
			memcpy (dst, src, data->w);
			src += pitch;
			dst += spriteset->bitmap->pitch;
		}
		compute_sprite_trim(spriteset, entry);
	}
	TLN_SetLastError (TLN_ERR_OK);
	return true;
}

/*!
 * \brief
 * Creates a duplicate of the specified spriteset and its associated palette
 *
 * \param src
 * Spriteset to clone
 *
 * \returns
 * A reference to the newly cloned spriteset, or NULL if error
 * \see
 * TLN_LoadSpriteset()
 */
TLN_Spriteset TLN_CloneSpriteset (TLN_Spriteset src)
{
	TLN_Spriteset spriteset;

	if (!CheckBaseObject (src, OT_SPRITESET))
		return NULL;

	spriteset = (TLN_Spriteset)CloneBaseObject (src);
	if (spriteset)
	{
		TLN_SetLastError (TLN_ERR_OK);
		return spriteset;
	}
	else
		return NULL;
}

/*!
 * \brief
 * Deletes the specified spriteset and frees memory
 *
 * \param spriteset
 * Spriteset to delete
 *
 * \remarks
 * Don't delete a spriteset currently attached to a sprite!
 *
 * \see
 * TLN_LoadSpriteset(), TLN_CloneSpriteset()
 */
bool TLN_DeleteSpriteset (TLN_Spriteset spriteset)
{
	if (CheckBaseObject (spriteset, OT_SPRITESET))
	{
		if (ObjectOwner (spriteset))
			TLN_DeleteBitmap (spriteset->bitmap);
		DeleteBaseObject (spriteset);
		TLN_SetLastError (TLN_ERR_OK);
		return true;
	}
	else
		return false;
}

/*!
 * \brief
 * Query the details about the specified sprite inside a spriteset
 *
 * \param spriteset
 * Reference to the spriteset to get info about
 *
 * \param entry
 * The entry index inside the spriteset [0, num_sprites - 1]
 *
 * \param info
 * Pointer to application-allocated TLN_SpriteInfo structure that will receive the data
 *
 * \returns
 * true if success or false if error
 */
bool TLN_GetSpriteInfo (TLN_Spriteset spriteset, int entry, TLN_SpriteInfo* info)
{
	if (CheckBaseObject (spriteset, OT_SPRITESET) && info)
	{
		SpriteEntry* sprite = (SpriteEntry*)spriteset->data;
		info->w = sprite[entry].w;
		info->h = sprite[entry].h;
		TLN_SetLastError (TLN_ERR_OK);
		return true;
	}
	else
		return false;
}

/*!
 * \brief
 * Returns a reference to the palette associated to the specified spriteset
 *
 * \param spriteset
 * Spriteset to obtain the palette
 *
 * \remarks
 * The palette of a spriteset is created at load time and cannot be modified. When TLN_ConfigSprite
 * function is used to setup a sprite, the palette associated with the specified spriteset is automatically
 * assigned to that sprite, but it can be later replaced with TLN_SetSpritePalette
 *
 * \see
 * TLN_SetSpritePalette()
 */
TLN_Palette TLN_GetSpritesetPalette (TLN_Spriteset spriteset)
{
	if (CheckBaseObject (spriteset, OT_SPRITESET))
	{
		TLN_SetLastError (TLN_ERR_OK);
		return spriteset->palette;
	}
	else
		return NULL;
}

/*!
 * \brief
 * Returns a reference to the palette associated to the specified spriteset
 *
 * \param spriteset
 * Spriteset where to find the sprite
 *
 * \param name
 * Name of the sprite to findo
 *
 * \returns
 * sprite index (0 -> num_sprites - 1) if found, or -1 if not found
 */
int TLN_FindSpritesetSprite (TLN_Spriteset spriteset, const char* name)
{
	uint32_t find;
	int entry = -1;
	int c = 0;

	if (!CheckBaseObject (spriteset, OT_SPRITESET))
		return false;

	if (name == NULL)
		return false;

	/* search by name hash */
	find = _crc32(0, name, strlen(name));
	for (c=0; c < spriteset->entries; c++)
	{
		const SpriteEntry* info = &spriteset->data[c];
		if (info->hash == find)
			return c;
	}
	return -1;
}
