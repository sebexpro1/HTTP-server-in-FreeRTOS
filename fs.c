//*****************************************************************************
//
// fs.c - File System Processing for lwIP Web Server Apps.
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
#include <cstdio>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utils/lwiplib.h"
#include "lwip_task.h"
#include "httpserver_raw/httpd.h"
#include "httpserver_raw/fs.h"
#include "httpserver_raw/fsdata.h"

//*****************************************************************************
//
// Include the file system data for this application.  This file is generated
// by the makefsfile utility, using the following command:
//
//     makefsfile -i fs -o htmldata.h -r -h
//
// If any changes are made to the static content of the web pages served by the
// application, this command must be used to regenerate lmi-fsdata.h in order
// for those changes to be picked up by the web server.
//
//*****************************************************************************
// Include http content only if HTTP server is run

#if (RUN_HTTP_SERVER)
#include "htmldata.h"

//*****************************************************************************
//
// Initialize the file system.
//
//*****************************************************************************
extern xQueueHandle xQueue1;
extern xQueueHandle xQueue2;

void
fs_init(void)
{
    //
    // Nothing special to do for this application.  Flash File System only.
    //
}

//*****************************************************************************
//
// File System tick handler.
//
//*****************************************************************************
void
fs_tick(uint32_t ui32TickMS)
{
    //
    // Nothing special to do for this application.  Flash File System only.
    //
}

//*****************************************************************************
//
// Open a file and return a handle to the file, if found.  Otherwise,
// return NULL.
//
//*****************************************************************************
struct fs_file *
fs_open(const char *name)
{
    const struct fsdata_file *psTree;
    struct fs_file *psFile = NULL;

    //
    // Allocate memory for the file system structure.
    //
    psFile = mem_malloc(sizeof(struct fs_file));
    if(NULL == psFile)
    {
        return(NULL);
    }

    if(strcmp(name, "/dataread") == 0)
    {
    	static char pcBuf[50];   //The buffer for HTTP response
    	float tempp = 0;
    	int timee = 0;
    	int temp_ten;
    	int points_temp2;
    	int full_temp2;


    	xQueuePeek(xQueue1, &timee, 1);
    	xQueuePeek(xQueue2, &tempp, 1);

    			temp_ten = tempp * 10;
    			points_temp2 = temp_ten % 10;
    			full_temp2 = temp_ten / 10;


    	// Create HTTP response here...
    	usprintf(pcBuf,"time=%d&temperature=%d.%d",timee, full_temp2, points_temp2);

		psFile->data = pcBuf;
		psFile->len = strlen(pcBuf);
		psFile->index = psFile->len;
		psFile->pextension = NULL;

		//
		// Return the psFile system pointer.
		//
		return(psFile);
   }

    else
    {
		//
		// Initialize the file system tree pointer to the root of the linked list.
		//
		psTree = FS_ROOT;

		//
		// Begin processing the linked list, looking for the requested file name.
		//
		while(NULL != psTree)
		{
			//
			// Compare the requested file "name" to the file name in the
			// current node.
			//
			if(strncmp(name, (char *)psTree->name, psTree->len) == 0)
			{
				//
				// Fill in the data pointer and length values from the
				// linked list node.
				//
				psFile->data = (char *)psTree->data;
				psFile->len = psTree->len;

				//
				// For now, we setup the read index to the end of the file,
				// indicating that all data has been read.
				//
				psFile->index = psTree->len;

				//
				// We are not using any file system extensions in this
				// application, so set the pointer to NULL.
				//
				psFile->pextension = NULL;

				//
				// Exit the loop and return the file system pointer.
				//
				break;
			}

			//
			// If we get here, we did not find the file at this node of the linked
			// list.  Get the next element in the list.
			//
			psTree = psTree->next;
		}
    }

    //
    // If we didn't find the file, ptTee will be NULL.  Make sure we
    // return a NULL pointer if this happens.
    //
    if(NULL == psTree)
    {
        mem_free(psFile);
        psFile = NULL;
    }

    //
    // Return the file system pointer.
    //
    return(psFile);
}

//*****************************************************************************
//
// Close an opened file designated by the handle.
//
//*****************************************************************************
void
fs_close(struct fs_file *file)
{
    //
    // If a Fat file was opened, free its object.
    //
    if(file->pextension)
    {
        mem_free(file->pextension);
    }

    //
    // Free the main file system object.
    //
    mem_free(file);
}

//*****************************************************************************
//
// Read the next chunck of data from the file.  Return the count of data
// that was read.  Return 0 if no data is currently available.  Return
// a -1 if at the end of file.
//
//*****************************************************************************
int
fs_read(struct fs_file *file, char *buffer, int count)
{
    int iAvailable;

    //
    // Check to see if more data is available.
    //
    if(file->len == file->index)
    {
        //
        // There is no remaining data.  Return a -1 for EOF indication.
        //
        return(-1);
    }

    //
    // Determine how much data we can copy.  The minimum of the 'count'
    // parameter or the available data in the file system buffer.
    //
    iAvailable = file->len - file->index;
    if(iAvailable > count)
    {
        iAvailable = count;
    }

    //
    // Copy the data.
    //
    memcpy(buffer, file->data + iAvailable, iAvailable);
    file->index += iAvailable;

    //
    // Return the count of data that we copied.
    //
    return(iAvailable);
}

//*****************************************************************************
//
// Determine the number of bytes left to read from the file.
//
//*****************************************************************************
int fs_bytes_left(struct fs_file *file)
{
    //
    // Return the number of bytes left to be read from this file.
    //
    return(file->len - file->index);
}
#endif
