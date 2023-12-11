/**
 *  DAC and PWM (not tested anymore) for sound generation..
 *
 *  Copyright (C) 2021-2023 by Nicola Wrachien (next-hack in the comments)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 *  DESCRIPTION:
 *  Simple audio driver. It supports multiple channels, but updateSound()
 *  shall be called frequently, otherwise sound glitches will occur.
 *  With a 1024 samples audio buffer, the minimum frame rate (and update rate)
 *  is (11025 / (1024 - AUDIO_BUFFER_DELAY)), i.e. 12 fps.
 *
 */
#ifndef SRC_PWM_AUDIO_H_
#define SRC_PWM_AUDIO_H_
#include "main.h"
#include "em_device.h"
#include "em_gpio.h"
#include "macros.h"
#include "boards.h"
//
#define AUDIO_SAMPLE_RATE               11025
//
#define MAX_CHANNELS                    (8)
#define AUDIO_BUFFER_LENGTH             (1024)
#define AUDIO_BUFFER_DELAY              (200)   // up to 20 ms delay
#define ZERO_AUDIO_LEVEL                (2048)  // 12 bit dac
//
#define DMX_DATA_SOUND_OFFSET       0x18
//
#ifndef AUDIO_SAMPLE_TIMER
  #define AUDIO_SAMPLE_TIMER_NUMBER     2
  #define AUDIO_SAMPLE_TIMER            CAT(TIMER, AUDIO_SAMPLE_TIMER_NUMBER)
#endif
  #if AUDIO_MODE == PWM_AUDIO_MODE
    #ifndef AUDIO_PWM_TIMER_NUMBER
      #define AUDIO_PWM_TIMER_NUMBER    1
      #define AUDIO_PWM_TIMER           CAT(TIMER, AUDIO_PWM_TIMER_NUMBER)
  #endif
#endif

//
typedef struct
{
    uint16_t lastAudioBufferIdx;
    uint16_t offset;
    uint8_t sfxIdx;
    int8_t volumeLeft;
    int8_t volumeRight;
} soundChannel_t;
//
void initAudio();
void updateSound();
void muteSound();
//
extern soundChannel_t soundChannels[MAX_CHANNELS];
extern int16_t audioBuffer[AUDIO_BUFFER_LENGTH];
#endif /* SRC_PWM_AUDIO_H_ */
