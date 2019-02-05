/*
 * Copyright (c) 2013 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv-control-warpblend.h"
#include <stdlib.h>

//#include <libpng16/png.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

typedef struct __attribute__((packed)) {
    float x, y;
} vertex2f;

typedef struct __attribute__((packed)) {
    vertex2f pos;
    vertex2f tex;
    vertex2f tex2;
} vertexDataRec;



int main(int ac, char **av)
{
    Display *xDpy = XOpenDisplay(NULL);
    int screenId;
      
    // read multiprojector system configuration file
    FILE *multiprojConfigFile = fopen("testdata/multiprojector-calib-config.txt", "r");
    unsigned int displayWidth, displayHeight;
    fscanf(multiprojConfigFile, "%u", &displayWidth);
    fscanf(multiprojConfigFile, "%u\n", &displayHeight);	
     // row X colum: number of points in warp matrix
    unsigned int  nFeaturesX, nFeaturesY; // represents the number of points used for warping
    fscanf(multiprojConfigFile, "%u", &nFeaturesX);
    fscanf(multiprojConfigFile, "%u\n", &nFeaturesY);	

     // read all GPUIDs:DFPIDs	
    unsigned int nGPUs, *GPUIDs, ** DPYIDs, *nDFPs;
  
    fscanf(multiprojConfigFile, "%u\n", &nGPUs);
    GPUIDs = (unsigned int*)malloc(nGPUs);	
    DPYIDs = (unsigned int**)malloc(nGPUs);
    nDFPs = (unsigned int*)malloc(nGPUs);
    
    for (unsigned int gID = 0; gID < nGPUs; gID++)
       	fscanf(multiprojConfigFile, "%u", &GPUIDs[gID]);
    
    for (unsigned int  gpuID = 0; gpuID < nGPUs; gpuID++)
    {
	fscanf(multiprojConfigFile,"%u\n", &nDFPs[gpuID]);
	// allocate memory to DFPIDs array
	printf("number of ports: %u\n", nDFPs[gpuID]);
	DPYIDs[gpuID] = (unsigned int*)malloc(nDFPs[gpuID]); 
    	for (unsigned int portID = 0; portID < nDFPs[gpuID]; portID++)
		fscanf(multiprojConfigFile, "%u", &DPYIDs[gpuID][portID]);
        fscanf(multiprojConfigFile, "\n");
    } 
   
    if (!xDpy) {
        fprintf (stderr, "Could not open X Display %s!\n", XDisplayName(NULL));
        return 1;
    }

    screenId = XDefaultScreen(xDpy);

    // Start with two screen-aligned triangles, and warp them using the sample
    // keystone matrix in transformPoint. Make sure we save W for correct
    // perspective and pass it through as the last texture coordinate component.
    // initialize vertex and texture arrays to be used for setting the warpmatrix
    
    FILE* warpMatrixFile;
    float Vertex[nFeaturesY][nFeaturesX][2], Texture[nFeaturesY][nFeaturesX][2];
    char* fileName = (char*)malloc(200);
    int count = 0;
    
   

    for (unsigned int gpuID = 0; gpuID < nGPUs; gpuID++)
    {
	for (unsigned int dfpID = 0; dfpID < nDFPs[gpuID]; dfpID++)
	{
		printf("gpu: %u\n", GPUIDs[gpuID]);
		printf("dfp: %u\n", DPYIDs[gpuID][dfpID]);
		sprintf(fileName, "testdata/WarpMap-DPY-%u.txt", DPYIDs[gpuID][dfpID]);
		warpMatrixFile = fopen(fileName, "r");
		
		for (unsigned int row = 0; row < nFeaturesY; row++)
	        {
    			for (unsigned int col = 0; col < nFeaturesX; col++)
			{
				fscanf(warpMatrixFile, "%f\t",&Vertex[row][col][0]);
				fscanf(warpMatrixFile, "%f\t",&Vertex[row][col][1]);
				fscanf(warpMatrixFile, "%f\t", &Texture[row][col][0]);
				fscanf(warpMatrixFile, "%f\n", &Texture[row][col][1]);

			}
	    	}	

		// adjust for coordinate-system conventions
		for (unsigned int row = 0; row < nFeaturesY; row++)
			for (unsigned int col = 0; col < nFeaturesX; col++)
			{	
				Texture[row][col][1] = 1.0 - Texture[row][col][1];
				for (unsigned int index = 0; index < 2; index++)
					Vertex[row][col][index] = (1 + Vertex[row][col][index]) * 0.5;
				Vertex[row][col][1] = 1.0 - Vertex[row][col][1];
			}


	    	unsigned int nWarpRectangles = (nFeaturesX - 1)*(nFeaturesY - 1), x, y;	
 	    	// read warping-map file	
	    	vertexDataRec warpData[6 * nWarpRectangles];
 
	        for (unsigned int warpRectangleID = 0; warpRectangleID < nWarpRectangles ; warpRectangleID++) 
    	        { 
			x = warpRectangleID / (nFeaturesX - 1); // row 
			y = warpRectangleID % (nFeaturesX - 1); // col
			warpData[warpRectangleID * 6].pos.x = Vertex[x][y][0];
	    		warpData[warpRectangleID * 6].pos.y = Vertex[x][y][1];
	    		warpData[warpRectangleID * 6].tex.x = Texture[x][y][0];
			warpData[warpRectangleID * 6].tex.y = Texture[x][y][1];
		    	warpData[warpRectangleID * 6].tex2.x = 0.0f;	
		    	warpData[warpRectangleID * 6].tex2.y = 1.0f;
			
		    	warpData[warpRectangleID * 6 + 1].pos.x = Vertex[x + 1][y][0];
		    	warpData[warpRectangleID * 6 + 1].pos.y = Vertex[x + 1][y][1];
		    	warpData[warpRectangleID * 6 + 1].tex.x = Texture[x + 1][y][0];
			warpData[warpRectangleID * 6 + 1].tex.y = Texture[x + 1][y][1];
		    	warpData[warpRectangleID * 6 + 1].tex2.x = 0.0f;
		    	warpData[warpRectangleID * 6 + 1].tex2.y = 1.0f;

		    	warpData[warpRectangleID * 6 + 2].pos.x = Vertex[x][y + 1][0];
		    	warpData[warpRectangleID * 6 + 2].pos.y = Vertex[x][y + 1][1];
		    	warpData[warpRectangleID * 6 + 2].tex.x = Texture[x][y + 1][0];
			warpData[warpRectangleID * 6 + 2].tex.y = Texture[x][y + 1][1];
		    	warpData[warpRectangleID * 6 + 2].tex2.x = 0.0f;
		    	warpData[warpRectangleID * 6 + 2].tex2.y = 1.0f;

		    	warpData[warpRectangleID * 6 + 3].pos.x = Vertex[x + 1][y][0];
		    	warpData[warpRectangleID * 6 + 3].pos.y = Vertex[x + 1][y][1];
		    	warpData[warpRectangleID * 6 + 3].tex.x = Texture[x + 1][y][0];
			warpData[warpRectangleID * 6 + 3].tex.y = Texture[x + 1][y][1];
		    	warpData[warpRectangleID * 6 + 3].tex2.x = 0.0f;
		    	warpData[warpRectangleID * 6 + 3].tex2.y = 1.0f;
	    
    			warpData[warpRectangleID * 6 + 4].pos.x = Vertex[x + 1][y + 1][0];
		    	warpData[warpRectangleID * 6 + 4].pos.y = Vertex[x + 1][y + 1][1];
		    	warpData[warpRectangleID * 6 + 4].tex.x = Texture[x + 1][y + 1][0];
			warpData[warpRectangleID * 6 + 4].tex.y = Texture[x + 1][y + 1][1];
		    	warpData[warpRectangleID * 6 + 4].tex2.x = 0.0f;
		    	warpData[warpRectangleID * 6 + 4].tex2.y = 1.0f;

		    	warpData[warpRectangleID * 6 + 5].pos.x = Vertex[x][y + 1][0];
		    	warpData[warpRectangleID * 6 + 5].pos.y = Vertex[x][y + 1][1];
		    	warpData[warpRectangleID * 6 + 5].tex.x = Texture[x][y + 1][0];
			warpData[warpRectangleID * 6 + 5].tex.y = Texture[x][y + 1][1];
		    	warpData[warpRectangleID * 6 + 5].tex2.x = 0.0f;
	    	warpData[warpRectangleID * 6 + 5].tex2.y = 1.0f;
    		}
		// Prime the random number generator, since the helper functions need it.
		srand(time(NULL));
		//printf ("Number of warped rectangles: %d", nWarpRectangles);
		// Apply our transformed warp data to the chosen display.
	
		int ret = XNVCTRLSetScanoutWarping(xDpy,
                	             screenId,
                        	     DPYIDs[gpuID][dfpID],
	                             NV_CTRL_WARP_DATA_TYPE_MESH_TRIANGLES_XYUVRQ,
        	                     nWarpRectangles * 6, 
                	             (float *)warpData); 
                printf("WARPING Returns: %d\n", ret);
		sleep(5);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   		Pixmap blendPixmap; // the blendmap
    		XGCValues values; // graphics context values
    		GC gc; // the graphics context
    		unsigned int blendingMap[displayWidth][displayHeight];	
    		unsigned int imageDepth = DefaultDepth(xDpy, screenId);
		printf("\nDepth of display: %d\n", imageDepth);

  		// Start with a 32x32 pixmap.
   		int return_value;	
 
  		blendPixmap = XCreatePixmap(xDpy, RootWindow(xDpy, screenId), displayWidth,  displayHeight, imageDepth);
    	gc = XCreateGC(xDpy, blendPixmap, GCForeground, &values);

//////////////////////////////////////////// CREATE BLENDING IMAGE FROM FILE ////////////////////////////

		int nChannels = 4;
  		unsigned char *blendingImage = (unsigned char*) malloc (displayWidth * displayHeight * nChannels);
		// Create a sample blending pixmap; let's make it solid white with a grey
    		// border and rely on upscaling with filtering to feather the edges.
		sprintf(fileName ,"testdata/BlendMap-DPY-%u.txt",DPYIDs[gpuID][dfpID]);
		FILE* blendingMapFile = fopen(fileName, "r");
		printf("Reading from: %s\n", fileName);

		// read header
		unsigned int temp1, temp2;
		fscanf(blendingMapFile, "%u %u\n", &temp1, &temp2);
		printf("Width, Height: %u, %u\n", temp1, temp2); 
		// read blending data:
		for (unsigned int dispHeight = 0; dispHeight < displayHeight; dispHeight++)
		{
			for (unsigned int dispWidth = 0; dispWidth < displayWidth; dispWidth++)
			{
				fscanf(blendingMapFile, "%u " , &blendingMap[dispWidth][displayHeight - dispHeight -1]);
			}
		}


	;
//		printf("image depth is: %u", imageDepth);
		for (unsigned int dispHeight = 0; dispHeight < displayHeight; dispHeight++)
		{
			for (unsigned int dispWidth = 0; dispWidth < displayWidth; dispWidth++)
			{
				for (unsigned int imageChannelID = 0; imageChannelID < nChannels; imageChannelID++)
				{		
					blendingImage[(dispHeight * displayWidth + dispWidth) * nChannels + imageChannelID] =  (unsigned char)blendingMap[dispWidth][dispHeight];

				} 
				/*blendingImage[(dispHeight * displayWidth + dispWidth) * nChannels + 0] = 0;
				blendingImage[(dispHeight * displayWidth + dispWidth) * nChannels + 1] = 0;
				blendingImage[(dispHeight * displayWidth + dispWidth) * nChannels + 2] = 0;
 				blendingImage[(dispHeight * displayWidth + dispWidth) * nChannels + 3] = 0; */ 

			}
		}

		

