//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
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
//	System interface for sound.
//

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "../audio/audio.h"
#include "../demo/demo.h"



#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif

#include "deh_str.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"
#include "xil_types.h"

//was not defined by default
typedef signed short Sint16;

#include "doomtype.h"


//defined in doomgeneric_zyboz7.c
extern u32* currentBuffer;
extern u32* nextBuffer;
extern int16_t playBuffer[BUFFER_MUSIC_SIZE];
extern u32 playBufferLen;
extern u32 currentLen;
extern XAxiDma sAxiDma;

//declared in isdl_music.c
extern boolean_doom current_track_music;


#define NUM_CHANNELS 16 //number of sound channels that can be played at the same time

#define MIX_MAX_VOLUME 127 /* Volume of a chunk defined by SDL*/

typedef struct allocated_sound_s allocated_sound_t;

//original struct from SDL_mixer, uses no particular library
typedef struct Mix_Chunk {
    int allocated;
    u8 *abuf;
    u32 alen;
    u8 volume;       /* Per-sample volume, 0-128 */
} Mix_Chunk;

//structure for a sound channel
typedef struct {
    u8 *data;       //data to be played
    int length;     //length of the data
    int position;   //current position in the data
    int active;     //is the channel active
    int vol;        //volume of the channel
} SoundChannel;

//structure for a sound effect
static SoundChannel channels[NUM_CHANNELS];


struct allocated_sound_s
{
    sfxinfo_t *sfxinfo;
    Mix_Chunk chunk;
    int use_count;
    allocated_sound_t *prev, *next;
};

static boolean_doom setpanning_workaround = doom_false;

static boolean_doom sound_initialized = doom_false;

static sfxinfo_t *channels_playing[NUM_CHANNELS];

//not used in this implementation as sound is mono
static u8 channel_left_volume[NUM_CHANNELS] = { 0 };
static u8 channel_right_volume[NUM_CHANNELS] = { 0 };

static int mixer_freq;
static int mixer_channels;
static boolean_doom use_sfx_prefix;
static boolean_doom (*ExpandSoundData)(sfxinfo_t *sfxinfo,
                                  byte *data,
                                  int samplerate,
                                  int length) = NULL;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest
// sounds not used recently are at the tail.

static allocated_sound_t *allocated_sounds_head = NULL;
static allocated_sound_t *allocated_sounds_tail = NULL;
static int allocated_sounds_size = 0;

int use_libsamplerate = 0;

// Scale factor used when converting libsamplerate floating point numbers
// to integers. Too high means the sounds can clip; too low means they
// will be too quiet. This is an amount that should avoid clipping most
// of the time: with all the Doom IWAD sound effects, at least. If a PWAD
// is used, clipping might occur.

float libsamplerate_scale = 0.65f;

// Hook a sound into the linked list at the head.

static void AllocatedSoundLink(allocated_sound_t *snd)
{
    snd->prev = NULL;

    snd->next = allocated_sounds_head;
    allocated_sounds_head = snd;

    if (allocated_sounds_tail == NULL)
    {
        allocated_sounds_tail = snd;
    }
    else
    {
        snd->next->prev = snd;
    }
}

// Unlink a sound from the linked list.

static void AllocatedSoundUnlink(allocated_sound_t *snd)
{
    if (snd->prev == NULL)
    {
        allocated_sounds_head = snd->next;
    }
    else
    {
        snd->prev->next = snd->next;
    }

    if (snd->next == NULL)
    {
        allocated_sounds_tail = snd->prev;
    }
    else
    {
        snd->next->prev = snd->prev;
    }
}



static void FreeAllocatedSound(allocated_sound_t *snd)
{
	// D�connecter de la liste cha�n�e.
    AllocatedSoundUnlink(snd);

    // Supprimer le lien avec les donn�es haut niveau.
    snd->sfxinfo->driver_data = NULL;

    // R�duire la taille totale utilis�e par les sons allou�s.
    allocated_sounds_size -= snd->chunk.alen;


    // Lib�rer la structure.
    free(snd);
}

