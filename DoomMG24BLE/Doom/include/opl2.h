// File: opl2.h/opl2.c
// Nuked OPL3 Emulator, adapted for Doom on MCU ports by Nicola Wrachien.
// This is atually an OPL2 emulator, with the bare minimum functionality for
// Doom.
// What's changed:
//  - Reduced number of voices and operators to match OPL2
//  - Some RAM savings by trimming the data size
//  - Percussion support was removed.
//  - Support for 11025 Hz sample rate
//  - Improved code for MCU speed. In particular for each operator we calculate
//    all the samples, and then they are summed at the end. Original code, for
//    each sample will iterate all the operators.
//  - heavy usage of tables.
//  Note: the sound will not be byte-exact even for original sample rate, because
//  of some optimizations, but the impact is negligible.
//
//  Copyright (C) 2013-2018 Alexey Khokholov (Nuke.YKT) (original Nuked 3)
//  Copyright (C) 2022-2023 Nicola Wrachien (next-hack) (Modifications for Doom)
//
// Original Copyright message follows:
//
// Copyright (C) 2013-2018 Alexey Khokholov (Nuke.YKT)
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
//
//  Nuked OPL3 emulator.
//  Thanks:
//      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
//          Feedback and Rhythm part calculation information.
//      forums.submarine.org.uk(carbon14, opl3):
//          Tremolo and phase generator calculation information.
//      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
//          OPL2 ROMs.
//      siliconpr0n.org(John McMaster, digshadow):
//          YMF262 and VRC VII decaps and die shots.
//
// version: 1.8
//

#ifndef OPL_OPL_H
#define OPL_OPL_H
#include <inttypes.h>

#define OPL_NO_FLOAT 0
#define ORIGINAL_SAMPLE_RATE        49716
#define RENDER_SAMPLE_RATE          11025
#define OPL_NUM_VOICES              9
#define OPL_NUM_OPERATORS           (2 * OPL_NUM_VOICES)

#define OPL_REG_WAVEFORM_ENABLE   0x01
#define OPL_REG_TIMER1            0x02
#define OPL_REG_TIMER2            0x03
#define OPL_REG_TIMER_CTRL        0x04
#define OPL_REG_FM_MODE           0x08
#define OPL_REG_NEW               0x105

// Operator registers (21 of each):

#define OPL_REGS_TREMOLO          0x20
#define OPL_REGS_LEVEL            0x40
#define OPL_REGS_ATTACK           0x60
#define OPL_REGS_SUSTAIN          0x80
#define OPL_REGS_WAVEFORM         0xE0

// Voice registers (9 of each):

#define OPL_REGS_FREQ_1           0xA0
#define OPL_REGS_FREQ_2           0xB0
#define OPL_REGS_FEEDBACK         0xC0



typedef uintptr_t       Bitu;
typedef intptr_t        Bits;
typedef uint64_t        Bit64u;
typedef int64_t         Bit64s;
typedef uint32_t        Bit32u;
typedef int32_t         Bit32s;
typedef uint16_t        Bit16u;
typedef int16_t         Bit16s;
typedef uint8_t         Bit8u;
typedef int8_t          Bit8s;

typedef struct _opl2_slot opl2_slot;
typedef struct _opl2_channel opl2_channel;
typedef struct _opl2_chip opl2_chip;


struct _opl2_slot {

    Bit32u pg_phase;
    const int32_t *vib_table;
    const int32_t *waveformMod;
#if OPL_NO_FLOAT
    Bit16s eg_rout;
#else
    float eg_rout_f;
#endif
    Bit16s out;             // output
    Bit16s prout;           // previous output (for fb calculation)

    uint16_t tl_ksl_add;

    Bit8u eg_gen;       // current phase on envelope generator

// bit config
// todo: determine which should be used without bitfield
// REGISTER 0x2x
    Bit8u bits_mult : 4;
    Bit8u bit_trem : 1;
    Bit8u bit_vib : 1;
    Bit8u bit_sustain : 1;
    Bit8u bit_ksr : 1;
// REGISTER 0x4x
    Bit8u bits_tl : 6;
    Bit8u bits_ksl : 2;
// REGISTER 0x6x
    Bit8u bits_dr : 4;
    Bit8u bits_ar : 4;
// REGISTER 0x8x
    Bit16u bits_rr : 4;
    Bit16u bits_sl : 5;   // this is 5 bit because if we write 0x0F we actually need to compute with 31.
    //
    Bit16u reg_wf : 2;

    //
    Bit8u key;      // we can save 1 byte per slot if we join this to wf

    uint8_t chanNum;
    uint8_t ks;
};

struct _opl2_channel {
    opl2_slot *slots[2];
    Bit16u f_num;
    Bit8u block;
    Bit8u nineMinusFb;
    Bit8u con;
    Bit8u alg;
    Bit8u ksv;

};

struct _opl2_chip {
    opl2_channel channel[OPL_NUM_VOICES];
    opl2_slot slot[OPL_NUM_OPERATORS];
    const uint8_t *tremoloTable;
    uint32_t timerFixedPoint;
    uint32_t tremolopos;
    uint16_t timer;

    Bit8u eg_timerrem;

    Bit8u nts;

    Bit8u vibshift;
};


void OPL2_GenerateStream(int32_t * buffer, uint32_t numsamples);
void OPL2_Reset(Bit32u samplerate);
void OPL2_WriteRegister(uint16_t reg, uint8_t value);
#endif