//////////////////////////////////////////// CREATE XIMAGE ////////////////////////////////////////////
    int bitmap_pad = 32;
    int bytes_per_line = 0; //displayWidth * nChannels;

	   XImage *xblendingImage = XCreateImage(xDpy, CopyFromParent, imageDepth, ZPixmap, 0, (char*)blendingImage, displayWidth, displayHeight, bitmap_pad , bytes_per_line);   
     unsigned int ximage_pixel;
     sprintf(fileName ,"testdata/ppm_after-%u.ppm",DPYIDs[gpuID][dfpID]);
     FILE* ppmFile = fopen(fileName, "wb"); 
     fprintf(ppmFile, "P6\n%d %d\n%d", displayWidth, displayHeight, 255);  
     
     for (unsigned int row = 0; row < displayHeight; row++)
             for (unsigned int col = 0; col < displayWidth; col++)
             {
                     ximage_pixel = XGetPixel(xblendingImage, col, row);
                     static unsigned char channel[4];
                     channel[0] = (ximage_pixel >> 24);
                     channel[1] = (ximage_pixel << 8) >> 24;
                     channel[2] = (ximage_pixel << 16) >> 24;
                     channel[3] = (ximage_pixel << 24) >> 24;

                     fwrite(channel, 1, 4, ppmFile);

             }
     fclose(ppmFile);    
	    
	    printf("after writing PPM files\n");

/////////////////////////////////////////// APPLY XIMAGE /////////////////////////////////////////////

		XPutImage(xDpy, blendPixmap, gc, xblendingImage, 0,0,0,0, displayWidth, displayHeight);


        
      
	 return_value = XNVCTRLSetScanoutIntensity(xDpy,
                                           screenId,
                             		   DPYIDs[gpuID][dfpID],
	                                  blendPixmap,
            		                   False); // TODO how to call for multigpu setup?
        printf("BLENDING returns: %d\n", return_value);
   		
	sleep(5); 
  		

  	}}
	printf("Done!!\n");
    return 0;
}
