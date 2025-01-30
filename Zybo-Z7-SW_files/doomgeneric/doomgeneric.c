#include <stdio.h>

#include "xil_types.h"

#include "m_argv.h"

#include "doomgeneric.h"


pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);




void doomgeneric_Create(int argc, char **argv)
{
	// save arguments

    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

//	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
//	xil_printf("DG_ScreenBuffer allocated: %d\r\n", DG_ScreenBuffer);

	DG_Init();
	xil_printf("DG_Init Done\r\n");

	D_DoomMain ();
}

