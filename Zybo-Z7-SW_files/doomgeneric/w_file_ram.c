//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"
#include "i_system.h"

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
} stdc_wad_file_t;

extern wad_file_class_t ram_wad_file;
// TODO: Extern doom_wad_file.c that contains the data from DOOM.WAD
extern unsigned char DOOM_WAD[];
extern unsigned int DOOM_WAD_len;

static wad_file_t *W_Ram_OpenFile(char *path)
{
    stdc_wad_file_t *result;

    if (strcmp(path, "WADRAM.wad") != 0) {
        printf("NOT WADRAM detected!!!!\n");
        return NULL;
    }

    result = Z_Malloc(sizeof(stdc_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &ram_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = DOOM_WAD_len;
    result->fstream = (void*)DOOM_WAD;

    return &result->wad;
}

static void W_Ram_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad;
    stdc_wad = (stdc_wad_file_t *) wad;
    Z_Free(stdc_wad);
}

// Read data from the specified position in the file into the
// provided buffer.  Returns the number of bytes read.

size_t W_Ram_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    stdc_wad_file_t *stdc_wad;
    stdc_wad = (stdc_wad_file_t *) wad;

    memcpy(buffer, ((uint8_t*)stdc_wad->fstream)+offset, buffer_len);
    return buffer_len;
}


wad_file_class_t ram_wad_file =
{
    W_Ram_OpenFile,
    W_Ram_CloseFile,
    W_Ram_Read,
};


