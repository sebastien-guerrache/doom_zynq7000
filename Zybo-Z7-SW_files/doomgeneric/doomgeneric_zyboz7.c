// doomgeneric for Zybo Z7 board with HDMI output video and Digilent Keypad on PMOD C

#include <unistd.h>
#include <stdbool.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h"

// Include for Port on Zybo Z7
#include "../video_demo.h"
#include "../timer_ps/timer_ps.h"
#include "../dma/dma.h"
#include "../demo/demo.h"
#include "../audio/audio.h"
#include "i_sound.h"
#include "doomfeatures.h"



//#define TSF_IMPLEMENTATION
//#include "../tinymidipcm/tsf.h"
//
//#define TML_IMPLEMENTATION
//#include "../tinymidipcm/tml.h"


#define GLOBAL_TIMER_BASEADDR  0xF8F00200
#define GLOBAL_TIMER_COUNTER0  (GLOBAL_TIMER_BASEADDR + 0x00)
#define GLOBAL_TIMER_COUNTER1  (GLOBAL_TIMER_BASEADDR + 0x04)


// Variables for video controller
extern DisplayCtrl dispCtrl;
u32 frameToRender = 2;
u32 renderedFrame = 1;

// Variables for board GPIO
extern XGpio input_btn_sw, input_row, output_led_col;
extern XAxiDma sAxiDma;


// Variables for audio controller
//buffer for audio samples -> 2 buffers for ping-pong
//aligned to 32 bytes for DMA transfer
static u32 bufferA[NR_AUDIO_SAMPLES] __attribute__((aligned(32)));
static u32 bufferB[NR_AUDIO_SAMPLES] __attribute__((aligned(32)));
u32* currentBuffer = bufferA;
u32* nextBuffer = bufferB;

//music is stored in this buffer
int16_t playBuffer[BUFFER_MUSIC_SIZE] = { 0 };// 13230000 = 44100 * 300 secondes -> 5 minutes of music
u32 playBufferLen;//length of the playBuffer
u32 currentLen;//current length of the playBuffer



// Variable to store the initial tick count for timing purposes
static u64 start_ticks = 0;



//read the global timer
u64 ReadGlobalTimer() {
    u32 lower = Xil_In32(GLOBAL_TIMER_COUNTER0);
    u32 upper = Xil_In32(GLOBAL_TIMER_COUNTER1);
    return ((u64)upper << 32) | lower;
}

// Initialiser le point de référence
void InitTicks() {
    start_ticks = ReadGlobalTimer();
}

//get the current tick count in ms
u32 GlobalTimer_GetTick() {
    u64 current_ticks = ReadGlobalTimer();
    return (u32)(((current_ticks - start_ticks) * 1000) / TIMER_FREQ_HZ);
}

//get the current tick count in us -> for testing purposes
u64 GlobalTimer_GetTick_us() {
    u64 current_ticks = ReadGlobalTimer();
    return ((current_ticks - start_ticks) * 1000000) / TIMER_FREQ_HZ;
}

// **************************************************************** //
// 							Doom Keys								//
// **************************************************************** //

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;



//20 keys exist -> 16 keys on pmod and 4 buttons on the board
static u8 buffer_keys_pressed[20] = { 0 };		//contains the keys currently pressed on pmod and buttons
static u8 buffer_last_keys_pressed[20] = { 0 }; //contains the keys pressed on the previous iteration

/*
 * The PmodKYPD utilizes 4 rows and columns to create an array of 16 momentary pushbuttons.
 * By driving the column lines to a logic level low voltage one at a time, users may read the 
 * corresponding logic level voltage on each of the rows to determine which button, if any, 
 * is currently being pressed. Simultaneous button presses can also be recorded, although it 
 * is still required to step through each row and column separately in order to ensure that 
 * the pressed buttons do not interfere with each measurement.
 */

