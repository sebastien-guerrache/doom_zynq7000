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
//	System interface for music.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include "../audio/audio.h"
#include "../demo/demo.h"


#define TSF_IMPLEMENTATION
#include "../tinymidipcm/tsf.h"

#define TML_IMPLEMENTATION
#include "../tinymidipcm/tml.h"


#include "config.h"
#include "doomtype.h"
#include "memio.h"
#include "mus2mid.h"

#include "deh_str.h"
#include "gusconf.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "sha1.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAXMIDLENGTH (96 * 1024)
#define MID_HEADER_MAGIC "MThd"
#define MUS_HEADER_MAGIC "MUS\x1a"

#define FLAC_HEADER "fLaC"
#define OGG_HEADER "OggS"

// Looping Vorbis metadata tag names. These have been defined by ZDoom
// for specifying the start and end positions for looping music tracks
// in .ogg and .flac files.
// More information is here: http://zdoom.org/wiki/Audio_loop
#define LOOP_START_TAG "LOOP_START"
#define LOOP_END_TAG   "LOOP_END"

// FLAC metadata headers that we care about.
#define FLAC_STREAMINFO      0
#define FLAC_VORBIS_COMMENT  4

// Ogg metadata headers that we care about.
#define OGG_ID_HEADER        1
#define OGG_COMMENT_HEADER   3

#define MIX_MAX_VOLUME          127 /* Volume of a chunk */

// Structure for music substitution.
// We store a mapping based on SHA1 checksum -> filename of substitute music
// file to play, so that substitution occurs based on content rather than
// lump name. This has some inherent advantages:
//  * Music for Plutonia (reused from Doom 1) works automatically.
//  * If a PWAD replaces music, the replacement music is used rather than
//    the substitute music for the IWAD.
//  * If a PWAD reuses music from an IWAD (even from a different game), we get
//    the high quality version of the music automatically (neat!)

typedef struct
{
    sha1_digest_t hash;
    char *filename;
} subst_music_t;

// Structure containing parsed metadata read from a digital music track:
typedef struct
{
    boolean_doom valid;
    unsigned int samplerate_hz;
    int start_time, end_time;
} file_metadata_t;

//static subst_music_t *subst_music = NULL;
//static unsigned int subst_music_len = 0;

//static const char *subst_config_filenames[] =
//{
//    "doom1-music.cfg",
//    "doom2-music.cfg",
//    "tnt-music.cfg",
//    "heretic-music.cfg",
//    "hexen-music.cfg",
//    "strife-music.cfg",
//};

static boolean_doom music_initialized = doom_false;

// If this is doom_true, this module initialized SDL sound and has the
// responsibility to shut it down

static boolean_doom musicpaused = doom_false;
static int current_music_volume;
static int new_music_volume;

char *timidity_cfg_path = "";

//static char *temp_timidity_cfg = NULL;

// If doom_true, we are playing a substitute digital track rather than in-WAD
// MIDI/MUS track, and file_metadata contains loop metadata.
static boolean_doom playing_substitute = doom_false;

// Currently playing music track.
boolean_doom current_track_music = doom_false;

// If doom_true, the currently playing track is being played on loop.
static boolean_doom current_track_loop;

// Buffer for the currently playing music track declared in doomgeneric_zyboz7.c
extern int16_t playBuffer[BUFFER_MUSIC_SIZE];
extern u32 playBufferLen;
extern u32 currentLen;


static tml_message *midi_render(tsf *soundfont, tml_message *midi_message,
                                int channels, int sample_rate,
                                uint8_t *buffer, int pcm_length, float *msecs) {
    const int block_size = TSF_RENDER_EFFECTSAMPLEBLOCK; // Use const for unchanging values
    int bytes_written = 0;
    const float ms_per_sample = 1000.0f / (float)sample_rate; // Precompute this constant

    




    while (bytes_written < pcm_length) {
        // Update the elapsed milliseconds for the current block
        *msecs += block_size * ms_per_sample;

        // Process all MIDI messages scheduled up to the current time
        while (midi_message && *msecs >= midi_message->time) {
            switch (midi_message->type) {
                case TML_PROGRAM_CHANGE:
                    tsf_channel_set_presetnumber(
                        soundfont, midi_message->channel, midi_message->program,
                        (midi_message->channel == 9)); // Use percussion bank for channel 9
                    break;

                case TML_NOTE_ON:
                    tsf_channel_note_on(
                        soundfont, midi_message->channel, midi_message->key,
                        midi_message->velocity / 127.0f); // Normalize velocity
                    break;

                case TML_NOTE_OFF:
                    tsf_channel_note_off(soundfont, midi_message->channel,
                                         midi_message->key);
                    break;

                case TML_PITCH_BEND:
                    tsf_channel_set_pitchwheel(soundfont, midi_message->channel,
                                               midi_message->pitch_bend);
                    break;

                case TML_CONTROL_CHANGE:
                    tsf_channel_midi_control(
                        soundfont, midi_message->channel, midi_message->control,
                        midi_message->control_value);
                    break;

                default:
                    // Ignore unrecognized message types
                    break;
            }

            // Move to the next MIDI message
            midi_message = midi_message->next;
        }

        // Render the audio block to the buffer
        tsf_render_short(soundfont, (int16_t *)(buffer + bytes_written),
                         block_size, 0);

        // Update the bytes written counter
        bytes_written += block_size * 2 * channels;
    }



    return midi_message; // Return the next unprocessed MIDI message
}


