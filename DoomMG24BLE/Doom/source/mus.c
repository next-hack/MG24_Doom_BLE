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

#include "mus.h"
#include "opl2.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "w_wad.h"
#include "global_data.h"
#include "extMemory.h"


#define LOOKING_FOR_PARAMS 0
#if LOOKING_FOR_PARAMS
#define dbgprintf printf
#else
#define dbgprintf(...)
#endif

#define tstprintf(...)
#define DEBUG 0

#define MUS_PERCUSSION_CHAN         15
#define OPL_NUM_VOICES              9
#define MAX_MUS_CHANNELS            16

#define GENMIDI_NUM_INSTRS          128
#define GENMIDI_NUM_PERCUSSION      47

#define GENMIDI_HEADER              "#OPL_II#"
#define GENMIDI_FLAG_FIXED          0x0001         /* fixed pitch */
#define GENMIDI_FLAG_2VOICE         0x0004         /* double voice (OPL3) */
static uint16_t allocatedVoices = 0;



typedef struct
{
    uint16_t freq;          // is 2-bytes ?
    uint8_t key;            // the note being played
    uint8_t channel;        // channel being using this voice
    uint8_t note;           // note
    uint8_t instrIndex;     // instrument index being set up on the voice
    uint8_t reg_pan;        // pan of the voice
    uint8_t car_volume;     // carrier volume
    uint8_t mod_volume;     // modulator volume
    uint8_t note_volume;
}  __attribute__((packed)) voice_t;
voice_t voices[OPL_NUM_VOICES];
//
// Data associated with a channel of a track that is currently playing.
typedef struct
{
    // The instrument index currently used for this track.
    uint8_t instrument;
    // Volume level
    uint8_t note_volume;
    uint8_t volume;
    uint8_t volume_base;
    // Pan - Removed, Doom music is mono.
    //int8_t pan;
    // Pitch bend value:
    int8_t bend;
} mus_channel_t;
mus_channel_t channels[MAX_MUS_CHANNELS];
// MUS event codes
typedef enum
{
    mus_releasekey = 0x00,
    mus_presskey = 0x10,
    mus_pitchwheel = 0x20,
    mus_systemevent = 0x30,
    mus_changecontroller = 0x40,
    mus_scoreend = 0x60
} musevent;
static uint32_t currentSampleIndex = 0;       // amount of samples already outputted for the song
static uint32_t nextEventSampleIndex = 0;       // when the next event shall be generated             //
static uint16_t musicReadIndex = 0;        // the index of the music.
static uint16_t numberOfReadMusicEvents = 0;
static uint8_t current_music_volume = 100;
static const unsigned short frequency_curve[] = {

    0x133, 0x133, 0x134, 0x134, 0x135, 0x136, 0x136, 0x137,   // -1
    0x137, 0x138, 0x138, 0x139, 0x139, 0x13a, 0x13b, 0x13b,
    0x13c, 0x13c, 0x13d, 0x13d, 0x13e, 0x13f, 0x13f, 0x140,
    0x140, 0x141, 0x142, 0x142, 0x143, 0x143, 0x144, 0x144,

    0x145, 0x146, 0x146, 0x147, 0x147, 0x148, 0x149, 0x149,   // -2
    0x14a, 0x14a, 0x14b, 0x14c, 0x14c, 0x14d, 0x14d, 0x14e,
    0x14f, 0x14f, 0x150, 0x150, 0x151, 0x152, 0x152, 0x153,
    0x153, 0x154, 0x155, 0x155, 0x156, 0x157, 0x157, 0x158,

    // These are used for the first seven MIDI note values:

    0x158, 0x159, 0x15a, 0x15a, 0x15b, 0x15b, 0x15c, 0x15d,   // 0
    0x15d, 0x15e, 0x15f, 0x15f, 0x160, 0x161, 0x161, 0x162,
    0x162, 0x163, 0x164, 0x164, 0x165, 0x166, 0x166, 0x167,
    0x168, 0x168, 0x169, 0x16a, 0x16a, 0x16b, 0x16c, 0x16c,

    0x16d, 0x16e, 0x16e, 0x16f, 0x170, 0x170, 0x171, 0x172,   // 1
    0x172, 0x173, 0x174, 0x174, 0x175, 0x176, 0x176, 0x177,
    0x178, 0x178, 0x179, 0x17a, 0x17a, 0x17b, 0x17c, 0x17c,
    0x17d, 0x17e, 0x17e, 0x17f, 0x180, 0x181, 0x181, 0x182,

    0x183, 0x183, 0x184, 0x185, 0x185, 0x186, 0x187, 0x188,   // 2
    0x188, 0x189, 0x18a, 0x18a, 0x18b, 0x18c, 0x18d, 0x18d,
    0x18e, 0x18f, 0x18f, 0x190, 0x191, 0x192, 0x192, 0x193,
    0x194, 0x194, 0x195, 0x196, 0x197, 0x197, 0x198, 0x199,

    0x19a, 0x19a, 0x19b, 0x19c, 0x19d, 0x19d, 0x19e, 0x19f,   // 3
    0x1a0, 0x1a0, 0x1a1, 0x1a2, 0x1a3, 0x1a3, 0x1a4, 0x1a5,
    0x1a6, 0x1a6, 0x1a7, 0x1a8, 0x1a9, 0x1a9, 0x1aa, 0x1ab,
    0x1ac, 0x1ad, 0x1ad, 0x1ae, 0x1af, 0x1b0, 0x1b0, 0x1b1,

    0x1b2, 0x1b3, 0x1b4, 0x1b4, 0x1b5, 0x1b6, 0x1b7, 0x1b8,   // 4
    0x1b8, 0x1b9, 0x1ba, 0x1bb, 0x1bc, 0x1bc, 0x1bd, 0x1be,
    0x1bf, 0x1c0, 0x1c0, 0x1c1, 0x1c2, 0x1c3, 0x1c4, 0x1c4,
    0x1c5, 0x1c6, 0x1c7, 0x1c8, 0x1c9, 0x1c9, 0x1ca, 0x1cb,

    0x1cc, 0x1cd, 0x1ce, 0x1ce, 0x1cf, 0x1d0, 0x1d1, 0x1d2,   // 5
    0x1d3, 0x1d3, 0x1d4, 0x1d5, 0x1d6, 0x1d7, 0x1d8, 0x1d8,
    0x1d9, 0x1da, 0x1db, 0x1dc, 0x1dd, 0x1de, 0x1de, 0x1df,
    0x1e0, 0x1e1, 0x1e2, 0x1e3, 0x1e4, 0x1e5, 0x1e5, 0x1e6,

    0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed, 0x1ed,   // 6
    0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5,
    0x1f6, 0x1f6, 0x1f7, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc,
    0x1fd, 0x1fe, 0x1ff, 0x200, 0x201, 0x201, 0x202, 0x203,

    // First note of looped range used for all octaves:

    0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x20a, 0x20b,   // 7
    0x20c, 0x20d, 0x20e, 0x20f, 0x210, 0x210, 0x211, 0x212,
    0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21a,
    0x21b, 0x21c, 0x21d, 0x21e, 0x21f, 0x220, 0x221, 0x222,

    0x223, 0x224, 0x225, 0x226, 0x227, 0x228, 0x229, 0x22a,   // 8
    0x22b, 0x22c, 0x22d, 0x22e, 0x22f, 0x230, 0x231, 0x232,
    0x233, 0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23a,
    0x23b, 0x23c, 0x23d, 0x23e, 0x23f, 0x240, 0x241, 0x242,

    0x244, 0x245, 0x246, 0x247, 0x248, 0x249, 0x24a, 0x24b,   // 9
    0x24c, 0x24d, 0x24e, 0x24f, 0x250, 0x251, 0x252, 0x253,
    0x254, 0x256, 0x257, 0x258, 0x259, 0x25a, 0x25b, 0x25c,
    0x25d, 0x25e, 0x25f, 0x260, 0x262, 0x263, 0x264, 0x265,

    0x266, 0x267, 0x268, 0x269, 0x26a, 0x26c, 0x26d, 0x26e,   // 10
    0x26f, 0x270, 0x271, 0x272, 0x273, 0x275, 0x276, 0x277,
    0x278, 0x279, 0x27a, 0x27b, 0x27d, 0x27e, 0x27f, 0x280,
    0x281, 0x282, 0x284, 0x285, 0x286, 0x287, 0x288, 0x289,

    0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290, 0x292, 0x293,   // 11
    0x294, 0x295, 0x296, 0x298, 0x299, 0x29a, 0x29b, 0x29c,
    0x29e, 0x29f, 0x2a0, 0x2a1, 0x2a2, 0x2a4, 0x2a5, 0x2a6,
    0x2a7, 0x2a9, 0x2aa, 0x2ab, 0x2ac, 0x2ae, 0x2af, 0x2b0,

    0x2b1, 0x2b2, 0x2b4, 0x2b5, 0x2b6, 0x2b7, 0x2b9, 0x2ba,   // 12
    0x2bb, 0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c2, 0x2c3, 0x2c4,
    0x2c5, 0x2c7, 0x2c8, 0x2c9, 0x2cb, 0x2cc, 0x2cd, 0x2ce,
    0x2d0, 0x2d1, 0x2d2, 0x2d4, 0x2d5, 0x2d6, 0x2d8, 0x2d9,

    0x2da, 0x2dc, 0x2dd, 0x2de, 0x2e0, 0x2e1, 0x2e2, 0x2e4,   // 13
    0x2e5, 0x2e6, 0x2e8, 0x2e9, 0x2ea, 0x2ec, 0x2ed, 0x2ee,
    0x2f0, 0x2f1, 0x2f2, 0x2f4, 0x2f5, 0x2f6, 0x2f8, 0x2f9,
    0x2fb, 0x2fc, 0x2fd, 0x2ff, 0x300, 0x302, 0x303, 0x304,

    0x306, 0x307, 0x309, 0x30a, 0x30b, 0x30d, 0x30e, 0x310,   // 14
    0x311, 0x312, 0x314, 0x315, 0x317, 0x318, 0x31a, 0x31b,
    0x31c, 0x31e, 0x31f, 0x321, 0x322, 0x324, 0x325, 0x327,
    0x328, 0x329, 0x32b, 0x32c, 0x32e, 0x32f, 0x331, 0x332,

    0x334, 0x335, 0x337, 0x338, 0x33a, 0x33b, 0x33d, 0x33e,   // 15
    0x340, 0x341, 0x343, 0x344, 0x346, 0x347, 0x349, 0x34a,
    0x34c, 0x34d, 0x34f, 0x350, 0x352, 0x353, 0x355, 0x357,
    0x358, 0x35a, 0x35b, 0x35d, 0x35e, 0x360, 0x361, 0x363,

    0x365, 0x366, 0x368, 0x369, 0x36b, 0x36c, 0x36e, 0x370,   // 16
    0x371, 0x373, 0x374, 0x376, 0x378, 0x379, 0x37b, 0x37c,
    0x37e, 0x380, 0x381, 0x383, 0x384, 0x386, 0x388, 0x389,
    0x38b, 0x38d, 0x38e, 0x390, 0x392, 0x393, 0x395, 0x397,

    0x398, 0x39a, 0x39c, 0x39d, 0x39f, 0x3a1, 0x3a2, 0x3a4,   // 17
    0x3a6, 0x3a7, 0x3a9, 0x3ab, 0x3ac, 0x3ae, 0x3b0, 0x3b1,
    0x3b3, 0x3b5, 0x3b7, 0x3b8, 0x3ba, 0x3bc, 0x3bd, 0x3bf,
    0x3c1, 0x3c3, 0x3c4, 0x3c6, 0x3c8, 0x3ca, 0x3cb, 0x3cd,

    // The last note has an incomplete range, and loops round back to
    // the start.  Note that the last value is actually a buffer overrun
    // and does not fit with the other values.

    0x3cf, 0x3d1, 0x3d2, 0x3d4, 0x3d6, 0x3d8, 0x3da, 0x3db,   // 18
    0x3dd, 0x3df, 0x3e1, 0x3e3, 0x3e4, 0x3e6, 0x3e8, 0x3ea,
    0x3ec, 0x3ed, 0x3ef, 0x3f1, 0x3f3, 0x3f5, 0x3f6, 0x3f8,
    0x3fa, 0x3fc, 0x3fe, 0x36c,
};

