/**
 *  DAC and PWM for sound generation..
 *
 *  Copyright (C) 2021-2024 by Nicola Wrachien (next-hack in the comments)
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
#pragma GCC optimize("Ofast") // we need to compile this code to be as fast as possible.
#include "audio.h"
#include "extMemory.h"
#include "sounds.h"
#include "w_wad.h"
#include "delay.h"
#include <string.h>
#include "em_ldma.h"
#include "em_cmu.h"
#include "em_vdac.h"
#include "em_gpio.h"
#include "em_prs.h"
//
#include "boards.h"
#include "main.h"
#include "global_data.h"
//
uint16_t lastMusicIdx = 0;
#if ENABLE_MUSIC
int16_t musBuffer[MUSIC_NUM_SAMPLES];
#endif
//
#define AUDIO_SAMPLE_TIMER_CLOCK CAT(cmuClock_TIMER, AUDIO_SAMPLE_TIMER_NUMBER)
#if AUDIO_MODE == PWM_AUDIO_MODE
    #define GLOBAL_AUDIO_RSHIFT 0
    #define AUDIO_PWM_TIMER_CLOCK CAT(cmuClock_TIMER, AUDIO_PWM_TIMER_NUMBER)
#define LDMA_REQSEL_VALUE CAT(LDMAXBAR_CH_REQSEL_SOURCESEL_TIMER, AUDIO_SAMPLE_TIMER_NUMBER) | CAT3(LDMAXBAR_CH_REQSEL_SIGSEL_TIMER, AUDIO_SAMPLE_TIMER_NUMBER, UFOF)
#elif AUDIO_MODE == DAC_AUDIO_MODE
    #define GLOBAL_AUDIO_RSHIFT 4
  #define LDMA_REQSEL_VALUE CAT(LDMAXBAR_CH_REQSEL_SOURCESEL_TIMER, AUDIO_SAMPLE_TIMER_NUMBER) | CAT3(LDMAXBAR_CH_REQSEL_SIGSEL_TIMER, AUDIO_SAMPLE_TIMER_NUMBER, UFOF)
#else
  #error audio mode not defined
#endif
//
int16_t audioBuffer[AUDIO_BUFFER_LENGTH];
#if STEREO_AUDIO
  int16_t audioBufferRight[AUDIO_BUFFER_LENGTH];
  static LDMA_Descriptor_t dmaXfer[2];  // 2 channels only
#else
  static LDMA_Descriptor_t dmaXfer[1];  // 1 channel only
#endif
soundChannel_t soundChannels[MAX_CHANNELS];
// Set the VDAC to max frequency of 1 MHz
#define CLK_VDAC_FREQ              1000000
//
void initAudio()
{
  //
  // enable timer
    CMU_ClockEnable(AUDIO_SAMPLE_TIMER_CLOCK, true);   // for Audio
    #if AUDIO_MODE == PWM_AUDIO_MODE
        CMU_ClockEnable(AUDIO_PWM_TIMER_CLOCK, true);
        // TIMER 1 generates PWM
        AUDIO_PWM_TIMER->CFG = TIMER_CFG_PRESC_DIV1 | TIMER_CFG_MODE_UPDOWN;
        AUDIO_PWM_TIMER->CC[0].CFG = TIMER_CC_CFG_MODE_PWM;
        AUDIO_PWM_TIMER->CC[1].CFG = TIMER_CC_CFG_MODE_PWM;
        AUDIO_PWM_TIMER->EN = TIMER_EN_EN;
        AUDIO_PWM_TIMER->CC[0].CTRL = TIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | TIMER_CC_CTRL_CMOA_TOGGLE;
        AUDIO_PWM_TIMER->CC[1].CTRL = TIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | TIMER_CC_CTRL_CMOA_TOGGLE;
        AUDIO_PWM_TIMER->TOP = 0xFF;
        //
        AUDIO_PWM_TIMER->CC[0].OC = 0x10;
        AUDIO_PWM_TIMER->CC[1].OC = 0x10;
        AUDIO_PWM_TIMER->CMD = TIMER_CMD_START;
        // Route PWM pin
        GPIO_PinModeSet(AUDIO_PORT_L, AUDIO_PIN_L , gpioModePushPullAlternate, 0);
        GPIO->TIMERROUTE[AUDIO_PWM_TIMER_NUMBER].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN;
        GPIO->TIMERROUTE[AUDIO_PWM_TIMER_NUMBER].CC0ROUTE = (AUDIO_PORT_L << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) | (AUDIO_PIN_L << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
#if STEREO_AUDIO
        GPIO_PinModeSet(AUDIO_PORT_R, AUDIO_PIN_R , gpioModePushPullAlternate, 0);
        GPIO->TIMERROUTE[AUDIO_PWM_TIMER_NUMBER].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN | GPIO_TIMER_ROUTEEN_CC1PEN;
        GPIO->TIMERROUTE[AUDIO_PWM_TIMER_NUMBER].CC1ROUTE = (AUDIO_PORT_R << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT) | (AUDIO_PIN_R << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);
#endif
    #else
        CMU_ClockEnable(cmuClock_VDAC0, true);
        __IOM uint32_t *analogBus[] = {&GPIO->ABUSALLOC, &GPIO->BBUSALLOC, &GPIO->CDBUSALLOC, &GPIO->CDBUSALLOC};
        #if AUDIO_PIN_L & 1
            *analogBus[AUDIO_PORT_L] = GPIO_CDBUSALLOC_CDODD0_VDAC0CH0;
        #else
            *analogBus[AUDIO_PORT_L] = GPIO_CDBUSALLOC_CDEVEN0_VDAC0CH0;
        #endif
        VDAC_Init_TypeDef vdacInit          = VDAC_INIT_DEFAULT;
        vdacInit.reference = vdacRefAvdd;
        // Set prescaler to get 1 MHz VDAC clock frequency.
        vdacInit.prescaler = VDAC_PrescaleCalc(VDAC0, 1000000);
        VDAC_Init(VDAC0, &vdacInit);
        VDAC_InitChannel_TypeDef vdacChInit = VDAC_INITCHANNEL_DEFAULT;
        vdacChInit.auxOutEnable = true;
        vdacChInit.mainOutEnable = false;
        //
        vdacChInit.pin = AUDIO_PIN_L;
        vdacChInit.port = AUDIO_PORT_L + 1; // GpioPortA starts from 0, but vdacPort starts from 1...
        vdacChInit.enable = true;
        VDAC_InitChannel(VDAC0, &vdacChInit, 0);
        #if STEREO_AUDIO
            #if (AUDIO_PORT_R != AUDIO_PORT_L)
              #if AUDIO_PIN_R & 1
                  *analogBus[AUDIO_PORT_R] = GPIO_CDBUSALLOC_CDODD1_VDAC0CH1;
              #else
                  *analogBus[AUDIO_PORT_R] = GPIO_CDBUSALLOC_CDEVEN1_VDAC0CH1;
              #endif
            #else
                #if AUDIO_PIN_R & 1
                    *analogBus[AUDIO_PORT_R] |= GPIO_CDBUSALLOC_CDODD1_VDAC0CH1;
                #else
                    *analogBus[AUDIO_PORT_R] |= GPIO_CDBUSALLOC_CDEVEN1_VDAC0CH1;
                #endif
            #endif
            // configure right channel as well
            vdacChInit.pin = AUDIO_PIN_R;
            vdacChInit.port = AUDIO_PORT_R + 1; // GpioPortA starts from 0, but vdacPort starts from 1...
            VDAC_InitChannel(VDAC0, &vdacChInit, 1);
        #endif
    #endif
    //
    // Configure Timer AUDIO_SAMPLE_TIMER to generate a signal every 1/11025 s
    AUDIO_SAMPLE_TIMER->CFG = TIMER_CFG_PRESC_DIV1 | TIMER_CFG_DMACLRACT;
    AUDIO_SAMPLE_TIMER->EN = TIMER_EN_EN;
    AUDIO_SAMPLE_TIMER->TOP = (80000000 / AUDIO_SAMPLE_RATE) - 1;
    AUDIO_SAMPLE_TIMER->CMD = TIMER_CMD_START;
    // Config for looping sound
    LDMAXBAR->CH[AUDIO_DMA_CHANNEL_L].REQSEL = LDMA_REQSEL_VALUE;
    //
    LDMA->CH[AUDIO_DMA_CHANNEL_L].LOOP = 0 << _LDMA_CH_LOOP_LOOPCNT_SHIFT;
    LDMA->CH[AUDIO_DMA_CHANNEL_L].CFG = LDMA_CH_CFG_ARBSLOTS_ONE | LDMA_CH_CFG_SRCINCSIGN_POSITIVE | _LDMA_CH_CFG_DSTINCSIGN_POSITIVE;

    // configure transfer descriptor
    dmaXfer[0].xfer.structType = _LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    // destination
    dmaXfer[0].xfer.dstAddrMode = _LDMA_CH_CTRL_DSTMODE_ABSOLUTE;
    #if AUDIO_MODE == PWM_AUDIO_MODE
        dmaXfer[0].xfer.dstAddr = (uint32_t) &AUDIO_PWM_TIMER->CC[0].OCB;
        dmaXfer[0].xfer.srcAddr = ((uint32_t) audioBuffer) + 1; //16 bit size, using only top part
        dmaXfer[0].xfer.srcInc = _LDMA_CH_CTRL_SRCINC_TWO;
        //
        dmaXfer[0].xfer.size = _LDMA_CH_CTRL_SIZE_BYTE;
        dmaXfer[0].xfer.blockSize = _LDMA_CH_CTRL_BLOCKSIZE_UNIT1; // one byte per transfer
#if STEREO_AUDIO
        // Config for looping sound
      LDMAXBAR->CH[AUDIO_DMA_CHANNEL_R].REQSEL = LDMA_REQSEL_VALUE;
      //
      LDMA->CH[AUDIO_DMA_CHANNEL_R].LOOP = 0 << _LDMA_CH_LOOP_LOOPCNT_SHIFT;
      LDMA->CH[AUDIO_DMA_CHANNEL_R].CFG = LDMA_CH_CFG_ARBSLOTS_ONE | LDMA_CH_CFG_SRCINCSIGN_POSITIVE | _LDMA_CH_CFG_DSTINCSIGN_POSITIVE;

      // configure transfer descriptor
      dmaXfer[1].xfer.structType = _LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
      // destination
      dmaXfer[1].xfer.dstAddrMode = _LDMA_CH_CTRL_DSTMODE_ABSOLUTE;

      dmaXfer[1].xfer.dstAddr = (uint32_t) &AUDIO_PWM_TIMER->CC[1].OCB;
      dmaXfer[1].xfer.srcAddr = ((uint32_t) audioBufferRight) + 1; //16 bit size, only top part
      dmaXfer[1].xfer.srcInc = _LDMA_CH_CTRL_SRCINC_TWO;
      //
      dmaXfer[1].xfer.size = _LDMA_CH_CTRL_SIZE_BYTE;
      dmaXfer[1].xfer.blockSize = _LDMA_CH_CTRL_BLOCKSIZE_UNIT1; // one unit per transfer
      dmaXfer[1].xfer.dstInc = _LDMA_CH_CTRL_DSTINC_NONE;
      //
      dmaXfer[1].xfer.srcAddrMode = _LDMA_CH_CTRL_SRCMODE_ABSOLUTE;

      dmaXfer[1].xfer.xferCnt = AUDIO_BUFFER_LENGTH - 1;
      dmaXfer[1].xfer.linkAddr = (uint32_t) &dmaXfer[1] >> 2;
      dmaXfer[1].xfer.link = 1;
      dmaXfer[1].xfer.linkMode = _LDMA_CH_LINK_LINKMODE_ABSOLUTE;
      //
      /* Set the descriptor address. */
      LDMA->CH[AUDIO_DMA_CHANNEL_R].LINK = ((uint32_t) &dmaXfer[1] & _LDMA_CH_LINK_LINKADDR_MASK) | LDMA_CH_LINK_LINK;
