/************************************************************************/
/*																		*/
/*	demo.c	--	Zybo DMA Demo				 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Lowe											*/
/*	Copyright 2015, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		DMA audio inputs and outputs on the Zybo.					*/
/*																		*/
/*																		*/
/************************************************************************/
/*  Notes:																*/
/*																		*/
/*		- The DMA max burst size needs to be set to 16 or less			*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		9/6/2016(SamL): Created										*/
/*																		*/
/************************************************************************/


#include "demo.h"




#include "../audio/audio.h"
#include "../dma/dma.h"
#include "../intc/intc.h"
//#include "../userio/userio.h"
#include "../iic/iic.h"

/***************************** Include Files *********************************/

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "xiic.h"
#include "xaxidma.h"
#include "xtime_l.h"

#include "../video_demo.h"
#include "../video_capture/video_capture.h"
#include "../display_ctrl/display_ctrl.h"
#include "../intc/intc.h"
#include <stdio.h>
#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "../timer_ps/timer_ps.h"
//#include "xparameters.h"

#include "xil_cache.h"


#ifdef XPAR_INTC_0_DEVICE_ID
 #include "xintc.h"
 #include "microblaze_sleep.h"
#else
 #include "xscugic.h"
//#include "sleep.h"
#include "xil_cache.h"
#endif

/************************** Constant Definitions *****************************/

/*
 * Device hardware build related constants.
 */

// Audio constants
// Number of seconds to record/playback




/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000

#define TEST_START_VALUE	0x0

#define DYNCLK_BASEADDR 		XPAR_AXI_DYNCLK_0_S_AXI_LITE_BASEADDR
#define VDMA_ID 				XPAR_AXIVDMA_0_DEVICE_ID
#define HDMI_OUT_VTC_ID 		XPAR_V_TC_OUT_DEVICE_ID
#define HDMI_IN_VTC_ID 			XPAR_V_TC_IN_DEVICE_ID
#define HDMI_IN_GPIO_ID 		XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define HDMI_IN_VTC_IRPT_ID 	XPAR_FABRIC_V_TC_IN_IRQ_INTR
#define HDMI_IN_GPIO_IRPT_ID 	XPAR_FABRIC_AXI_GPIO_VIDEO_IP2INTC_IRPT_INTR
#define SCU_TIMER_ID 			XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR 			XPAR_PS7_UART_1_BASEADDR

/* I2S Register offsets */
#define I2S_RESET_REG 		0x00
#define I2S_CTRL_REG 		0x04
#define I2S_CLK_CTRL_REG 	0x08
#define I2S_FIFO_STS_REG 	0x20
#define I2S_RX_FIFO_REG 	0x28
#define I2S_TX_FIFO_REG 	0x2C


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/
#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif


/************************** Variable Definitions *****************************/
// This variable holds the demo related settings
volatile sDemo_t Demo;

/*
 * Device instance definitions
 */

int _unlink(const char *path) {
    return -1; // Indique une erreur
}

int _link(const char *oldpath, const char *newpath) {
    return -1; // Indique une erreur
}

DisplayCtrl dispCtrl;
XAxiVdma vdma;
VideoCapture videoCapt;//no need for volatile because the videoCapt's state is either checked just one time
					   //in functions VideoStart and VideoStop, or (in the DemoGetInactiveFrame function) the worst case scenario is
                       //losing the frame with the inverted colors of the frame gotten from the disconnected input
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect
XGpio input_btn_sw, input_row, output_led_col;
/*
 * Framebuffers for video data
 */
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__((aligned(0x20)));
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