// Search from the tail backwards along the allocated sounds list, find
// and free a sound that is not in use, to free up memory.  Return doom_true
// for success.

static boolean_doom FindAndFreeSound(void)
{
    allocated_sound_t *snd;

    snd = allocated_sounds_tail;

    while (snd != NULL)
    {
        if (snd->use_count == 0)
        {
            FreeAllocatedSound(snd);
            return doom_true;
        }

        snd = snd->prev;
    }

    // No available sounds to free...

    return doom_false;
}

// Enforce SFX cache size limit.  We are just about to allocate "len"
// bytes on the heap for a new sound effect, so free up some space
// so that we keep allocated_sounds_size < snd_cachesize

static void ReserveCacheSpace(size_t len)
{
    if (snd_cachesize <= 0)
    {
        return;
    }

    // Keep freeing sound effects that aren't currently being played,
    // until there is enough space for the new sound.

    while (allocated_sounds_size + len > snd_cachesize)
    {
        // Free a sound.  If there is nothing more to free, stop.

        if (!FindAndFreeSound())
        {
            break;
        }
    }
}

// Allocate a block for a new sound effect.

static Mix_Chunk *AllocateSound(sfxinfo_t *sfxinfo, size_t len)
{
    allocated_sound_t *snd;

    // Keep allocated sounds within the cache size.

    ReserveCacheSpace(len);

    // Allocate the sound structure and data.  The data will immediately
    // follow the structure, which acts as a header.

    do
    {
        snd = malloc(sizeof(allocated_sound_t) + len);

        // Out of memory?  Try to free an old sound, then loop round
        // and try again.

        if (snd == NULL && !FindAndFreeSound())
        {
            return NULL;
        }

    } while (snd == NULL);

    // Skip past the chunk structure for the audio buffer

    snd->chunk.abuf = (byte *) (snd + 1);
    snd->chunk.alen = len;
    snd->chunk.allocated = 1;
    snd->chunk.volume = MIX_MAX_VOLUME;

    snd->sfxinfo = sfxinfo;
    snd->use_count = 0;

    // driver_data pointer points to the allocated_sound structure.

    sfxinfo->driver_data = snd;

    // Keep track of how much memory all these cached sounds are using...

    allocated_sounds_size += len;

    AllocatedSoundLink(snd);

    return &snd->chunk;
}
// Lock a sound, to indicate that it may not be freed.

static void LockAllocatedSound(allocated_sound_t *snd)
{
    // Increase use count, to stop the sound being freed.

    ++snd->use_count;

    //printf("++ %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);

    // When we use a sound, re-link it into the list at the head, so
    // that the oldest sounds fall to the end of the list for freeing.

    AllocatedSoundUnlink(snd);
    AllocatedSoundLink(snd);
}

// Unlock a sound to indicate that it may now be freed.

static void UnlockAllocatedSound(allocated_sound_t *snd)
{
    if (snd->use_count <= 0)
    {
        I_Error("Sound effect released more times than it was locked...");
    }

    --snd->use_count;

    //printf("-- %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);
}

// When a sound stops, check if it is still playing.  If it is not, 
// we can mark the sound data as CACHE to be freed back for other
// means.

static void ReleaseSoundOnChannel(int channel)
{
    sfxinfo_t *sfxinfo = channels_playing[channel];

    if (sfxinfo == NULL)
    {
        return;
    }

    channels_playing[channel] = NULL;

    UnlockAllocatedSound(sfxinfo->driver_data);
}


static boolean_doom ConvertibleRatio(int freq1, int freq2)
{
    int ratio;

    if (freq1 > freq2)
    {
        return ConvertibleRatio(freq2, freq1);
    }
    else if ((freq2 % freq1) != 0)
    {
        // Not in a direct ratio

        return doom_false;
    }
    else
    {
        // Check the ratio is a power of 2

        ratio = freq2 / freq1;

        while ((ratio & 1) == 0)
        {
            ratio = ratio >> 1;
        }

        return ratio == 1;
    }
}