#endif
    #else
        #if STEREO_AUDIO
              // Config for looping sound
              LDMAXBAR->CH[AUDIO_DMA_CHANNEL_R].REQSEL = LDMA_REQSEL_VALUE;
              //
              LDMA->CH[AUDIO_DMA_CHANNEL_R].LOOP = 0 << _LDMA_CH_LOOP_LOOPCNT_SHIFT;
              LDMA->CH[AUDIO_DMA_CHANNEL_R].CFG = LDMA_CH_CFG_ARBSLOTS_ONE | LDMA_CH_CFG_SRCINCSIGN_POSITIVE | _LDMA_CH_CFG_DSTINCSIGN_POSITIVE;

              // configure transfer descriptor
              dmaXfer[1].xfer.structType = _LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
              // destination
              dmaXfer[1].xfer.dstAddrMode = _LDMA_CH_CTRL_DSTMODE_ABSOLUTE;

              dmaXfer[1].xfer.dstAddr = (uint32_t) &VDAC0->CH1F;
              dmaXfer[1].xfer.srcAddr = ((uint32_t) audioBufferRight); //16 bit size,
              dmaXfer[1].xfer.srcInc = _LDMA_CH_CTRL_SRCINC_ONE;
              //
              dmaXfer[1].xfer.size = _LDMA_CH_CTRL_SIZE_HALFWORD;
              dmaXfer[1].xfer.blockSize = _LDMA_CH_CTRL_BLOCKSIZE_UNIT1; // one unit per transfer
              dmaXfer[1].xfer.dstInc = _LDMA_CH_CTRL_DSTINC_NONE;
              //
              dmaXfer[1].xfer.srcAddrMode = _LDMA_CH_CTRL_SRCMODE_ABSOLUTE;

              dmaXfer[1].xfer.xferCnt = AUDIO_BUFFER_LENGTH - 1;
              dmaXfer[1].xfer.linkAddr = (uint32_t) &dmaXfer[1] >> 2;
              dmaXfer[1].xfer.link = 1;
              dmaXfer[1].xfer.linkMode = _LDMA_CH_LINK_LINKMODE_ABSOLUTE;
              //
              /* Set the descriptor address. */
              LDMA->CH[AUDIO_DMA_CHANNEL_R].LINK = ((uint32_t) &dmaXfer[1] & _LDMA_CH_LINK_LINKADDR_MASK) | LDMA_CH_LINK_LINK;
        #endif
        dmaXfer[0].xfer.dstAddr = (uint32_t) &VDAC0->CH0F;
        dmaXfer[0].xfer.srcAddr = ((uint32_t) audioBuffer); //16 bit size,
        dmaXfer[0].xfer.srcInc = _LDMA_CH_CTRL_SRCINC_ONE;
        //
        dmaXfer[0].xfer.size = _LDMA_CH_CTRL_SIZE_HALFWORD;
        dmaXfer[0].xfer.blockSize = _LDMA_CH_CTRL_BLOCKSIZE_UNIT1; // one unit per transfer
    #endif
    dmaXfer[0].xfer.dstInc = _LDMA_CH_CTRL_DSTINC_NONE;
    //
    dmaXfer[0].xfer.srcAddrMode = _LDMA_CH_CTRL_SRCMODE_ABSOLUTE;

    dmaXfer[0].xfer.xferCnt = AUDIO_BUFFER_LENGTH - 1;
    dmaXfer[0].xfer.linkAddr = (uint32_t) &dmaXfer[0] >> 2;
    dmaXfer[0].xfer.link = 1;
    dmaXfer[0].xfer.linkMode = _LDMA_CH_LINK_LINKMODE_ABSOLUTE;
    //
    /* Set the descriptor address. */
    LDMA->CH[AUDIO_DMA_CHANNEL_L].LINK = ((uint32_t) &dmaXfer[0] & _LDMA_CH_LINK_LINKADDR_MASK) | LDMA_CH_LINK_LINK;
    //
    LDMA->IF_CLR = 1;
    //
    BUS_RegMaskedClear(&LDMA->CHDONE, (1 << AUDIO_DMA_CHANNEL_L) | (STEREO_AUDIO ? ((1 << AUDIO_DMA_CHANNEL_R)) : 0)); /* Clear the done flag.     */
    LDMA->LINKLOAD = (1 << AUDIO_DMA_CHANNEL_L) | (STEREO_AUDIO ? ((1 << AUDIO_DMA_CHANNEL_R)) : 0); /* Start a transfer by loading the descriptor.  */
    //
    memset(soundChannels, 0, sizeof(soundChannels));
}
/**
 * Immediately shuts off sound
 */