const u16 sin_wave[] = {32768,38420,44016,49500,54819,59920,64753,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,62764,58895,55005,51141,47352,43684,40180,36878,33814,31024,28532,26362,24530,23050,21928,21164,20756,20692,20959,21536,22399,23519,24864,26398,28083,29877,31739,33624,35490,37292,38988,40538,41901,43044,43931,44535,44831,44798,44421,43690,42601,41153,39353,37213,34749,31985,28946,25664,22176,18520,14741,10882,6992,3120,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,357,5164,10242,15543,21014,26603};
const u16 sin_440[] = {32768,34654,36534,38401,40250,42074,43867,45623,47337,49002,50614,52166,53654,55073,56417,57684,58867,59965,60971,61885,62702,63419,64035,64547,64954,65254,65447,65530,65506,65372,65131,64782,64327,63768,63106,62343,61481,60525,59477,58340,57118,55816,54437,52986,51468,49888,48252,46564,44830,43056,41248,39412,37554,35680,33797,31910,30026,28151,26291,24453,22642,20865,19127,17435,15793,14208,12684,11227,9842,8532,7303,6158,5101,4137,3267,2495,1823,1254,790,431,180,37,2,76,259,549,946,1449,2056,2764,3572,4477,5475,6564,7740,8999,10337,11749,13231,14778,16384,18045,19754,21507,23297,25118,26965,28831,30710};
const u16 sin_880[] = {32768,36534,40250,43867,47337,50614,53654,56417,58867,60971,62702,64035,64954,65447,65506,65131,64327,63106,61481,59477,57118,54437,51468,48252,44830,41248,37554,33797,30026,26291,22642,19127,15793,12684,9842,7303,5101,3267,1823,790,180,2,259,946,2056,3572,5475,7740,10337,13231,16384,19754,23297,26965,30710,34482,38232,41909,45465,48853,52027,54947,57572,59868,61806,63358,64505,65231,65527,65389,64818,63823,62416,60616,58447,55937,53121,50034,46719,43219,39580,35851,32081,28321,24619,21025,17587,14350,11357,8648,6258,4220,2561,1302,459,45,65,518,1399,2695,4390,6462,8881,11618,14635,17892,21346,24951,28661};

//const unsigned int doom_pcm_size = sizeof(doom_pcm) / sizeof(doom_pcm[0]);

//u32 output_length = 14073412/4;

//u32 bufferA_main[NR_AUDIO_SAMPLES] __attribute__((aligned(32)));
//u32 bufferB_main[NR_AUDIO_SAMPLES] __attribute__((aligned(32)));
//int16_t playBufferMain[100000000];
//u32* currentBuffer_main = bufferA_main;
//u32* nextBuffer_main = bufferB_main;

//u32 playBufferLen;
//u32 currentLen;

//u8 loopMusic = 1;
//u8 monoSTEREOMusic = 2; //1 if mono | 2 if stereo


//int DemoStereo(void);
//int DemoSoundFromMono(void);
//int DemoSoundFromStereo(void);
// Audio constants
// Number of seconds to record/playback




/*
 * Interrupt vector table
 */



static XIic sIic;
XAxiDma sAxiDma;		/* Instance of the XAxiDma */

XGpio input_btw_sw;

#ifdef XPAR_INTC_0_DEVICE_ID
 static XIntc sIntc;
#else
 static XScuGic sIntc;
#endif

//
// Interrupt vector table
#ifdef XPAR_INTC_0_DEVICE_ID
const ivt_t ivt[] = {
	//IIC
	{XPAR_AXI_INTC_0_AXI_IIC_0_IIC2INTC_IRPT_INTR, (XInterruptHandler)XIic_InterruptHandler, &sIic},
	//DMA Stream to MemoryMap Interrupt handler
	{XPAR_AXI_INTC_0_AXI_DMA_0_S2MM_INTROUT_INTR, (XInterruptHandler)fnS2MMInterruptHandler, &sAxiDma},
	//DMA MemoryMap to Stream Interrupt handler
	{XPAR_AXI_INTC_0_AXI_DMA_0_MM2S_INTROUT_INTR, (XInterruptHandler)fnMM2SInterruptHandler, &sAxiDma},
	//User I/O (buttons, switches, LEDs)
	{XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR, (XInterruptHandler)fnUserIOIsr, &input_btw_sw}
};
#else
const ivt_t ivt[] = {
	//IIC
	videoGpioIvt(HDMI_IN_GPIO_IRPT_ID, &videoCapt),
	videoVtcIvt(HDMI_IN_VTC_IRPT_ID, &(videoCapt.vtc)),
	{XPAR_FABRIC_AXI_IIC_0_IIC2INTC_IRPT_INTR, (Xil_ExceptionHandler)XIic_InterruptHandler, &sIic},
	//DMA Stream to MemoryMap Interrupt handler
	{XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR, (Xil_ExceptionHandler)fnS2MMInterruptHandler, &sAxiDma},
	//DMA MemoryMap to Stream Interrupt handler
	{XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)fnMM2SInterruptHandler, &sAxiDma}

	//User I/O (buttons, switches, LEDs)
	//{XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR, (Xil_ExceptionHandler)fnUserIOIsr, &input_btw_sw}
};