static boolean_doom ExpandSoundData_SDL(sfxinfo_t *sfxinfo,
                                    byte *data,
                                    int samplerate,
                                    int length)
{
    //SDL_AudioCVT convertor;
    Mix_Chunk *chunk;
    uint32_t expanded_length;

    // Calculate the length of the expanded version of the sample.

    expanded_length = (uint32_t) ((((uint64_t) length) * mixer_freq) / samplerate);

    // Double up twice: 8 -> 16 bit and mono -> stereo

    expanded_length *= 4;

    // Allocate a chunk in which to expand the sound

    chunk = AllocateSound(sfxinfo, expanded_length);

    if (chunk == NULL)
    {
        return doom_false;
    }

    // If we can, use the standard / optimized SDL conversion routines.
        Sint16 *expanded = (Sint16 *) chunk->abuf;
        int expand_ratio;
        int i;

        // Generic expansion if conversion does not work:
        //
        // SDL's audio conversion only works for rate conversions that are
        // powers of 2; if the two formats are not in a direct power of 2
        // ratio, do this naive conversion instead.

        // number of samples in the converted sound

        expanded_length = ((uint64_t) length * mixer_freq) / samplerate;
        expand_ratio = (length << 8) / expanded_length;

        for (i=0; i<expanded_length; ++i)
        {
            Sint16 sample;
            int src;

            src = (i * expand_ratio) >> 8;

            sample = data[src] | (data[src] << 8);
            sample -= 32768;

            // expand 8->16 bits, mono->stereo

            expanded[i * 2] = expanded[i * 2 + 1] = sample;
        }


    return doom_true;
}

// Load and convert a sound effect
// Returns doom_true if successful

static boolean_doom CacheSFX(sfxinfo_t *sfxinfo)
{
    int lumpnum;
    unsigned int lumplen;
    int samplerate;
    unsigned int length;
    byte *data;

    // need to load the sound

    lumpnum = sfxinfo->lumpnum;
    data = W_CacheLumpNum(lumpnum, PU_STATIC);
    lumplen = W_LumpLength(lumpnum);

    // Check the header, and ensure this is a valid sound

    if (lumplen < 8
     || data[0] != 0x03 || data[1] != 0x00)
    {
        // Invalid sound

        return doom_false;
    }

    // 16 bit sample rate field, 32 bit length field

    samplerate = (data[3] << 8) | data[2];
    length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

    // If the header specifies that the length of the sound is greater than
    // the length of the lump itself, this is an invalid sound lump

    // We also discard sound lumps that are less than 49 samples long,
    // as this is how DMX behaves - although the actual cut-off length
    // seems to vary slightly depending on the sample rate.  This needs
    // further investigation to better understand the correct
    // behavior.

    if (length > lumplen - 8 || length <= 48)
    {
        return doom_false;
    }

    // The DMX sound library seems to skip the first 16 and last 16
    // bytes of the lump - reason unknown.

    data += 16;
    length -= 32;

    // Sample rate conversion

    if (!ExpandSoundData(sfxinfo, data + 8, samplerate, length))
    {
        return doom_false;
    }


    // don't need the original lump any more
  
    W_ReleaseLumpNum(lumpnum);

    return doom_true;
}

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    // Linked sfx lumps? Get the lump number for the sound linked to.

    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    // Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't
    // do this.

    if (use_sfx_prefix)
    {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    }
    else
    {
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
    }
}


static void I_SDL_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    // no-op
}


// Load a SFX chunk into memory and ensure that it is locked.

static boolean_doom LockSound(sfxinfo_t *sfxinfo)
{
    // If the sound isn't loaded, load it now

    if (sfxinfo->driver_data == NULL)
    {
        if (!CacheSFX(sfxinfo))
        {
            return doom_false;
        }
    }

    LockAllocatedSound(sfxinfo->driver_data);

    return doom_true;
}