void muteSound()
{
    for (int i = 0; i < AUDIO_BUFFER_LENGTH; i++)
    {
        #if AUDIO_MODE == PWM_AUDIO_MODE
            audioBuffer[i] = ZERO_AUDIO_LEVEL;
            #if STEREO_AUDIO
                  audioBufferRight[i] = ZERO_AUDIO_LEVEL;
            #endif

        #else
            audioBuffer[i] = ZERO_AUDIO_LEVEL;
            #if STEREO_AUDIO
                  audioBufferRight[i] = ZERO_AUDIO_LEVEL;
            #endif
        #endif
    }
    for (int ch = 0; ch < MAX_CHANNELS; ch++)
    {
        soundChannels[ch].sfxIdx = 0;
        soundChannels[ch].volumeLeft = 0;
        soundChannels[ch].volumeRight = 0;
    }
}
/*
 * next-hack: poor's man mixer.
 *
 * There are N channels, each one can play one sample. We cannot have infinite buffer
 * and we cannot use interrupts, because we cannot interrupt time critical DSPI flash readout.
 * Therefore we create a buffer with 1024 samples, which is sent by DMA to the PWM.
 * Each sample of the buffer is 16-bit, because we need to mix all the channels.
 * The audio buffer is updated after all drawing operations have been done.
 *
 * Since doom's sample rate is 11025 Hz, then with a 1024 buffer, the minimum
 * required frame rate is about 11 fps, which is quite low and already unplayable on its own.
 *
 * The actual frame rate is unknown so we need to see where is the DMA source pointer, and
 * start updating some samples after its current position.
 *
 * This means a small delay for new samples, of some ms.
 *
 *
 */