#endif


/*****************************************************************************/
/**
*
* Main function
*
* This function is the main entry of the interrupt test. It does the following:
*	Initialize the interrupt controller
*	Initialize the IIC controller
*	Initialize the User I/O driver
*	Initialize the DMA engine
*	Initialize the Audio I2S controller
*	Enable the interrupts
*	Wait for a button event then start selected task
*	Wait for task to complete
*
* @param	None
*
* @return
*		- XST_SUCCESS if example finishes successfully
*		- XST_FAILURE if example fails.
*
* @note		None.
*
******************************************************************************/
/*int main(void)
{
	u8 c;
	DemoInitialize();


	//DemoRun();
	fnDemoInit();
	loadMusic(doom_pcm, (u32) doom_pcm_len, LOOP, MONO);
	while(1){
		if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			c = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			xil_printf("c = %c\n\r", c);
			if(c == 'j')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);
			}
			else if (c == 'k')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);
			}
		}
	}
	//DemoSoundFromStereo();
	//DemoStereo();


}*/



/*int DemoSoundFromMono(void)
{
	u32 current_length = 0;
	unsigned char c;
	for (u32 i = 0; i < NR_AUDIO_SAMPLES; i++) {
		int16_t sample = (int16_t)((output_pcm[i * 2 + 1] << 8) | output_pcm[i * 2]); // Combine les octets en 16 bits signés
		currentBuffer_main[i] = (u32)((sample + 32768) * 8); // Convertit en non signé
	}
	while(current_length < output_length)
	{
		if (!Demo.fAudioPlayback)
		{
		    xil_printf("\r\nPlaying generated sound...\r\n");
		    fnAudioRecordFromMemory(sAxiDma, currentBuffer_main, NR_AUDIO_SAMPLES);
		    fnAudioPlayFromMemory(sAxiDma, NR_AUDIO_SAMPLES);
		    Demo.fAudioPlayback = 1;
		    current_length += NR_AUDIO_SAMPLES;
		    if (current_length < output_length)
		    {
				for (u32 i = 0; i < NR_AUDIO_SAMPLES; i++)
				{
					u32 index = current_length * 2 + i * 2;
					int16_t sample = (int16_t)((output_pcm[index + 1] << 8) | output_pcm[index]);
					nextBuffer_main[i] = (u32)((sample + 32768) * 8);
				}

				u32* temp = currentBuffer_main;
				currentBuffer_main = nextBuffer_main;
				nextBuffer_main = temp;
		    }

		}
		else if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			c = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			xil_printf("c = %c\n\r", c);
			if(c == 'j')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);

			}
			else if (c == 'k')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);

			}

		}

	}




		xil_printf("\r\n--- Exiting main() --- \r\n");


		return XST_SUCCESS;
}*/

/*int DemoSoundFromStereo(void)
{
	u32 current_length = 0;
	unsigned char c;
	for (u32 i = 0; i < output_length; i++) {
	    playBufferMain[i] = (int16_t)((output_pcm[i*2*STEREO+1] << 8) + output_pcm[i*2*STEREO]);
	}
	for (u32 i = 0; i < NR_AUDIO_SAMPLES; i++) {
		currentBuffer_main[i] = (u32)((playBufferMain[i] + 32768) * 8); // Convertit en non signé
	}
	while(current_length < output_length)
	{
		if (!Demo.fAudioPlayback)
		{
		    xil_printf("\r\nPlaying generated sound...\r\n");
		    fnAudioRecordFromMemory(sAxiDma, currentBuffer_main, NR_AUDIO_SAMPLES);
		    fnAudioPlayFromMemory(sAxiDma, NR_AUDIO_SAMPLES);
		    Demo.fAudioPlayback = 1;

		    current_length += NR_AUDIO_SAMPLES;
		    if (current_length < output_length)
		    {
				for (u32 i = 0; i < NR_AUDIO_SAMPLES; i++)
				{
					nextBuffer_main[i] = (u32)((playBufferMain[i + current_length] + 32768) * 8);
				}

				u32* temp = currentBuffer_main;
				currentBuffer_main = nextBuffer_main;
				nextBuffer_main = temp;
		    }

		}
		else if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			c = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			xil_printf("c = %c\n\r", c);
			if(c == 'j')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);

			}
			else if (c == 'k')
			{
				DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);

			}

		}

	}




		xil_printf("\r\n--- Exiting main() --- \r\n");


		return XST_SUCCESS;
}*/