#define SAMPLE_RATE MIX_DEFAULT_FREQUENCY //link to i_sdlsound.c
#define CHANNELS MIX_DEFAULT_CHANNELS

//sf2 file converted to c array
extern unsigned char scc1t2_sf2[];
extern unsigned int scc1t2_sf2_len;





//receive the midi to convert and restitute the pcm value
static boolean_doom midi2pcm(uint8_t *midi_buffer, int midi_length) {

    tsf *soundfont = tsf_load_memory(scc1t2_sf2, scc1t2_sf2_len);
    if (!soundfont) {
        return doom_false;
    }

    tsf_set_output(soundfont, TSF_MONO, SAMPLE_RATE, 0.0f);

    tml_message *midi_message = tml_load_memory(midi_buffer, midi_length);
    if (!midi_message) {
        tsf_close(soundfont);
        return doom_false;
    }

    
    float msecs = 0;
    int samples = 4 * SAMPLE_RATE * CHANNELS;

    uint8_t *pcm_buffer = malloc(samples);
    if (!pcm_buffer) {
        tml_free(midi_message);
        tsf_close(soundfont);
        return doom_false;
    }

    // Length of the play buffer is set to 0 -> start of the buffer
    playBufferLen = 0;

    do {
        midi_message = midi_render(soundfont, midi_message,
                                   CHANNELS, SAMPLE_RATE,
                                   pcm_buffer, samples, &msecs);
        //load buffer here

        for (int i = 0; i < samples; i += 2) // Process two bytes at once
        {
            // Directly combine two bytes into a 16-bit signed integer
            playBuffer[playBufferLen++] = (int16_t)((pcm_buffer[i + 1] << 8) | pcm_buffer[i]);
        }

    } while (midi_message != NULL);

    //Current length of the play buffer is set to 0 to start from the beginning
    currentLen = 0;

    //clean up
    free(pcm_buffer);
    tml_free(midi_message);
    tsf_close(soundfont);

    //program finished successfully
    return doom_true;
}





// Shutdown music => managed by i_sdlsound.c in our case

static void I_SDL_ShutdownMusic(void)
{
    if (music_initialized)
    {
        //Mix_HaltMusic();
        music_initialized = doom_false;

//        if (sdl_was_initialized)
//        {
//            Mix_CloseAudio();
//            SDL_QuitSubSystem(SDL_INIT_AUDIO);
//            sdl_was_initialized = doom_false;
//        }
    }
}


// Callback function that is invoked to track current track position.
//static void TrackPositionCallback(int chan, void *stream, int len, void *udata)
//{
//    // Position is doubled up twice: for 16-bit samples and for stereo.
//    current_track_pos += len / 4;
//}

// Initialize music subsystem
static boolean_doom I_SDL_InitMusic(void)
{
    music_initialized = doom_true;

    return music_initialized;
}

//
// SDL_mixer's native MIDI music playing does not pause properly.
// As a workaround, set the volume to 0 when paused.
//

static void UpdateMusicVolume(void)
{
    int vol;

    if (musicpaused)
    {
        vol = 0;
    }
    else
    {
        vol = (current_music_volume * MIX_MAX_VOLUME) / 127;
    }


    //Mix_VolumeMusic(vol);
    //the volume to be applied is now contained by new_music_volume
    new_music_volume = vol;
}

// Set music volume (0 - 127)

static void I_SDL_SetMusicVolume(int volume)
{
    // Internal state variable.
    current_music_volume = volume;

    UpdateMusicVolume();
}

// Start playing a song