// Initialize the channel data
void init_sound_system() {
    memset(channels, 0, sizeof(channels));
}

// Play a sound on a channel
void play_sound(u8 *data, int length, int channel, int vol) {
    // Check if the channel is valid
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return;
    }

    channels[channel].data = data;
    channels[channel].length = length;
    channels[channel].position = 0;
    channels[channel].active = 1;
    channels[channel].vol = vol;
}


void mix_sounds(u32 *output_buffer) {
    // Clear the output buffer
    memset(output_buffer, 0, NR_AUDIO_SAMPLES * sizeof(u32));
    int32_t mixed_sample[NR_AUDIO_SAMPLES] = { 0 };

    // Mix all active channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channels[i].active) {
            for (int j = 0; j < NR_AUDIO_SAMPLES; j++) {
                if (channels[i].position < channels[i].length) {
                    int16_t sample = (int16_t)((channels[i].data[channels[i].position + 1] << 8) | channels[i].data[channels[i].position]);
                    mixed_sample[j] += (sample * channels[i].vol) / MIX_MAX_VOLUME;
                    channels[i].position += 4;//data is 16-bit stereo, so we increment by 4
                } else {
                    channels[i].active = 0;
                    break;
                }
            }

        }
    }

    // Clamp and convert the mixed samples to 32-bit unsigned integers
    for (int i = 0; i < NR_AUDIO_SAMPLES; i++) {

    	//if music is playing, we add the music to the sound
    	if(current_track_music)
    	{
    		mixed_sample[i] += (int32_t)(playBuffer[i + currentLen]);
    	}

        // Clamp the sample to the range of an int16_t
        if (mixed_sample[i] > INT16_MAX) {
            mixed_sample[i] = INT16_MAX;
        } else if (mixed_sample[i] < INT16_MIN) {
            mixed_sample[i] = INT16_MIN;
        }

        // Convert the sample to a 32-bit unsigned integer
        output_buffer[i] = (u32)(((int16_t)mixed_sample[i] + 32768) * 4);
    }
}

void loadNextBuffer()
{
    //load current buffer
	fnAudioRecordFromMemory(sAxiDma, currentBuffer, NR_AUDIO_SAMPLES);
    //play current buffer
	fnAudioPlayFromMemory(sAxiDma, NR_AUDIO_SAMPLES);
    //current length is incremented by the number of samples played
	currentLen += NR_AUDIO_SAMPLES;
	if(currentLen < playBufferLen){
        //load next buffer with the next samples
		mix_sounds(nextBuffer);

        //current buffer is now the next buffer for next iteration
		u32* temp = currentBuffer;
		currentBuffer = nextBuffer;
		nextBuffer = temp;
	}
}

void loadMusic()
{
    //reset current length to 0
	currentLen = 0;
    //load the first buffer
	mix_sounds(currentBuffer);
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//

static int I_SDL_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

    return W_GetNumForName(namebuf);
}

static void I_SDL_UpdateSoundParams(int handle, int vol, int sep)
{
    int left, right;

    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    // Calcul du volume pour le canal gauche et droit
    left = ((254 - sep) * vol) / 127;
    right = ((sep) * vol) / 127;

    if (left < 0) left = 0;
    else if ( left > 255) left = 255;
    if (right < 0) right = 0;
    else if (right > 255) right = 255;


    //Mix_SetPanning(handle, left, right);
    channel_left_volume[handle] = (u8)(left);
    channel_right_volume[handle] = (u8)(right);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//

static int I_SDL_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    allocated_sound_t *snd;

    if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
    {
        return -1;
    }

    // Release a sound effect if there is already one playing
    // on this channel

    ReleaseSoundOnChannel(channel);

    // Get the sound data

    if (!LockSound(sfxinfo))
    {
	return -1;
    }

    snd = sfxinfo->driver_data;

    // play sound

    // The sound is now playing on this channel
    play_sound(snd->chunk.abuf, snd->chunk.alen, channel, vol);

    //not required if sound is defined as mono
    //I_SDL_UpdateSoundParams(channel, vol, sep);


    return channel;
}

