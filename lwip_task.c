//*****************************************************************************
//
// lwip_task.c - Tasks to serve web pages over Ethernet using lwIP.
//
// Copyright (c) 2009-2014 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.0.12573 of the DK-TM4C129X Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "driverlib/rom.h"
#include "utils/lwiplib.h"
#include "utils/locator.h"
#include "utils/ustdlib.h"
#include "grlib/grlib.h"
#include "httpserver_raw/httpd.h"
#include "lwip_task.h"

extern uint32_t g_ui32SysClock;
extern tContext g_sContext;

//*****************************************************************************
//
// Sets up the additional lwIP raw API services provided by the application.
//
//*****************************************************************************
void
SetupServices(void *pvArg)
{
    uint8_t pui8MAC[6];

    //
    // Setup the device locator service.
    //
    LocatorInit();
    lwIPLocalMACGet(pui8MAC);
    LocatorMACAddrSet(pui8MAC);

    LocatorAppTitleSet("DK-TM4C129X freertos_demo");

    //
    // Initialize the sample httpd server.
    //
    httpd_init();

}

//*****************************************************************************
//
// Initializes the lwIP tasks.
//
//*****************************************************************************
uint32_t
lwIPTaskInit(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MAC[6];

    //
    // Get the MAC address from the user registers.
    //
    ROM_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        return(1);
    }

    //
    // Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
    // address needed to program the hardware registers, then program the MAC
    // address into the Ethernet Controller registers.
    //
    pui8MAC[0] = ((ui32User0 >>  0) & 0xff);
    pui8MAC[1] = ((ui32User0 >>  8) & 0xff);
    pui8MAC[2] = ((ui32User0 >> 16) & 0xff);
    pui8MAC[3] = ((ui32User1 >>  0) & 0xff);
    pui8MAC[4] = ((ui32User1 >>  8) & 0xff);
    pui8MAC[5] = ((ui32User1 >> 16) & 0xff);

    //
    // Lower the priority of the Ethernet interrupt handler.  This is required
    // so that the interrupt handler can safely call the interrupt-safe
    // FreeRTOS functions (specifically to send messages to the queue).
    //
    ROM_IntPrioritySet(INT_EMAC0, 0xC0);

    //
    // Initialize lwIP.
    //
    lwIPInit(g_ui32SysClock, pui8MAC, 0, 0, 0, IPADDR_USE_DHCP);

    //
    // Setup the remaining services inside the TCP/IP thread's context.
    //
    tcpip_callback(SetupServices, 0);

    //
    // Success.
    //
    return(0);
}

//
// This hook is called by FreeRTOS when an stack overflow error is detected.
//
//*****************************************************************************
void
vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName)
{

    // A fatal FreeRTOS error was detected

    while(1)
    {
    }
}

#if (RUN_HTTP_SERVER)

static uint32_t g_ui32Tasks = 0;
static uint32_t g_ui32IPAddress = 0xffffffff;
static char g_pcIPString[24];

//*****************************************************************************
//
// Displays the IP address in a human-readable format.
//
//*****************************************************************************
static void
DisplayIP(uint32_t ui32IP)
{
    uint32_t ui32Loop, ui32Idx, ui32Value;

    //
    // If there is no IP address, indicate that one is being acquired.
    //
    if(ui32IP == 0)
    {
        GrStringDraw(&g_sContext, "IP address: acquiring...", -1, 50, 50, 1);
        return;
    }

    //
    // Set the initial index into the string that is being constructed.
    //
    ui32Idx = 0;

    //
    // Start the string with four spaces.  Not all will necessarily be used,
    // depending upon the length of the IP address string.
    //
    for(ui32Loop = 0; ui32Loop < 4; ui32Loop++)
    {
        g_pcIPString[ui32Idx++] = ' ';
    }

    //
    // Loop through the four bytes of the IP address.
    //
    for(ui32Loop = 0; ui32Loop < 32; ui32Loop += 8)
    {
        //
        // Extract this byte from the IP address word.
        //
        ui32Value = (ui32IP >> ui32Loop) & 0xff;

        //
        // Convert this byte into ASCII, using only the characters required.
        //
        if(ui32Value > 99)
        {
            g_pcIPString[ui32Idx++] = '0' + (ui32Value / 100);
        }
        if(ui32Value > 9)
        {
            g_pcIPString[ui32Idx++] = '0' + ((ui32Value / 10) % 10);
        }
        g_pcIPString[ui32Idx++] = '0' + (ui32Value % 10);

        //
        // Add a dot to separate this byte from the next.
        //
        g_pcIPString[ui32Idx++] = '.';
    }

    //
    // Fill the remainder of the string buffer with spaces.
    //
    for(ui32Loop = ui32Idx - 1; ui32Loop < 20; ui32Loop++)
    {
        g_pcIPString[ui32Loop] = ' ';
    }

    //
    // Null terminate the string at the appropriate place, based on the length
    // of the string version of the IP address.  There may or may not be
    // trailing spaces that remain.
    //
    g_pcIPString[ui32Idx + 3 - ((ui32Idx - 12) / 2)] = '\0';

    //
    // Display the string.  The horizontal position and the number of leading
    // spaces utilized depend on the length of the string version of the IP
    // address.  The end result is the IP address centered in the provided
    // space with leading/trailing spaces as required to clear the remainder of
    // the space.
    //

    GrStringDraw(&g_sContext, g_pcIPString + ((ui32Idx - 12) / 2), -1, 130, 50, 1);
}

void
vApplicationIdleHook(void)
{
	char buf[20];

	uint32_t ui32Temp = uxTaskGetNumberOfTasks() - 1;

	if(ui32Temp != g_ui32Tasks)
	{
		g_ui32Tasks = ui32Temp;

	    usprintf(buf, "Tasks: %d", g_ui32Tasks);

		GrStringDraw(&g_sContext, buf, -1, 50, 100, 1);
	}


	//
    // Get the current IP address.
    //
	ui32Temp = lwIPLocalIPAddrGet();

    //
    // See if the IP address has changed.
    //
    if(ui32Temp != g_ui32IPAddress)
    {
        //
        // Save the current IP address.
        //
        g_ui32IPAddress = ui32Temp;

        //
        // Update the display of the IP address.
        //
        DisplayIP(ui32Temp);
    }
}
#else
void vApplicationIdleHook(void)
{
	// do nothing
}
#endif