/*int DemoStereo(void)
{
	for (u32 i = 0; i < NR_AUDIO_SAMPLES; i++) {
		nextBuffer_main[i] = sin_440[i%109]; // Combine les octets en 16 bits signés
		currentBuffer_main[i] = 0; // Convertit en non signé
		bufferC[i] = (nextBuffer_main[i] << 16) | currentBuffer_main[i];
	}
	while(1)
	{

		if (!Demo.fAudioPlayback) {
		    xil_printf("\r\nPlaying generated sound...\r\n");
		    fnAudioRecordFromMemory(sAxiDma, bufferC, NR_AUDIO_SAMPLES);
		    fnAudioPlayFromMemory(sAxiDma, NR_AUDIO_SAMPLES);
		    Demo.fAudioPlayback = 1;
		}

	}




		xil_printf("\r\n--- Exiting main() --- \r\n");


		return XST_SUCCESS;
}*/


void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i;

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}

	/*
	 * Initialize a timer used for a simple delay
	 */
	TimerInitialize(SCU_TIMER_ID);
	//if(initTimer() == XST_SUCCESS) xil_printf("Timer initialized\n\r");

//	InitTicks();  // Init Global Timer

	xil_printf("Timers initialized\n\r");

	// Initialization input XGpio variable
	XGpio_Initialize(&input_btn_sw, XPAR_AXI_GPIO_0_DEVICE_ID);
	XGpio_Initialize(&input_row, XPAR_AXI_GPIO_2_DEVICE_ID);

	// Initialization output XGpio variable
	XGpio_Initialize(&output_led_col, XPAR_AXI_GPIO_1_DEVICE_ID);

	// Variable to keep track of the state of the switches
	//XGpio_DiscreteWrite(&output_led_col, 1, 0xB);

	xil_printf("GPIO initialized\n\r");

	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, HDMI_OUT_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	DisplaySetMode(&dispCtrl, &VMODE_1280x1024);
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Interrupt controller and start it.
	 */
	Status = fnInitInterruptController(&intc);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing interrupts");
		return;
	}
	fnEnableInterrupts(&intc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

	/*
	 * Initialize the Video Capture device
	 */
	Status = VideoInitialize(&videoCapt, &intc, &vdma, HDMI_IN_GPIO_ID, HDMI_IN_VTC_ID, HDMI_IN_VTC_IRPT_ID, pFrames, DEMO_STRIDE, DEMO_START_ON_DET);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Video Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Set the Video Detect callback to trigger the menu to reset, displaying the new detected resolution
	 */
	VideoSetCallback(&videoCapt, DemoISR, &fRefresh);

	DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_1);
	xil_printf("wait start\r\n");
	TimerDelay(5000000);
	xil_printf("wait end\r\n");
	DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_0);

	return;
}


void DemoRun()
{
	int nextFrame = 0;
	char userInput = 0;

	/* Flush UART FIFO */
	while (XUartPs_IsReceiveData(UART_BASEADDR))
	{
		XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
	}

	while (userInput != 'q')
	{
		fRefresh = 0;
		DemoPrintMenu();

		/* Wait for data on UART */
		while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART receive FIFO and echo it */
		if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			xil_printf("%c", userInput);
		}
		else  //Refresh triggered by video detect interrupt
		{
			userInput = 'r';
		}

		switch (userInput)
		{
		case '1':
			DemoChangeRes();
			break;
		case '2':
			nextFrame = dispCtrl.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 1;
			}
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '3':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);
			break;
		case '4':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);
			break;
		case '5':
			if (videoCapt.state == VIDEO_STREAMING)
				VideoStop(&videoCapt);
			else
				VideoStart(&videoCapt);
			break;
		case '6':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 1;
			}
			VideoChangeFrame(&videoCapt, nextFrame);
			break;
		case '7':
			Xil_DCacheDisable();
			nextFrame = DemoGetInactiveFrame(&dispCtrl, &videoCapt);
//			nextFrame = videoCapt.curFrame + 1;
//			if (nextFrame >= DISPLAY_NUM_FRAMES)
//			{
//				nextFrame = 1;
//			}
			VideoStop(&videoCapt);
			DemoInvertFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			Xil_DCacheEnable();
			break;
		case '8':
			nextFrame = DemoGetInactiveFrame(&dispCtrl, &videoCapt);