static void I_SDL_StopSound(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }


    // Sound data is no longer needed; release the
    // sound data being used for this channel

    ReleaseSoundOnChannel(handle);
}

static boolean_doom I_SDL_SoundIsPlaying(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return doom_false;
    }

    //Probably useless
    if(channels_playing[handle] != NULL) return 0;
    else return 1;
}
// 
// Periodically called to update the sound system
// Couln't be called directly from the interrupt in dma.c since
// it would be too long and would cause the program to crash
// 

static void I_SDL_UpdateSound(void)
{
    int i;
    //Demo.fAudioPlayback is a flag that indicates if the audio is playing
    //It is managed in dma.c and is set to 0 when a dma transfer is finished
    if(!Demo.fAudioPlayback){
        //stop audio
    	Xil_Out32(I2S_STREAM_CONTROL_REG, 0x00000000);
    	Xil_Out32(I2S_TRANSFER_CONTROL_REG, 0x00000000);
        //if music is playing, we load the next buffer
    	if (currentLen < playBufferLen){
    		loadNextBuffer();
    	}
        //if music is not playing, we load the music
    	else {
    		loadMusic();
    		loadNextBuffer();
    	}
        //indicate that audio is playing
    	Demo.fAudioPlayback = 1;
    }

}

static void I_SDL_ShutdownSound(void)
{    
	if (!sound_initialized)
	{
	    return;
	}

	//Mix_CloseAudio();
	//SDL_QuitSubSystem(SDL_INIT_AUDIO);


	//Stop audio and reinitialize sound system
	Xil_Out32(I2S_STREAM_CONTROL_REG, 0x00000000);
	Xil_Out32(I2S_TRANSFER_CONTROL_REG, 0x00000000);
	fnDemoInit();

	sound_initialized = doom_false;
}

// Calculate slice size, based on snd_maxslicetime_ms.
// The result must be a power of two.

static int GetSliceSize(void)
{
    int limit;
    int n;

    limit = (snd_samplerate * snd_maxslicetime_ms) / 1000;

    // Try all powers of two, not exceeding the limit.

    for (n=0;; ++n)
    {
        // 2^n <= limit < 2^n+1 ?

        if ((1 << (n + 1)) > limit)
        {
            return (1 << n);
        }
    }

    // Should never happen?

    return 1024;
}



static boolean_doom I_SDL_InitSound(boolean_doom _use_sfx_prefix)
{
    int i;

    use_sfx_prefix = _use_sfx_prefix;

    // R�initialisez tous les canaux.
    for (i = 0; i < NUM_CHANNELS; ++i)
    {
        channels_playing[i] = NULL;
    }

    // Initialisez le codec audio.
    if(fnDemoInit() != XST_SUCCESS)
    {
    	xil_printf("Unable to set up sound\n\r");
    	return doom_false;
    }

    ExpandSoundData = ExpandSoundData_SDL;

    //Mix_QuerySpec(&mixer_freq, &mixer_format, &mixer_channels);
    //Certainly useless because managed with bare-metal
    mixer_freq = MIX_DEFAULT_FREQUENCY;
    mixer_channels = MIX_DEFAULT_CHANNELS;

    //Mix_AllocateChannels(NUM_CHANNELS);
    init_sound_system();

    //SDL_PauseAudio(0);

    sound_initialized = doom_true;

    return doom_true;
}





static snddevice_t sound_sdl_devices[] = 
{
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module = 
{
    sound_sdl_devices,
    arrlen(sound_sdl_devices),
    I_SDL_InitSound,
    I_SDL_ShutdownSound,
    I_SDL_GetSfxLumpNum,
    I_SDL_UpdateSound,
    I_SDL_UpdateSoundParams,
    I_SDL_StartSound,
    I_SDL_StopSound,
    I_SDL_SoundIsPlaying,
    I_SDL_PrecacheSounds,
};