static void I_SDL_PlaySong(void *handle, boolean_doom looping)
{

    if (!music_initialized)
    {
        return;
    }

    if (handle == NULL)
    {
        return;
    }

    current_track_music = doom_true;
    current_track_loop = looping;

    // Act like we're not playing audio anymore
    Demo.fAudioPlayback = 0;


    // Don't loop when playing substitute music, as we do it
    // ourselves instead.

    //Mix_PlayMusic(current_track_music, loops);
}

static void I_SDL_PauseSong(void)
{
    if (!music_initialized)
    {
        return;
    }

    musicpaused = doom_true;

    UpdateMusicVolume();
}

static void I_SDL_ResumeSong(void)
{
    if (!music_initialized)
    {
        return;
    }

    musicpaused = doom_false;

    UpdateMusicVolume();
}

static void I_SDL_StopSong(void)
{
    if (!music_initialized)
    {
        return;
    }

    //Mix_HaltMusic();
    playing_substitute = doom_false;
    current_track_music = doom_false;
}

static void I_SDL_UnRegisterSong(void *handle)
{
    //Mix_Music *music = (Mix_Music *) handle;

    if (!music_initialized)
    {
        return;
    }

    if (handle == NULL)
    {
        return;
    }


    //memset(playBuffer, 0, playbufferLen * sizeof(u32));
    //Mix_FreeMusic(music);
}

//converts MUS to MIDI

static boolean_doom ConvertMus(byte *musdata, int len, uint8_t **midi_buffer, int *midi_length)
{
    MEMFILE *instream;
    MEMFILE *outstream;
    void *outbuf;
    size_t outbuf_len;
    int result;

    instream = mem_fopen_read(musdata, len);
    outstream = mem_fopen_write();

    result = mus2mid(instream, outstream); // Returns 0 on success, 1 on failure

    if (result == 0) // Success
    {
        mem_get_buf(outstream, &outbuf, &outbuf_len);

        *midi_buffer = malloc(outbuf_len);
        if (*midi_buffer == NULL)
        {
            result = 1; // Allocation failure
        }
        else
        {
            // Copy the MIDI data to the output buffer
            memcpy(*midi_buffer, outbuf, outbuf_len);
            *midi_length = (int)outbuf_len;
        }
    }

    mem_fclose(instream);
    mem_fclose(outstream);

    return result;
}

//register a song to be played
static void *I_SDL_RegisterSong(void *data, int len)
{
    uint8_t *midi_buffer = NULL; // MIDI buffer
    int midi_length = 0; // Length of MIDI buffer

    // Stop audio and reinitialize sound system
    Xil_Out32(I2S_STREAM_CONTROL_REG, 0x00000000);
    Xil_Out32(I2S_TRANSFER_CONTROL_REG, 0x00000000);

    // Act like we're playing audio
    Demo.fAudioPlayback = 1;

    if (!music_initialized)
    {
        return NULL;
    }

    //playing_substitute = doom_false;

    // Convert MUS to MIDI and handle errors
    if (ConvertMus(data, len, &midi_buffer, &midi_length) != 0)
    {
        return NULL;
    }

    // Convert MIDI to PCM
    if (!midi2pcm(midi_buffer, midi_length))
    {
        free(midi_buffer);
        return NULL;
    }

    // Clean up
    free(midi_buffer);
    // Return something that isn't NULL
    return playBuffer;
}

// Is the song playing?
static int music_is_not_playing = -1;

static boolean_doom I_SDL_MusicIsPlaying(void)
{
    if (!music_initialized)
    {
        return doom_false;
    }

    if(!Demo.fAudioPlayback)
    {
    	music_is_not_playing++;

    	//music_is_not_playign can be true only if
    	//Demo.fAudioPlayback is false twice in a row
    	if(music_is_not_playing)
    	{
    		//if music_is_not_playing is true
    		return 0;
    	}
    	//otherwise it means it's playing
    	else return 1;
    }
    else
    {
    	music_is_not_playing = -1;
    	return 1;
    }

    //return Mix_PlayingMusic();
}



// Poll music position; if we have passed the loop point end position
// then we need to go back.
static void I_SDL_PollMusic(void)
{
    //not implemented
}

static snddevice_t music_sdl_devices[] =
{
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_GENMIDI,
    SNDDEVICE_AWE32,
};

music_module_t DG_music_module =
{
    music_sdl_devices,
    arrlen(music_sdl_devices),
    I_SDL_InitMusic,
    I_SDL_ShutdownMusic,
    I_SDL_SetMusicVolume,
    I_SDL_PauseSong,
    I_SDL_ResumeSong,
    I_SDL_RegisterSong,
    I_SDL_UnRegisterSong,
    I_SDL_PlaySong,
    I_SDL_StopSong,
    I_SDL_MusicIsPlaying,
    I_SDL_PollMusic,
};