void updateSound()
{
    // where are we in our circular buffer?
    uint32_t currentIdx = (LDMA->CH[AUDIO_DMA_CHANNEL_L].SRC - (uint32_t) audioBuffer) / sizeof(audioBuffer[0]);
    // we cannot start updating the audio buffer just on next sample (which will occur in about 90us from now).
    // if frame rate is high enough, then the audio buffer is valid several samples after currentIdx.
    // therefore we can start updating the audio buffer AUDIO_BUFFER_DELAY samples after the current one.
    // AUDIO_BUFFER_DELAY must be chosen so that we have enough time to read the sample data from all the lumps
    // and store to the buffer. This might take some time.
    // The index at which we recalculate/update audio samples is startIdx.
    // The buffer is recalculated from startIdx up to the sample before currentIdx
    // As first thing, zeroize buffer;
    // Note that we do not have to end exactly at current index.
#define TWO_SAMPLES_AT_TIME 1
#if TWO_SAMPLES_AT_TIME
    uint32_t startIdx = (currentIdx + AUDIO_BUFFER_DELAY) & (AUDIO_BUFFER_LENGTH - 1);
    if (startIdx & 1)
    {
      audioBuffer[startIdx] = 0;
      #if STEREO_AUDIO
            audioBufferRight[startIdx] = 0;
      #endif
    }
    for (uint32_t i = ((startIdx + 1) & (AUDIO_BUFFER_LENGTH - 1) )  >> 1; i != (((currentIdx - 2) & (AUDIO_BUFFER_LENGTH - 1))) >>1 ; i = ((i + 1) & ((AUDIO_BUFFER_LENGTH - 1) >> 1) ) )
    {
        ((uint32_t*)audioBuffer)[i] = 0; //(ZERO_AUDIO_LEVEL) | (ZERO_AUDIO_LEVEL << 16);
        #if STEREO_AUDIO
        ((uint32_t*)audioBufferRight)[i] = 0; //(ZERO_AUDIO_LEVEL) | (ZERO_AUDIO_LEVEL << 16);
        #endif
    }

#else
    uint32_t startIdx = (currentIdx + AUDIO_BUFFER_DELAY) & (AUDIO_BUFFER_LENGTH - 1);
    for (uint32_t i = startIdx; i != ((currentIdx - 1) & (AUDIO_BUFFER_LENGTH - 1)); i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
    {
        audioBuffer[i] = 0;
        #if STEREO_AUDIO
            audioBufferRight[i] = 0;
        #endif
    }

#endif
    // Now, for each channel we need to copy sample data.
    for (int ch = 0; ch < MAX_CHANNELS; ch++)
    {
        // channel active?
#if STEREO_AUDIO
        if ((soundChannels[ch].volumeLeft || soundChannels[ch].volumeRight) && soundChannels[ch].sfxIdx)
#else
          if (soundChannels[ch].volumeLeft && soundChannels[ch].sfxIdx)

#endif
        {
            const void *lumpPtr = p_wad_immutable_flash_data->soundLumps[soundChannels[ch].sfxIdx].lumpAddress;
            if (!isOnExternalFlash(lumpPtr))
                continue;
            // get sample size
            int32_t size =  p_wad_immutable_flash_data->soundLumps[soundChannels[ch].sfxIdx].length;

            uint32_t samplesPlayed;
            // is this a new sample (not played before? Then do not change the offset
            if (soundChannels[ch].lastAudioBufferIdx != 0xFFFF)
            {
                // otherwise, we need to calcualte how many sample do we have played since last call.
                // considering also that we are starting at startIdx.
                samplesPlayed = (startIdx - soundChannels[ch].lastAudioBufferIdx) & (AUDIO_BUFFER_LENGTH - 1);
                soundChannels[ch].offset = soundChannels[ch].offset + samplesPlayed;
            }
            // remember last index.
            soundChannels[ch].lastAudioBufferIdx = startIdx;
            // how many bytes do we need to read?
            int32_t increment =  p_wad_immutable_flash_data->soundLumps[soundChannels[ch].sfxIdx].increment;

            int32_t sizeToRead = size - soundChannels[ch].offset * increment;
            if (sizeToRead <= 0) // already outputted all samples? zeroize index and volume.
            {
                soundChannels[ch].sfxIdx = 0;
                soundChannels[ch].volumeLeft = 0;
                soundChannels[ch].volumeRight = 0;
                continue;
            }
            // if the number of samples to read exceed the size of the buffer we need to update
            // then crop it
            // note: in samples where the sample rate is 22050, we must read twice the amount of data.
            if (sizeToRead > (AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY) * increment)
            {
                sizeToRead = (AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY) * increment;
            }
            // create a temporary buffer. In stack is ok, we have plenty
            uint8_t tmpBuffer[2 * (AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY)];
            extMemSetCurrentAddress((uint32_t) lumpPtr + soundChannels[ch].offset * increment + DMX_DATA_SOUND_OFFSET);
            // read audio bytes
            extMemGetDataFromCurrentAddress(tmpBuffer, sizeToRead); //AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY);
            //
            uint32_t stopIdx;
            if (increment == 1)
            {
              stopIdx = (startIdx + sizeToRead) & (AUDIO_BUFFER_LENGTH - 1);
            }
            else
            {
              stopIdx = (startIdx + sizeToRead / 2) & (AUDIO_BUFFER_LENGTH - 1);
            }
            //
            uint8_t *p = tmpBuffer;
            for (uint32_t i = startIdx; i != stopIdx; i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
            {
                // update audio buffer
                int16_t sampleValue = (0xFF & *p) - 128;  // sound fx are unsigned
                p = p + increment;
                int16_t sampleValueLeft = sampleValue * soundChannels[ch].volumeLeft;
                audioBuffer[i] += sampleValueLeft;
                #if STEREO_AUDIO
                    int16_t sampleValueRight = sampleValue * soundChannels[ch].volumeRight;
                      audioBufferRight[i] += sampleValueRight;
                #endif
            }
        }
    }
    // now add music
#if ENABLE_MUSIC
    if (_g->mus_playing && !_g->mus_paused && _g->snd_MusicVolume)
    {
      int32_t musBuffer32[MUSIC_NUM_SAMPLES];
      //
      uint32_t numberOfSamplesToGenerate =  (1 + (currentIdx - 1 - lastMusicIdx))  & (MUSIC_NUM_SAMPLES - 1);
      generateMusicOutput(musBuffer32, numberOfSamplesToGenerate);
      uint32_t m = 0;
      for (; lastMusicIdx != ((currentIdx - 1) & (MUSIC_NUM_SAMPLES - 1)); lastMusicIdx = (lastMusicIdx + 1) & (MUSIC_NUM_SAMPLES - 1))
      {
        int32_t sv =  musBuffer32[m++];
        musBuffer[lastMusicIdx] = __SSAT(sv, 16);
      }
    }

#endif
    //
//    #if AUDIO_MODE == DAC_AUDIO_MODE
        if (_g->mus_playing && !_g->mus_paused && _g->snd_MusicVolume)
        {
          for (uint32_t i = startIdx; i != ((currentIdx - 1) & (AUDIO_BUFFER_LENGTH - 1)); i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
          {
              int32_t music = musBuffer[i];
              audioBuffer[i] = (((audioBuffer[i] + music)) >> GLOBAL_AUDIO_RSHIFT) + ZERO_AUDIO_LEVEL;
              #if STEREO_AUDIO
                  audioBufferRight[i] = (((audioBufferRight[i] + music)) >> GLOBAL_AUDIO_RSHIFT) + ZERO_AUDIO_LEVEL;
              #endif
          }
        }
        else
        {
          for (uint32_t i = startIdx; i != ((currentIdx - 1) & (AUDIO_BUFFER_LENGTH - 1)); i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
          {
              audioBuffer[i] = (audioBuffer[i] >> GLOBAL_AUDIO_RSHIFT) + ZERO_AUDIO_LEVEL;
              #if STEREO_AUDIO
                  audioBufferRight[i] = (audioBufferRight[i] >> GLOBAL_AUDIO_RSHIFT) + ZERO_AUDIO_LEVEL;
              #endif
          }
        }
//    #endif
}