//			nextFrame = videoCapt.curFrame + 1;
//			if (nextFrame >= DISPLAY_NUM_FRAMES)
//			{
//				nextFrame = 1;
//			}
			VideoStop(&videoCapt);
			DemoScaleFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '9':
			//Xil_DCacheFlushRange((u32)pFrames[dispCtrl.curFrame], DEMO_MAX_FRAME);
			//DemoSound();
			break;
		case 'q':
			break;
		case 'r':
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			TimerDelay(500000);
		}
	}

	return;
}

void DemoPrintMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*                ZYBO Video Demo                 *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Display Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Display Pixel Clock Freq. (MHz): %15.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("*Display Frame Index: %27d*\n\r", dispCtrl.curFrame);
	if (videoCapt.state == VIDEO_DISCONNECTED) xil_printf("*Video Capture Resolution: %22s*\n\r", "!HDMI UNPLUGGED!");
	else xil_printf("*Video Capture Resolution: %17dx%-4d*\n\r", videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo);
	xil_printf("*Video Frame Index: %29d*\n\r", videoCapt.curFrame);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - Change Display Resolution\n\r");
	xil_printf("2 - Change Display Framebuffer Index\n\r");
	xil_printf("3 - Print Blended Test Pattern to Display Framebuffer\n\r");
	xil_printf("4 - Print Color Bar Test Pattern to Display Framebuffer\n\r");
	xil_printf("5 - Start/Stop Video stream into Video Framebuffer\n\r");
	xil_printf("6 - Change Video Framebuffer Index\n\r");
	xil_printf("7 - Grab Video Frame and invert colors\n\r");
	xil_printf("8 - Grab Video Frame and scale to Display resolution\n\r");
	xil_printf("9 - Demo Sound\n\r");
	xil_printf("q - Quit\n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Enter a selection:");
}

void DemoChangeRes()
{
	int fResSet = 0;
	int status;
	char userInput = 0;

	/* Flush UART FIFO */
	while (XUartPs_IsReceiveData(UART_BASEADDR))
	{
		XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
	}

	while (!fResSet)
	{
		DemoCRMenu();

		/* Wait for data on UART */
		while (!XUartPs_IsReceiveData(UART_BASEADDR))
		{}

		/* Store the first character in the UART recieve FIFO and echo it */
		userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
		xil_printf("%c", userInput);
		status = XST_SUCCESS;
		switch (userInput)
		{
		case '1':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_640x480);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '2':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_800x600);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '3':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x720);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '4':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x1024);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '5':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1600x900);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '6':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1920x1080);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case 'q':
			fResSet = 1;
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			TimerDelay(500000);
		}
		if (status == XST_DMA_ERROR)
		{
			xil_printf("\n\rWARNING: AXI VDMA Error detected and cleared\n\r");
		}
	}
}

void DemoCRMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*                ZYBO Video Demo                 *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Current Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Pixel Clock Freq. (MHz): %23.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - %s\n\r", VMODE_640x480.label);
	xil_printf("2 - %s\n\r", VMODE_800x600.label);
	xil_printf("3 - %s\n\r", VMODE_1280x720.label);
	xil_printf("4 - %s\n\r", VMODE_1280x1024.label);
	xil_printf("5 - %s\n\r", VMODE_1600x900.label);
	xil_printf("6 - %s\n\r", VMODE_1920x1080.label);
	xil_printf("q - Quit (don't change resolution)\n\r");
	xil_printf("\n\r");
	xil_printf("Select a new resolution:");
}

int DemoGetInactiveFrame(DisplayCtrl *DispCtrlPtr, VideoCapture *VideoCaptPtr)
{
	int i;
	for (i=1; i<DISPLAY_NUM_FRAMES; i++)
	{
		if (DispCtrlPtr->curFrame == i && DispCtrlPtr->state == DISPLAY_RUNNING)
		{
			continue;
		}
		else if (VideoCaptPtr->curFrame == i && VideoCaptPtr->state == VIDEO_STREAMING)
		{
			continue;
		}
		else
		{
			return i;
		}
	}
	xil_printf("Unreachable error state reached. All buffers are in use.\r\n");
}