static void checkPMOD(void)
{
	u8 pmod_pressed = 0;
	u8 readRow; //variable to store the value of the row
	u8 val[6] = { 0xE, 0xD, 0xB, 0x7, 0x3, 0x9 };
	//0xE = 1110 -> it means that the first column is at 0
	//0xD = 1101 -> it means that the second column is at 0
	//0x3 = 0011 -> it means that the two last columns are at 0
	u8 PMOD[4][4] = {{ 0x1, 0x2, 0x3, 0xA },
	                 { 0x4, 0x5, 0x6, 0xB },
	                 { 0x7, 0x8, 0x9, 0xC },
	                 { 0x0, 0xF, 0xE, 0xD }};

    for(u8 i = 0; i < 4; i++)
    {
		//a column is set to 0
		XGpio_DiscreteWrite(&output_led_col, 2, val[i]);
		//we read the value of the row
		readRow = XGpio_DiscreteRead(&input_row, 1);
		//we check which key is pressed
		for(u8 j = 0; j < 4; j++)
		{
			//if the key is equal to 0, it means that the key is pressed
			if (!(readRow & (1 << j)))
			{
				//we store the key pressed in the keys_pressed array
				buffer_keys_pressed[PMOD[j][i]] = 1;
				pmod_pressed = 1;

			}
		}
    }

	//The following check is specific to a bug in the pmod keypad
	//If two keys are pressed on the same row, the pmod will not detect the second key
	//To fix this, we set two columns to 0 and check if the row is at 0
	//If it is the case, we know which keys are pressed, otherwise the
	//row would be at 1

    if(!pmod_pressed) {
       	XGpio_DiscreteWrite(&output_led_col, 2, val[4]);
       	readRow = XGpio_DiscreteRead(&input_row, 1);
       	if (!(readRow & 0x4))
       	{
       		buffer_keys_pressed[0xC] = 1;
       		buffer_keys_pressed[0x9] = 1;
       	}
       	XGpio_DiscreteWrite(&output_led_col, 2, val[5]);
       	readRow = XGpio_DiscreteRead(&input_row, 1);
       	if (!(readRow & 0x4))
       	{
       		buffer_keys_pressed[0x8] = 1;
       	   	buffer_keys_pressed[0x9] = 1;
      	}
	}
}



static unsigned char convertToDoomKey(unsigned int key){

	switch (key)
	{
		//case 0x1 : return key_prevweapon; -> declared in m_controls.c l.130
		//case 0x4 : return key_nextweapon; -> declared in m_controls.c l.131
		case 0x2   : return KEY_RALT;		//strafe when pressed with left/right arrow
		case 0xA   : return KEY_FIRE;		//fire
		case 0xE   : return KEY_UPARROW;	//up
		case 0xC   : return KEY_LEFTARROW;	//left
		case 0x9   : return KEY_DOWNARROW;	//down
		case 0x8   : return KEY_RIGHTARROW;	//right
		case 0x3   : return KEY_RSHIFT;		//run
		case 0xB   : return KEY_USE;		//use
		case 0x0   : return KEY_ENTER;		//enter
		case 0x7   : return KEY_ESCAPE;		//escape
		case 16	   : return KEY_F2;			//save game
		case 17    : return KEY_F5;			//change details
		case 18    : return KEY_F8;			//messages on/off
		case 19    : return KEY_F11;		//change gamma
		default    : return tolower(key);	//default
	}
}

static void addKeyToQueue(int pressed, u8 keyCode){
	u8 key = convertToDoomKey(keyCode);

	u16 keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void updateKeys(void)
{
	//we check if the keys pressed are different from the last iteration
	for(u8 i = 0; i < 20; i++)
	{
		if(buffer_keys_pressed[i] != buffer_last_keys_pressed[i])
		{
			if(buffer_keys_pressed[i]) addKeyToQueue(1, i);
			else addKeyToQueue(0, i);
			buffer_last_keys_pressed[i] = buffer_keys_pressed[i];
		}
	}
}

static void handleKeyInput(){
	u8 i, button_states;
	u8 j = 0;

	//we set the buffer to 0, suppose that no key is pressed
	for(i = 0; i < 20; i++) buffer_keys_pressed[i] = 0;


	//we read the state of the buttons
	button_states = XGpio_DiscreteRead(&input_btn_sw, 1);
	//we check the keys pressed on the pmod
	checkPMOD();

	//if a button on the board is pressed
    if(button_states != 0x0)
    {
    	while (button_states >>= 1) j++;
    	buffer_keys_pressed[16 + j] = 1;
    }


	//update the keys
	updateKeys();
}

// *************************************************************** //

/* -------------------------- */
/* DOOM FUNCTIONS DEFINITIONS */
/* -------------------------- */

void DG_Init(){
	InitTicks();  		// Init Global Timer
	DemoInitialize(); 	// Initialize the video controller
}

void DG_DrawFrame()
{
	DisplayChangeFrame(&dispCtrl, renderedFrame);

	frameToRender++;
	if (frameToRender >= DISPLAY_NUM_FRAMES)
	{
		frameToRender = 1;
	}
	renderedFrame++;
	if (renderedFrame >= DISPLAY_NUM_FRAMES)
	{
		renderedFrame = 1;
	}

	handleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{
	TimerDelay(1000 * ms);
}

uint32_t DG_GetTicksMs()
{
	return GlobalTimer_GetTick();
}

int DG_GetKey(int* pressed, unsigned char* doomKey){

	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex){
		//key queue is empty
		return 0;
	}else{
		unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;

		return 1;
	}

	return 0;
}

void DG_SetWindowTitle(const char * title){}



int main(void)
{
    doomgeneric_Create(0, "");

    while(1)
    {
		doomgeneric_Tick();
    }

	return 0;
}