// Mapping from MIDI volume level to OPL level value.

static const unsigned char volume_mapping_table[] = {
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

static const uint8_t voice_operators[2][OPL_NUM_VOICES] = {
    { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 },
    { 0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15 },
};
//
void SetChannelVolume(uint8_t channelNumber, int8_t volume);
//
static void musResetCounters()
{
    currentSampleIndex = 0;
    nextEventSampleIndex = 0;
    musicReadIndex = 0;
    numberOfReadMusicEvents = 0;
}
void freeAllVoices()
{
    allocatedVoices = 0;
}
static inline void freeVoice(uint8_t num)
{
    allocatedVoices &= ~(1 << num);
    voices[num].note = 0;
    // TODO
}
uint32_t isVoiceAllocated(uint8_t voice)
{
    return (allocatedVoices & (1 << voice)) >> voice;
}
int getFreeVoice()
{
    int voice = -1;
    for (int i = 0; i < OPL_NUM_VOICES; i++)
    {
        if (0 == (allocatedVoices & (1 << i)))
        {
            voice = i;
            allocatedVoices |= (1 << i);
            dbgprintf("Found free voice %d\r\n",i);
            break;
        }
    }
    return voice;
}
int replaceExistingVoice()
{
    int found = 0;
    // find the voice corresponding to the one at the highest channel
    for (int i = 0; i < OPL_NUM_VOICES; i++)
    {
        if (voices[i].channel >= voices[found].channel)
        {
            found = i;
        }
    }
    return found;
}

static inline void voiceKeyOff(uint8_t index)
{
    OPL2_WriteRegister((OPL_REGS_FREQ_2 + index), voices[index].freq >> 8);
}
static void keyOffEvent(uint8_t channel, uint8_t key)
{
    // Turn off voices being used to play this key.
    // If it is a double voice instrument there will be two.

    for (int i = 0; i < OPL_NUM_VOICES; i++)
    {
        if (isVoiceAllocated(i))
        {
            if (voices[i].channel == channel && voices[i].key == key)
            {
                voiceKeyOff(i);
                freeVoice(i);
            }
        }
    }
}
void OPL_InitRegisters(void)
{
    int r;
    for (int i = 0; i < OPL_NUM_VOICES; i++)
    {
        voices[i].instrIndex = 0xFF;    // not valid
    }
    // Initialize level registers

    for (r=OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
    {
        OPL2_WriteRegister(r, 0x3f);
    }

    // Initialize other registers
    // These two loops write to registers that actually don't exist,
    // but this is what Doom does ...
    // Similarly, the <= is also intentional.

    for (r=OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
    {
        OPL2_WriteRegister(r, 0x00);
    }

    // More registers ...

    for (r=1; r < OPL_REGS_LEVEL; ++r)
    {
        OPL2_WriteRegister(r, 0x00);
    }

    // Re-initialize the low registers:

    // Reset both timers and enable interrupts:
    OPL2_WriteRegister(OPL_REG_TIMER_CTRL,      0x60);
    OPL2_WriteRegister(OPL_REG_TIMER_CTRL,      0x80);

    // "Allow FM chips to control the waveform of each operator":
    OPL2_WriteRegister(OPL_REG_WAVEFORM_ENABLE, 0x20);

    // Keyboard split point on (?)
    OPL2_WriteRegister(OPL_REG_FM_MODE,         0x40);

}

// Load data to the specified operator

static void loadOperatorData(int op, genmidi_op_t *data,
                             bool max_level, uint8_t *volume)
{
    int level;

    // The scale and level fields must be combined for the level register.
    // For the carrier wave we always set the maximum level.

    level = data->scale;

    if (max_level)
    {
        level |= 0x3f;
    }
    else
    {
        level |= data->level;
    }

    *volume = level;
    dbgprintf("Operator %d: level %d, tremolo %d, attack %d, sustain %d, waveform %d\r\n", op,level, data->tremolo, data->attack, data->sustain, data->waveform);
    OPL2_WriteRegister(OPL_REGS_LEVEL + op, level);
    OPL2_WriteRegister(OPL_REGS_TREMOLO + op, data->tremolo);
    OPL2_WriteRegister(OPL_REGS_ATTACK + op, data->attack);
    OPL2_WriteRegister(OPL_REGS_SUSTAIN + op, data->sustain);
    OPL2_WriteRegister(OPL_REGS_WAVEFORM + op, data->waveform);
}
// Set the instrument for a particular voice.

static void setVoiceInstrument(uint8_t voiceIndex, uint8_t instrIndex)
{
    genmidi_voice_t *data;
    unsigned int modulating;

    // Instrument already set for this channel?

    if (voices[voiceIndex].instrIndex == instrIndex)
    {
        return;
    }

    voices[voiceIndex].instrIndex = instrIndex;

    data = &p_wad_immutable_flash_data->instruments[instrIndex].voices[0];

    // Are we using modulated feedback mode?

    modulating = (data->feedback & 0x01) == 0;

    // Doom loads the second operator first, then the first.
    // The carrier is set to minimum volume until the voice volume
    // is set in setVoiceVolume (below).  If we are not using
    // modulating mode, we must set both to minimum volume.

    loadOperatorData(voice_operators[1][voiceIndex], &data->carrier, true, &voices[voiceIndex].car_volume);
    loadOperatorData(voice_operators[0][voiceIndex], &data->modulator, !modulating, &voices[voiceIndex].mod_volume);

    // Set feedback register that control the connection between the
    // two operators.  Turn on bits in the upper nybble; I think this
    // is for OPL3, where it turns on channel A/B.

    OPL2_WriteRegister((OPL_REGS_FEEDBACK + voiceIndex), data->feedback /*| voices[voiceIndex].reg_pan*/);

}
static void setVoiceVolume(uint8_t voiceIndex, unsigned int volume)
{

    unsigned int midi_volume;
    unsigned int full_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    genmidi_voice_t *opl_voice;

    voice_t *voice = &voices[voiceIndex];
    voice->note_volume = volume;

    opl_voice = &p_wad_immutable_flash_data->instruments[voice->instrIndex].voices[0];

    // Multiply note volume and channel volume to get the actual volume.

    midi_volume = 2 * (volume_mapping_table[channels[voice->channel].volume] + 1);

    full_volume = (volume_mapping_table[voice->note_volume] * midi_volume) >> 9;

    // The volume value to use in the register:
    car_volume = 0x3f - full_volume;

    // Update the volume register(s) if necessary.

    if (car_volume != (voice->car_volume & 0x3f))
    {
        voice->car_volume = car_volume | (voice->car_volume & 0xc0);

        OPL2_WriteRegister((OPL_REGS_LEVEL + voice_operators[1][voiceIndex]), voice->car_volume);

        // If we are using non-modulated feedback mode, we must set the
        // volume for both voices.

        if ((opl_voice->feedback & 0x01) != 0 && opl_voice->modulator.level != 0x3f)
        {
            mod_volume = opl_voice->modulator.level;
            if (mod_volume < car_volume)
            {
                mod_volume = car_volume;
            }

            mod_volume |= voice->mod_volume & 0xc0;

            if(mod_volume != voice->mod_volume)
            {
                voice->mod_volume = mod_volume;
                OPL2_WriteRegister((OPL_REGS_LEVEL + voice_operators[0][voiceIndex]),
                                  mod_volume | (opl_voice->modulator.scale & 0xc0));
            }
        }
    }
}
static unsigned int frequencyForVoice(uint8_t voiceIndex)
{
    genmidi_voice_t *gm_voice;
    signed int freq_index;
    unsigned int octave;
    unsigned int sub_index;
    signed int note;

    voice_t *voice = &voices[voiceIndex];
    note = voice->note;

    // Apply note offset.
    // Don't apply offset if the instrument is a fixed note instrument.

    gm_voice = &p_wad_immutable_flash_data->instruments[voice->instrIndex].voices[0];

    if ((p_wad_immutable_flash_data->instruments[voice->instrIndex].flags & GENMIDI_FLAG_FIXED) == 0)
    {
        note += (signed short) gm_voice->base_note_offset;
    }

    // Avoid possible overflow due to base note offset:

    while (note < 0)
    {
        note += 12;
    }

    while (note > 95)
    {
        note -= 12;
    }

    freq_index = 64 + 32 * note + channels[voice->channel].bend;

    if (freq_index < 0)
    {
        freq_index = 0;
    }

    // The first 7 notes use the start of the table, while
    // consecutive notes loop around the latter part.

    if (freq_index < 284)
    {
        return frequency_curve[freq_index];
    }

    sub_index = (freq_index - 284) % (12 * 32);
    octave = (freq_index - 284) / (12 * 32);

    // Once the seventh octave is reached, things break down.
    // We can only go up to octave 7 as a maximum anyway (the OPL
    // register only has three bits for octave number), but for the
    // notes in octave 7, the first five bits have octave=7, the
    // following notes have octave=6.  This 7/6 pattern repeats in
    // following octaves (which are technically impossible to
    // represent anyway).

    if (octave >= 7)
    {
        octave = 7;
    }

    // Calculate the resulting register value to use for the frequency.

    return frequency_curve[sub_index + 284] | (octave << 10);
}

// Update the frequency that a voice is programmed to use.

static void updateVoiceFrequency(uint8_t voiceIndex)
{
    unsigned int freq;

    voice_t *voice = &voices[voiceIndex];
    // Calculate the frequency to use for this voice and update it
    // if necessary.

    freq = frequencyForVoice(voiceIndex);

    if (voice->freq != freq)
    {
        OPL2_WriteRegister((OPL_REGS_FREQ_1 + voiceIndex), freq & 0xff);
        OPL2_WriteRegister((OPL_REGS_FREQ_2 + voiceIndex), (freq >> 8) | 0x20);
        voice->freq = freq;
    }
}

// Program a single voice for an instrument.  For a double voice
// instrument (GENMIDI_FLAG_2VOICE), this is called twice for each
// key on event.
static void voiceKeyOn(uint8_t voiceIndex,
                       uint8_t channel,
                       uint8_t instrumentIdx,
                       unsigned int note,
                       unsigned int key,
                       int volume)
{
    voices[voiceIndex].channel = channel;
    voices[voiceIndex].key = key;

    genmidi_instr_t *instrument = &p_wad_immutable_flash_data->instruments[instrumentIdx];

    // Work out the note to use.  This is normally the same as
    // the key, unless it is a fixed pitch instrument.

    if ((instrument->flags & GENMIDI_FLAG_FIXED) != 0)
    {
        voices[voiceIndex].note = instrument->fixed_note;
    }
    else
    {
        voices[voiceIndex].note = note;
    }
    //
    voices[voiceIndex].reg_pan = 0; //channels[channel].pan;

    // Program the voice with the instrument data:
    setVoiceInstrument(voiceIndex, instrumentIdx);

    // Set the volume level.
    setVoiceVolume(voiceIndex, volume);

    // Write the frequency value to turn the note on.
    voices[voiceIndex].freq = 0;
    updateVoiceFrequency(voiceIndex);
}

static void keyOnEvent(uint8_t channel, uint8_t key, int volume)
{
    //genmidi_instr_t *instrument;
    uint8_t instrument;
    unsigned int note = key;


    if (volume == -1)
    {
        volume = channels[channel].note_volume;
    }
    else
    {
      channels[channel].note_volume = volume;
    }
    // A volume of zero means key off. Some MIDI tracks, eg. the ones
    // in AV.wad, use a second key on with a volume of zero to mean
    // key off.
    if (volume <= 0)
    {
        keyOffEvent(channel, key);
        return;
    }

    // Percussion channel is treated differently.
    if (channel == MUS_PERCUSSION_CHAN)
    {
        if (key < 35 || key > 81)
        {
            return;
        }
        //instrument = &percussion_instrs[key - 35];
        instrument = GENMIDI_NUM_INSTRS + key - 35;
        note = 60;
    }
    else
    {
        instrument = channels[channel].instrument;
        if (instrument == 0xFF)
        {
            dbgprintf("ERRORRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR\r\n");
        }
    }


    int freeVoice = getFreeVoice();
    if ( freeVoice == -1)
    {
        dbgprintf("NO VOICEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\r\n");
        freeVoice = replaceExistingVoice();
    }

    // Find and program a voice for this instrument.

    voiceKeyOn(freeVoice, channel, instrument, note, key, volume);
}

// Process a pitch bend event.

static void pitchBendEvent(uint8_t channelIndex, int bend)
{

    mus_channel_t *channel = &channels[channelIndex];
    // Update the channel bend value.  Only the MSB of the pitch bend
    // value is considered: this is what Doom does.
    channel->bend = bend - 128;

    // Update all voices for this channel.

  for (int i = 0; i < OPL_NUM_VOICES; ++i)
    {
        if (voices[i].channel == channelIndex && isVoiceAllocated(i))
        {
            updateVoiceFrequency(i);
        }
    }
}
void initMusic(void)
{
    memset(voices, 0, sizeof(voices));
    memset(channels, 0, sizeof(channels));
    freeAllVoices();
    for (int i = 0; i < MAX_MUS_CHANNELS; i++)
    {
        channels[i].volume_base = 100;
        channels[i].volume = current_music_volume;
        if (channels[i].volume > channels[i].volume_base)
        {
            channels[i].volume = channels[i].volume_base;
        }
    }
}
void I_OPL_SetMusicVolume(int volume)
{
    if (current_music_volume == volume)
    {
        return;
    }
    // Internal state variable.
    current_music_volume = volume;
    // Update the volume of all voices.

    for (int i = 0; i < MAX_MUS_CHANNELS; i++)
    {
        if (i == MUS_PERCUSSION_CHAN)
        {
            SetChannelVolume(i, volume);
        }
        else
        {
            SetChannelVolume(i, channels[i].volume_base);
        }
    }
}
void SetChannelVolume(uint8_t channelNumber, int8_t volume)
{
    channels[channelNumber].volume_base = volume;

    if (volume > current_music_volume)
    {
        volume = current_music_volume;
    }
    channels[channelNumber].volume = volume;

    // Update all voices that this channel is using.

    for (int i = 0; i < OPL_NUM_VOICES; ++i)
    {
        if (voices[i].channel == channelNumber)
        {
            setVoiceVolume(i, voices[i].note_volume);
        }
    }
}
uint8_t musicEventBuffer[64];
uint8_t readMusicEvent()
{
  uint8_t v;
  if (musicReadIndex < numberOfReadMusicEvents)
  {
    v = musicEventBuffer[musicReadIndex - numberOfReadMusicEvents + sizeof(musicEventBuffer)];
  }
  else
  { // we don't have any more data
    if (_g->mus_playing > 0)
    {
      extMemSetCurrentAddress(p_wad_immutable_flash_data->musicAddrs[_g->mus_playing] + numberOfReadMusicEvents);
      numberOfReadMusicEvents += sizeof(musicEventBuffer);
      extMemGetDataFromCurrentAddress(musicEventBuffer, sizeof(musicEventBuffer));
    }
    v = musicEventBuffer[0];
  }
  musicReadIndex++;
  return v;
}
//
void setMusic(uint8_t id_num)
{
  (void) id_num;
  musResetCounters();
}
// Simple mus renderer
// musicEvents: raw events
// destBufferRight/Left: where samples will be stored
// howManySamples: how many samples we need to generate.
void generateMusicOutput(int32_t *destBuffer, uint32_t howManySamples)
{
    for (uint32_t i = 0; i < howManySamples;)
    {
        // let's process all the concurrent events
        while (nextEventSampleIndex == currentSampleIndex)
        {
            // we need to read the event.
            uint8_t eventdescriptor = readMusicEvent();
            uint8_t channel = eventdescriptor & 0x0F;
            uint8_t event = eventdescriptor & 0x70;
            uint8_t param, param2;
#if DEBUG
            static const char *eventString[256] = {"Release", "press", "pitch", "system", "controller", "UNDEF", "END"};
            dbgprintf("At %x, Sample Index: %d (%f s), Read: 0x%02x, Evt 0x%02x ch %d %s\n", musicReadIndex - 1, currentSampleIndex, currentSampleIndex / 1.0f / RENDER_SAMPLE_RATE, eventdescriptor, event, channel, eventString[event >> 4]);
#endif
            switch (event)
            {
                case mus_releasekey:
                    param = readMusicEvent();
                    dbgprintf("Key: %d\n", param);
                    keyOffEvent(channel, param);
                    break;

                case mus_presskey:
                    param = readMusicEvent();
                    dbgprintf("Key: %d\n", param & 0x7F);

                    if (param & 0x80)
                    {
                        param2 = 0x7F & readMusicEvent();
                        dbgprintf("Volume %d\n", param2);
                        keyOnEvent(channel, param & 0x7F, param2);
                    }
                    else
                    {
                        keyOnEvent(channel, param, -1);
                    }
                    break;

                case mus_pitchwheel:
                    param = readMusicEvent();
                    dbgprintf("PITCHBEND Key: %d\n", param);
                    pitchBendEvent(channel, param);
                    break;

                case mus_systemevent:
                    param = readMusicEvent();
                    dbgprintf("System Event: %d\n", param);
                    if (param < 10 || param > 14)
                    {
                        dbgprintf("Invalid system event number value!\r\n");
                    }
                    break;

                case mus_changecontroller:
                    param = readMusicEvent(); // controller number
                    param2 = readMusicEvent(); // controller value
#if DEBUG
                    {
                        static const char *controllerString[256] =
                        {
                            " Change Instrument",
                            "YBank Select",     // used but always bank 0
                            "XModulation",
                            " Volume", // this is used
                            " Pan",    // this is used
                            "XBalance",
                            "XExpression",
                            "XReverb depth",
                            "XChours depth",
                            "XSustain Pedal",
                            "XSoft Pedal",
                        };
                        dbgprintf("Change controller %d %s. Ctrl Value: %d\r\n", param, &controllerString[param][1], param2);
                        if (controllerString[param][0] == 'X')
                        {
                          dbgprintf("At %x, Sample Index: %d (%f s), Read: 0x%02x, Evt 0x%02x ch %d %s\n", musicReadIndex - 1, currentSampleIndex, currentSampleIndex / 1.0f / RENDER_SAMPLE_RATE, eventdescriptor, event, channel, eventString[event >> 4]);
                          dbgprintf("Change controller %d %s. Ctrl Value: %d\r\n", param, &controllerString[param][1], param2);
                          dbgprintf("THIS PARAM WAS UNDER WATCHLIST! EXITING!");
                            return;
                        }
                        else if (controllerString[param][0] == 'Y' && param2 != 0)
                        {
                          dbgprintf("At %x, Sample Index: %d (%f s), Read: 0x%02x, Evt 0x%02x ch %d %s\n", musicReadIndex - 1, currentSampleIndex, currentSampleIndex / 1.0f / RENDER_SAMPLE_RATE, eventdescriptor, event, channel, eventString[event >> 4]);
                          dbgprintf("Change controller %d %s. Ctrl Value: %d\r\n", param, &controllerString[param][1], param2);
                          dbgprintf("THIS PARAM WAS UNDER WATCHLIST! EXITING!\n");
                            return;
                        }
                    }
#endif

                    if (param == 0)
                    {
                      dbgprintf(">>>>>>>>>>>>>>>>>>>>>SETTING INSTURENTS TO %d\r\n", param2);
                        channels[channel].instrument = param2;
                    }
                    else
                    {
                        if (param < 1 || param > 9)
                        {
                          dbgprintf("Invalid controller param!\r\n");
                        }
                        else if (param == 3)
                        {
                            SetChannelVolume(channel, param2 & 0x7f);
                        }
                    }

                    break;

                case mus_scoreend:
                    musResetCounters();
                    break;
                default:
                    dbgprintf("ERROR! FOUND UNKNOWN EVENT %d, at position %d\r\n", event, musicReadIndex);
                    break;
            } // end switch event
            // read time delay  code
            uint32_t timedelay = 0;
            if (event != mus_scoreend && (eventdescriptor & 0x80))
            {
                for (;;)
                {
                    uint8_t tmp =  readMusicEvent();
                    timedelay = timedelay * 128 + (tmp & 0x7F);
                    if ((tmp & 0x80) == 0)
                    {
                        break;
                    }
                }
                // set new event sample index
                nextEventSampleIndex += timedelay * RENDER_SAMPLE_RATE / MUS_RATE;
            }
        }//end while
        if (nextEventSampleIndex > currentSampleIndex)      // if is <= then it was end of song
        {
            uint32_t newSamplesToGenerate = nextEventSampleIndex - currentSampleIndex;
            if (newSamplesToGenerate > howManySamples - i)
            {
                newSamplesToGenerate = howManySamples - i;
            }
            //printf("Generating %d new samples\r\n", newSamplesToGenerate);
            OPL2_GenerateStream(destBuffer + i, newSamplesToGenerate);
            i += newSamplesToGenerate;
            currentSampleIndex += newSamplesToGenerate;
        }
    }
}