void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride)
{
	u32 xcoi, ycoi;
	u32 lineStart = 0;
	Xil_DCacheInvalidateRange((unsigned int) srcFrame, DEMO_MAX_FRAME);
	for(ycoi = 0; ycoi < height; ycoi++)
	{
		for(xcoi = 0; xcoi < (width * 3); xcoi+=3)
		{
			destFrame[xcoi + lineStart] = ~srcFrame[xcoi + lineStart];         //Red
			destFrame[xcoi + lineStart + 1] = ~srcFrame[xcoi + lineStart + 1]; //Blue
			destFrame[xcoi + lineStart + 2] = ~srcFrame[xcoi + lineStart + 2]; //Green
		}
		lineStart += stride;
	}
	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);
}


/*
 * Bilinear interpolation algorithm. Assumes both frames have the same stride.
 */
void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride)
{
	float xInc, yInc; // Width/height of a destination frame pixel in the source frame coordinate system
	float xcoSrc, ycoSrc; // Location of the destination pixel being operated on in the source frame coordinate system
	float x1y1, x2y1, x1y2, x2y2; //Used to store the color data of the four nearest source pixels to the destination pixel
	int ix1y1, ix2y1, ix1y2, ix2y2; //indexes into the source frame for the four nearest source pixels to the destination pixel
	float xDist, yDist; //distances between destination pixel and x1y1 source pixels in source frame coordinate system

	int xcoDest, ycoDest; // Location of the destination pixel being operated on in the destination coordinate system
	int iy1; //Used to store the index of the first source pixel in the line with y1
	int iDest; //index of the pixel data in the destination frame being operated on

	int i;

	xInc = ((float) srcWidth - 1.0) / ((float) destWidth);
	yInc = ((float) srcHeight - 1.0) / ((float) destHeight);

	ycoSrc = 0.0;
	for (ycoDest = 0; ycoDest < destHeight; ycoDest++)
	{
		iy1 = ((int) ycoSrc) * stride;
		yDist = ycoSrc - ((float) ((int) ycoSrc));

		/*
		 * Save some cycles in the loop below by presetting the destination
		 * index to the first pixel in the current line
		 */
		iDest = ycoDest * stride;

		xcoSrc = 0.0;
		for (xcoDest = 0; xcoDest < destWidth; xcoDest++)
		{
			ix1y1 = iy1 + ((int) xcoSrc) * 3;
			ix2y1 = ix1y1 + 3;
			ix1y2 = ix1y1 + stride;
			ix2y2 = ix1y1 + stride + 3;

			xDist = xcoSrc - ((float) ((int) xcoSrc));

			/*
			 * For loop handles all three colors
			 */
			for (i = 0; i < 3; i++)
			{
				x1y1 = (float) srcFrame[ix1y1 + i];
				x2y1 = (float) srcFrame[ix2y1 + i];
				x1y2 = (float) srcFrame[ix1y2 + i];
				x2y2 = (float) srcFrame[ix2y2 + i];

				/*
				 * Bilinear interpolation function
				 */
				destFrame[iDest] = (u8) ((1.0-yDist)*((1.0-xDist)*x1y1+xDist*x2y1) + yDist*((1.0-xDist)*x1y2+xDist*x2y2));
				iDest++;
			}
			xcoSrc += xInc;
		}
		ycoSrc += yInc;
	}

	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);

	return;
}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi;
	u32 iPixelAddr;
	u8 wRed, wBlue, wGreen;
	u32 wCurrentInt;
	double fRed, fBlue, fGreen, fColor;
	u32 xLeft, xMid, xRight, xInt;
	u32 yMid, yInt;
	double xInc, yInc;


	switch (pattern)
	{
	case DEMO_PATTERN_0:

		xInt = width / 4; //Four intervals, each with width/4 pixels
		xLeft = xInt * 3;
		xMid = xInt * 2 * 3;
		xRight = xInt * 3 * 3;
		xInc = 256.0 / ((double) xInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		yInt = height / 2; //Two intervals, each with width/2 lines
		yMid = yInt;
		yInc = 256.0 / ((double) yInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		fBlue = 0.0;
		fRed = 256.0;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{
			/*
			 * Convert color intensities to integers < 256, and trim values >=256
			 */
			wRed = (fRed >= 256.0) ? 255 : ((u8) fRed);
			wBlue = (fBlue >= 256.0) ? 255 : ((u8) fBlue);
			iPixelAddr = xcoi;
			fGreen = 0.0;
			for(ycoi = 0; ycoi < height; ycoi++)
			{

				wGreen = (fGreen >= 256.0) ? 255 : ((u8) fGreen);
				frame[iPixelAddr] = 0;
				frame[iPixelAddr + 1] = 0;
				frame[iPixelAddr + 2] = 0;
				if (ycoi < yMid)
				{
					fGreen += yInc;
				}
				else
				{
					fGreen -= yInc;
				}

				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			if (xcoi < xLeft)
			{
				fBlue = 0.0;
				fRed -= xInc;
			}
			else if (xcoi < xMid)
			{
				fBlue += xInc;
				fRed += xInc;
			}
			else if (xcoi < xRight)
			{
				fBlue -= xInc;
				fRed -= xInc;
			}
			else
			{
				fBlue += xInc;
				fRed = 0;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	case DEMO_PATTERN_1:

		xInt = width / 7; //Seven intervals, each with width/7 pixels
		xInc = 256.0 / ((double) xInt); //256 color intensities per interval. Notice that overflow is handled for this pattern.

		fColor = 0.0;
		wCurrentInt = 1;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{

			/*
			 * Just draw white in the last partial interval (when width is not divisible by 7)
			 */
			if (wCurrentInt > 7)
			{
				wRed = 255;
				wBlue = 255;
				wGreen = 255;
			}
			else
			{
				if (wCurrentInt & 0b001)
					wRed = (u8) fColor;
				else
					wRed = 0;

				if (wCurrentInt & 0b010)
					wBlue = (u8) fColor;
				else
					wBlue = 0;

				if (wCurrentInt & 0b100)
					wGreen = (u8) fColor;
				else
					wGreen = 0;
			}

			iPixelAddr = xcoi;

			for(ycoi = 0; ycoi < height; ycoi++)
			{
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			fColor += xInc;
			if (fColor >= 256.0)
			{
				fColor = 0.0;
				wCurrentInt++;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	default :
		xil_printf("Error: invalid pattern passed to DemoPrintTest");
	}
}

void DemoISR(void *callBackRef, void *pVideo)
{
	char *data = (char *) callBackRef;
	*data = 1; //set fRefresh to 1
}

XStatus fnDemoInit(void)
{
		int Status;

		Demo.u8Verbose = 0;

//		const unsigned char i2sClockFreq[] ={
//				16,
//				24,
//				32,
//				48,
//				64,
//				96,
//				192
//		};

		//Xil_DCacheDisable();

		xil_printf("\r\n--- Entering main() --- \r\n");
		xil_printf("\r\n--- Audio Freq = 44.1kHz --- \r\n");


		//
		//Initialize the interrupt controller

		Status = fnInitInterruptController(&sIntc);
		if(Status != XST_SUCCESS) {
			xil_printf("Error initializing interrupts");
			return XST_FAILURE;
		}


		// Initialize IIC controller
		Status = fnInitIic(&sIic);
		if(Status != XST_SUCCESS) {
			xil_printf("Error initializing I2C controller");
			return XST_FAILURE;
		}

		// Initialize User I/O driver
			    /*Status = fnInitUserIO(&input_btw_sw);
			    if(Status != XST_SUCCESS) {
			    	xil_printf("User I/O ERROR");
			    	return XST_FAILURE;
			    }*/



		//Initialize DMA
		Status = fnConfigDma(&sAxiDma);
		if(Status != XST_SUCCESS) {
			xil_printf("DMA configuration ERROR");
			return XST_FAILURE;
		}


		//Initialize Audio I2S
		Status = fnInitAudio();
		if(Status != XST_SUCCESS) {
			xil_printf("Audio initializing ERROR");
			return XST_FAILURE;
		}




		// Enable all interrupts in our interrupt vector table
		// Make sure all driver instances using interrupts are initialized first
		fnEnableInterrupts(&sIntc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

		//Xil_DCacheFlushRange((u32) myDataBuffer, 5*NR_AUDIO_SAMPLES);
		//Xil_DCacheInvalidateRange((u32) myDataBuffer, 5*NR_AUDIO_SAMPLES);

		//fnAudioReadFromReg(u8RegAddr, u8RxData)
		fnSetHpOutput();

		return XST_SUCCESS;
}









