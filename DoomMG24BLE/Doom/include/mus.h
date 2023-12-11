/***************************************************************************//**
 * @file mus.c
 * @brief .MUS (Doom format) player for OPL2 emulator by Nicola Wrachien
 *
 * Copyright(C) 2022-2023 Nicola Wrachien (next-hack in the comments)
 * Additional Copyright(C) see below.
 *
 * This file is loosely based on Chocolate Doom i_opl_music.c and mus2mid.c.
 * We ask  generateMusicOutput() to generate into the specified buffer the
 * number of samples we need. This will parse the MUS file (based on mus2mid)
 * and generate the corresponding OPL2 register values write operations
 * (based on i_opl_music.c)
 *
 *
 * Original Copyright messages below:
// i_oplmusic.c:
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
//   System interface for music.
//
// mus2mid.c
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2006 Ben Ryves 2006
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
// mus2mid.c - Ben Ryves 2006 - http://benryves.com - benryves@benryves.com
// Use to convert a MUS file into a single track, type 0 MIDI file.
 ******************************************************************************/
#ifndef DOOM_INCLUDE_MUS_H_
#define DOOM_INCLUDE_MUS_H_

#include <stdint.h>
#include <stdbool.h>
#define MUS_RATE                    140
#define GENMIDI_HEADER_SIZE         8
typedef struct
{
    char header[4];             // should be MUS + 0x1A
    uint16_t lenSong;           // in number of bytes, from starting of file
    uint16_t offSong;           // where actually the song starts
    uint16_t primaryChannels;   // number of primary channels.
    uint16_t secondaryChannels; // number of secondary channels, which can be dropped. Not used.
    uint16_t numInstruments;    // how many instruments will be in the song.
    uint16_t reserverd;         // should be 0.
} mus_t;
typedef struct
{
    uint8_t tremolo;
    uint8_t attack;
    uint8_t sustain;
    uint8_t waveform;
    uint8_t scale;
    uint8_t level;
} __attribute__((packed)) genmidi_op_t;

typedef struct
{
    genmidi_op_t modulator;
    uint8_t feedback;
    genmidi_op_t carrier;
    uint8_t unused;
    int16_t base_note_offset;
} __attribute__((packed)) genmidi_voice_t ;

typedef struct
{
    uint16_t flags;
    uint8_t fine_tuning;
    uint8_t fixed_note;

    genmidi_voice_t voices[2];
}  __attribute__((packed)) genmidi_instr_t;
void generateMusicOutput(int32_t *destBuffer, uint32_t howManySamples);
bool loadInstrumentTable(void);
void I_OPL_SetMusicVolume(int volume);
void initMusic(void);
void OPL_InitRegisters(void);
void setMusic(uint8_t id_num);
#endif /* DOOM_INCLUDE_MUS_H_ */
