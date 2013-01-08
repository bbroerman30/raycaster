/*************************************************************************/ 
/***                                                                   ***/ 
/***      R A Y                            (C) 1995 NPS Software       ***/  
/***                                                                   ***/  
/***                Writen By:        Brad Broerman                    ***/  
/***                Started:          June 1994                        ***/  
/***                Last Modified:    July 2, 2000                     ***/  
/***                Version:          1.5                              ***/  
/***                                                                   ***/  
/***                                                                   ***/  
/***    A 3-D Raycasting graphics engine with up to a 64x64 cell map,  ***/  
/***  35 textures (256 color 128x128 PCX files), textured floor and    ***/  
/***  ceiling, sprites, 3 types of animated doors, 20 levels of        ***/  
/***  transparency, translucency, animated textures,lighting and       ***/  
/***  depth cueing (with fog effects), zone maps, and triggers.        ***/  
/***                                                                   ***/  
/***    To be added: 2 more door types, door side texturing, door api, ***/  
/***  sprite logic api, lighting api, var. height floors, look up/down,***/  
/***  animated palette section, main menu, stats updating,             ***/
/***  trigger api.                                                     ***/
/***                                                                   ***/  
/*************************************************************************/  
/***                            Change Log:                            ***/  
/***                                                                   ***/  
/***     06/15/94  Started RAY project                                 ***/  
/***     06/30/94  Basic raycaster engine finished.                    ***/  
/***     07/30/94  Base engine optimized.                              ***/  
/***     09/10/94  Floor / ceiling texturing added.                    ***/  
/***     09/15/94  RAY first distributed.                              ***/  
/***                   -- 4 years later --                             ***/  
/***     09/25/98  Started working on Ray again...                     ***/  
/***     10/03/98  Added depth cueing/fog effects                      ***/  
/***     10/06/98  Added garage and elevator door styles               ***/  
/***     10/10/98  Fixed doors, now recessed 1/2 way back              ***/  
/***     10/14/98  Fixed array overruns ... Thx to Jorge A. Martin     ***/  
/***     10/29/98  Added animated textures, translucency, and          ***/  
/***                 recesed tile flags. Combined T & X tiles.         ***/  
/***     11/01/98  Finally got it to run in Protected Mode!            ***/  
/***     11/10/98  Added tiled light maps.                             ***/  
/***     12/05/98  Added animated lighting                             ***/  
/***     01/11/99  Increased PCX file resolution to 128x128            ***/  
/***     02/24/99  Added zone maps and zone options.                   ***/  
/***     03/11/99  Added Triggers (except 2,3,&10) (see triggers.txt)  ***/  
/***               Released Ray version 1.5                            ***/
/***     07/02/00  Did windows port of RAY. Added resize capability.   ***/
/***               Thanks to Jorge for showin me how to do the Windows ***/
/***               port.                                               ***/
/***                                                                   ***/  
/*************************************************************************/  
#include "WinRay.h"

//
// Initialize the double buffer and windows bitmaps that will be used to 
// render the double buffer.
//
void init_double_buffer(void)
{
	HDC hDC;

	if ( BitMapInfo == NULL )
		BitMapInfo = (BITMAPINFO *)malloc(sizeof(BITMAPINFOHEADER) + (256 * sizeof(RGBQUAD)));

	hDC = GetDC(ghWnd);

	BitMapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
	BitMapInfo->bmiHeader.biWidth = 320; 
	BitMapInfo->bmiHeader.biHeight = -200; 
	BitMapInfo->bmiHeader.biPlanes = 1; 
	BitMapInfo->bmiHeader.biBitCount = 8; 
	BitMapInfo->bmiHeader.biCompression = BI_RGB; 
	BitMapInfo->bmiHeader.biSizeImage  = 0; 
	BitMapInfo->bmiHeader.biXPelsPerMeter = 0; 
	BitMapInfo->bmiHeader.biYPelsPerMeter = 0; 
	BitMapInfo->bmiHeader.biClrUsed = 0;
	BitMapInfo->bmiHeader.biClrImportant = 0;
    
	//	Load the palette
	for (int i= 0; i < 256; ++i)
	{
		BitMapInfo->bmiColors[i].rgbBlue = palette[i][2]; 
		BitMapInfo->bmiColors[i].rgbGreen = palette[i][1]; 
		BitMapInfo->bmiColors[i].rgbRed = palette[i][0]; 
	}
	if (DblBuff != NULL)
  		DeleteObject(DblBuff);

	DblBuff = CreateDIBSection(hDC, (BITMAPINFO *)BitMapInfo, DIB_RGB_COLORS, (VOID **)&double_buffer, 0, 0);

	ReleaseDC(ghWnd,hDC);
}

// This modifies the double buffers palette. It can be used for fading or flashing effects (whole screen)
//
void ChangePalette ( float percent )
{
	HDC hDC, hMemDC;
	HBITMAP DefaultBitmap;
 	RGBQUAD tmpPalette[256];
 	
 	hDC = GetDC(ghWnd);
 	
 	// Create memory device context compatible with the engines window dc
 	hMemDC = CreateCompatibleDC(GetDC(ghWnd));
 
 	// Save the default bitmap and select the engines double buffer into the memory dc
 	DefaultBitmap = (HBITMAP)SelectObject(hMemDC, DblBuff);
 
 	// Now get the palette for the selected bitmap.
 	GetDIBColorTable(hMemDC, 0, 256, tmpPalette);
  
 	// Modify the palette by the percentage.
 	for (int i=0; i<256; ++i)
 	{
 		tmpPalette[i].rgbBlue = (unsigned char )(tmpPalette[i].rgbBlue * percent); 
 		tmpPalette[i].rgbGreen = (unsigned char )(tmpPalette[i].rgbGreen * percent);
 		tmpPalette[i].rgbRed = (unsigned char )(tmpPalette[i].rgbRed * percent); 
 	}
 	
 	// and now put it back into the bitmap.
 	SetDIBColorTable(hMemDC, 0, 256, tmpPalette);
 
 	// put back the default bitmap
 	SelectObject(hMemDC, DefaultBitmap);
 	
 	// delete the memory dc
 	DeleteDC(hMemDC);
 
 	// delete the default bitmap
 	DeleteObject(DefaultBitmap);
 
 	// release the engines window dc 
 	ReleaseDC(ghWnd,hDC);
 
 }
 
//
// This copies the double buffer to the visible window. It also performs the stretching
// when the window is not the default size.
//
void blit(void)
{
	HDC hDC, hMemDC;
	HBITMAP DefaultBitmap;

	hDC = GetDC(ghWnd);
	
	// Create memory device context compatible with the engines window dc
	hMemDC = CreateCompatibleDC(GetDC(ghWnd));

	// Save the default bitmap and select the engines double buffer into the memory dc
	DefaultBitmap = (HBITMAP)SelectObject(hMemDC, DblBuff);

	// Copy double buffer to visible windows client area
	StretchBlt(hDC, 0, 0, ClientWidth, ClientHeight, hMemDC, 0, 0, 320, 200,  SRCCOPY);

	// put back the default bitmap
	SelectObject(hMemDC, DefaultBitmap);
	
	// delete the memory dc
	DeleteDC(hMemDC);

	// delete the default bitmap
	DeleteObject(DefaultBitmap);

	// release the engines window dc 
	ReleaseDC(ghWnd,hDC);
}

//
// This method copies a loaded buffer (read PCX file) into the double buffer.
//
void copyscrn (char *Buffer) 
{ 
	 memcpy(double_buffer, Buffer, 64000ul);
}

//
// This is a delay that can be cancelled by pressing SPACE or ESCape. 
// It is used during the start-up screens. 
//
void delay2(long time) // time is in seconds. 
{ 
    time_t start; 
 
    start= clock(); 
    while (KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE)) // Wait for the keys to clear. 
        ; 
    while ((clock() - start) < time*CLK_TCK) // Start the delay 
    { 
        if (KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE)) // if space or ESC, break out. 
            break; 
    } 
} 

//
// This function clears te screen to the color "C", by blasting the 
// byte to the double buffer. This used to use a rep movsw, but now we're
// in windows, and I don't know if that will work anymore.
//
void clearscrn(char c) 
{ 
	memset(double_buffer, c, 64000ul);
} 

//
// Deallocates all of the tables. Used when the engine is shut down.
//
void free_tables(void) 
{ 
    int i,j; 

	for (i = 0; i < Num_Animations; ++i) 
		WallTextures[Animations[i].Txt_Nbr] = NULL;
    for (i = 0; i < Num_LightAnimations; ++i) 
		LightMapTiles[Light_Animations[i].Txt_Nbr] = NULL;
    if (world != NULL) 
        free(world); 
    if (FloorMap != NULL) 
        free(FloorMap); 
    if (CeilMap != NULL) 
        free(CeilMap); 
    if (ZoneMap != NULL) 
        free(ZoneMap); 
    if (CeilLightMap != NULL) 
        free(CeilLightMap); 
    if (FloorLightMap != NULL) 
        free(FloorLightMap); 
    for (i=0; i<Num_Textures; ++i) 
        if (WallTextures[i] != NULL) 
            free (WallTextures[i]); 
    for (i=0; i<Num_Light_Tiles; ++i) 
        if (LightMapTiles != NULL) 
            free(LightMapTiles); 
    if (tan_table != NULL) 
        free(tan_table); 
    if (inv_tan_table != NULL) 
        free(inv_tan_table); 
    if (y_step != NULL) 
        free(y_step); 
    if (x_step != NULL) 
        free(x_step); 
    if (cos_table != NULL) 
        free(cos_table); 
    if (sin_table != NULL) 
        free(sin_table); 
    if (inv_cos_table != NULL) 
        free(inv_cos_table); 
    if (inv_sin_table != NULL) 
        free(inv_sin_table); 
    if (sliver_factor != NULL) 
        free(sliver_factor); 
    if (Floor_inv_cos_table != NULL) 
        free(Floor_inv_cos_table); 
    if (Floor_cos_table != NULL) 
        free(Floor_cos_table); 
    if (Floor_sin_table != NULL) 
        free(Floor_sin_table); 
    if (Floor_Dx_table != NULL) 
        free (Floor_Dx_table); 
    if (LightLevel != NULL) 
        free(LightLevel); 
    if (LightMap != NULL) 
        free(LightMap); 
    if (FogMap != NULL) 
        free(FogMap); 
    if (Translucency != NULL) 
        free(Translucency); 
    for (j = 0; j <= Num_Sprites; ++j) 
        for (i=0; i < 8; ++i) 
            if (Sprites[j].Frames[i] != NULL) 
                free (Sprites[j].Frames[i]); 
    for (j = 0; j < Num_Animations; ++j) 
        for (i=0; i < Animations[j].Nbr_Frames; ++ i) 
            if (Animations[j].Frames[i] != NULL) 
                free(Animations[j].Frames[i]); 
    for (i=0; i < Num_Light_Tiles ; ++i) 
        if (LightFlags[i].PulsePattern != NULL) 
            free(LightFlags[i].PulsePattern); 
	if (FireSrc != NULL)
 		free(FireSrc);
 	if (DblBuff != NULL)
 		DeleteObject(DblBuff);
} 

//
// This prints an error message, clears all the tables, and informs the
// message loop that we need to exit the program.
//
int Die(char *string)
{
    free_tables(); 
	MessageBox(ghWnd, string, lpszTitle, MB_APPLMODAL | MB_ICONERROR | MB_OK );
    SendMessage(ghWnd, WM_DESTROY, 0, 0);
	return -1;
}

//
//   Here is were we load all the math tables. This is MUCH faster than 
// using the trig functions. We will also pre-calculate a couple of other 
// things here as well. 
// 
int Build_Tables(void) 
{ 
    int   tmp; 
    int   i; 
    FILE *handle; 
 
    // Step 1. Initialize static arrays, pointers, and counters. 

	// Clear the global palette.
    for (i = 0; i < 256; ++i) 
    { 
        palette[i][0] = 0; 
        palette[i][1] = 0; 
        palette[i][2] = 0; 
    } 
    // Then set up the WallTextures table. 
    for (i=0; i < 80; ++i) 
    { 
        WallTextures[i] = NULL; 
        TextureFlags[i].AnimNbr = -1; 
    } 
    // Then set up the LightMapTextures table. 
    for (i=0; i < 30; ++i) 
    { 
        LightMapTiles[i] = NULL; 
        LightFlags[i].AnimNbr = -1; 
    } 
    // And clean up the OpeningDoors table. 
    for (i=0; i<10; ++i) 
        Opening_Doors[i].USED = 0; 
    // Initialize the light flags pattern param. 
    for (i=0; i < 30 ; ++i) 
    { 
        LightFlags[i].PulsePattern = NULL; 
        LightFlags[i].CurrPatternIndx = 0; 
    } 
    ZoneAttr[0].inside = 0; 
    ZoneAttr[0].ambient = 0; 
    ZoneAttr[0].fog = 0; 
    for (tmp  = 0; tmp <10 ; ++tmp) 
        for (i=0; i < 8; ++i) 
            Sprites[tmp].Frames[i] = NULL; 
 
 
    // Step 2. Allocate the maps and the tables.
    if (!(world = (char *)malloc(4096))) 
        return Die("Allocation Error while creating world map"); 
    if (!(FloorMap = (char *)malloc(4096))) 
        return Die("Allocation Error while creating floor map"); 
    if (!(CeilMap = (char  *)malloc(4096))) 
        return Die("Allocation Error while creating ceiling map"); 
    if (!(ZoneMap = (char  *)malloc(4096))) 
        return Die("Allocation Error while creating zone map"); 
    if (!(CeilLightMap = (char  *)malloc(4096))) 
        return Die("Allocation Error while creating ceiling light map"); 
    if (!(FloorLightMap = (char  *)malloc(4096))) 
        return Die("Allocation Error while creating floor light map"); 
    if (!(tan_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating tangent table"); 
    if (!(inv_tan_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating inverse tangent table"); 
    if (!(y_step = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating y step table"); 
    if (!(x_step = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating x step table"); 
    if (!(cos_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating cosine table"); 
    if (!(sin_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating sine table"); 
    if (!(inv_cos_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating inverse cosine table"); 
    if (!(inv_sin_table = (long  *)malloc(sizeof(long) * (ANGLE_360+1)))) 
        return Die("Allocation Error while creating inverse sine table"); 
    if (!(sliver_factor = (long  *)malloc(sizeof(long)*600))) 
        return Die("Allocation Error while creating sliver scaling table"); 
    if (!(Floor_inv_cos_table = (long  *)malloc(sizeof(long)*(ANGLE_360+1)))) 
        return Die("Allocation Error while creating Floor inverse cosine table"); 
    if (!(Floor_cos_table = (long  *)malloc(sizeof(long)*(ANGLE_360+1)))) 
        return Die("Allocation Error while creating Floor cosine table"); 
    if (!(Floor_sin_table = (long  *)malloc(sizeof(long)*(ANGLE_360+1)))) 
        return Die("Allocation Error while creating Floor sine table"); 
    if (!(Floor_Dx_table = (long  *)malloc(sizeof(long)*10000L))) 
        return Die("Allocation Error while creating Floor DX table"); 
    if (!(LightLevel = (char  *)malloc(sizeof(char)* MAX_DISTANCE ))) // Maximum distance. 
        return Die("Allocation Error while creating Light levels"); 
    if (!(LightMap = (char  *)malloc(sizeof(char)*(PALETTE_SIZE*MAX_LIGHT_LEVELS))))  // 128 levels of 256 colors. 
        return Die("Allocation Error while creating Light map"); 
    if (!(FogMap = (char  *)malloc(sizeof(char)*(PALETTE_SIZE*MAX_LIGHT_LEVELS))))  // 128 levels of 256 colors. 
        return Die("Allocation Error while creating Fog Map"); 
    if (!(Translucency = (char  *)malloc(65536ul))) // Mixing 256 colors with 256 colors. 
        return Die("Allocation Error while creating Translucency Map"); 
    
	// Step 3. Read the tables from the data file... 
    if((handle=fopen("tables.dat","rb")) == NULL) 
		return Die("Error reading data tables.\n");
 
    fread(sliver_factor,1l,(sizeof(long)*600L),handle); 
    fread(Floor_Row_Table,1l,(sizeof(long)*151L),handle); 
    fread(Floor_Dx_table,1,(sizeof(long)*10000L),handle); 
    fread(row,1,(sizeof(long)*200L),handle); 
    fread(Sprite_Frame,1,(sizeof(int)*361L),handle); 
    fread(tan_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(inv_tan_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(Floor_cos_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(Floor_sin_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(y_step,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(x_step,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(inv_cos_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(inv_sin_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(sin_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(Floor_inv_cos_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(cos_table,1,(sizeof(long) * (ANGLE_360+1L)),handle); 
    fread(LightLevel,1,(sizeof(char)* MAX_DISTANCE ), handle); 
    fread(FogMap, MAX_LIGHT_LEVELS, PALETTE_SIZE, handle); 
    fread(LightMap,PALETTE_SIZE,MAX_LIGHT_LEVELS, handle); 
    fread(Translucency,1,65535l, handle); 
    
    fclose(handle); 

	return 1;
} 
 
//
//   This method re-calculates the light level table based on the current torch level. When the
// user changes the torch level, this method re-calculates the lighting table.
//
void CalcTorchLevel(int level) 
{ 
    long dist; 
    float ratio; 
    // There are 8 levels of torch. (1 = blackness, 8 = full) 
    if ((level < 1) || (level > 8)) return; 
 
    for (dist = 1; dist < MAX_DISTANCE; dist++) 
    { 
        ratio = (float)(MAX_LIGHT_LEVELS/(2.0 * (9 - level))) / dist * 2.0; // Normal intensity is mid-range. 
        if (ratio > 1.0) ratio = 1.0; 
        *(LightLevel + dist) = (char)( ratio * MAX_LIGHT_LEVELS ); 
    }  
} 
 
//
// This function saves a 256 color PCX file, of a specified filename and size, from the
// indicated buffer (usually the double buffer).
// 
// Return Codes: 0  - Success 
//               -1 - Error 
// 
int Save_Pcx(char *filename, unsigned char  *Buffer, unsigned long width, unsigned long height) 
{ 
    unsigned long count,     // the count for the RLE run. 
                  bcount,    // the count for the line. 
                  index,     // Overall byte count. 
                  TotalSize; 
    unsigned char data;      // The data byte written. 
    FILE          *outfile;  // The file pointer. 
    PCX_HEADER    header;    // The PCX header info. 
 
    outfile = fopen(filename, "wb"); 
    if(!outfile) { 
        return -1; 
    } 
 
    // Build a PCX file header. Must be little endian, so beware 
    header.manufacturer = 10; 
    header.version = 5; 
    header.encoding = 1; 
    header.bits_per_pixel = 8; 
    header.x = 0; 
    header.y = 0; 
    header.width = width-1; 
    header.height = height-1; 
    header.horiz_rez = width; 
    header.vert_rez = height; 
    header.Num_Planes = 1; 
    header.bytes_per_line = width; 
    header.palette_type = 1; 
 
    // Write the PCX file header 
    fwrite((void *)&header,sizeof(PCX_HEADER),1,outfile); 
 
    //  Do run length encoding for pixel runs of 2 to 63.  Add 192 to the 
    //  run count, and write it before the pixel to indicate how many are 
    //  present. Runs are not encoded across line boundaries, so 
    //  bcount keeps track of how many bytes have been written per line, 
    //  and when it hits "width", the run is written even if less than 63. 
 
    bcount = 0; 
    index = 0; 
    TotalSize = width * height; 
    while (index < TotalSize) 
    { 
        if((index < TotalSize-1) && (Buffer[index] == Buffer[index + 1]) && (bcount < width-1)) 
        { 
            // We have a run of two or more 
            data = Buffer[index]; 
            count = 2; 
			while((index + count < TotalSize) && 
				  (data == Buffer[index + count]) && 
				  (count < 63) && (bcount + count < width) ) 
                    count++; 
            index = index + count; 
            bcount = bcount + count; 
            putc((char)(count + 192), outfile); 
            putc(data, outfile); 
        } 
        else 
        { 
            // We are writing one byte.  If byte is 192-255, put a run count of 1 (193) in front of it 
            if(Buffer[index] >= 192) 
                putc((char)193, outfile); 
            putc((char)Buffer[index], outfile); 
            index++; 
            bcount++; 
        } 
        if(bcount == width)
            bcount = 0; 
    } 
 
    // Write the palette 
    // 256 color palette must be prefixed with this byte 
    putc((char)12, outfile); 
    for(index=0; index<256; index++) 
    { 
        putc((char)palette[index][0], outfile); 
        putc((char)palette[index][1], outfile); 
        putc((char)palette[index][2], outfile); 
    } 
    fclose(outfile); 
	return 1;
} 
 
//
// This function loads in a 256 color PCX file, of a specified size, and 
// returns a pointer to it. If load_pal > 0 then it loads the palette as well. 
//
char  *Load_PCX(char *filename, unsigned long size, int load_pal) 
{ 
    unsigned long count,        // the count for the RLE run. 
                  index;        // Overall byte count. main FOR loop. 
    unsigned char data;         // The data byte read in. 
    FILE          *infile;      // The file pointer. 
    PCX_HEADER    header;       // The PCX header info. 
    char          *Pointer;     // Points to the bitmap data. 
    char          errorMsg[128];
 
    // Step 1. Allocate a new bit of memory for the bitmap. 
    if (!(Pointer = (char  *)malloc(size))) 
    { 
        // If there was a problem, quit and complain! 
        sprintf(errorMsg,"Memory error loading PCX File '%s'. \n", filename); 
        Die(errorMsg);
		return NULL;
    } 

    // Step 2: Open the file 
    infile = fopen(filename,"rb"); 
	
    if (!infile) 
    { 
        sprintf(errorMsg,"Error opening PCX File '%s'. \n", filename); 
        Die(errorMsg);
		return NULL;
    } 
 
    // Step 3: Read in the header, and get it out of the way... 
    fread((char *)&header,128,1,infile); 
 
    // Step 4. Check the header to make sure we're reading the correct file type... 
    if ( header.bits_per_pixel != 8)
    { 
        sprintf(errorMsg,"Error opening PCX File. Wrong file type '%s'. \n", filename); 
        Die(errorMsg);
		return NULL;
    } 
 
    // Step 5: Read in the bitmap data. 
    index = 0; 
    while(index < size) // for each byte in the bitmap (WxL), 
    { 
        data = getc(infile); // Get the byte. 
        if (data >= 192) // If it's an RLE run, 
        { 
            count = data-192; // Get the count, 
            data = getc(infile); // and the data, 
            while ((count-- > 0) && (index < size)) // and blast it out. 
            { 
                *(Pointer+index) = (char)data; 
                ++index; 
            } 
        } 
        else // Else if it isn't an RLE, 
        { 
            *(Pointer+index) = (char)data; // just write it. 
            ++index; 
        } 
    } 
    // If specified, load in the palette. 
    if (load_pal > 0) 
    { 
        // Now seek to the palette data, and read that in. 
        fseek(infile,-768l,SEEK_END); 
        for (index = 0; index < 256; ++index) 
        { 
            // Get the data, 
            palette[index][0] = getc(infile); 
            palette[index][1] = getc(infile); 
            palette[index][2] = getc(infile); 
        } 
    } 
    // Step 5: Close the file. 
    fclose(infile); 
    return Pointer; 
} 

// 
// This function takes a bitmap and transposes the columns with the rows. This is 
// because the sliver renderer reads across a row of the bitmap, and draws a column. 
// 
int transpose(char  *Bitmap) 
{ 
    int  rows,  // These two are loop counters. 
         cols; 
    char *temp; // A temporary holding buffer. 
    
    temp = (char  *)malloc(16385); 
    if (temp == NULL) 
		return Die("Memory error transposing PCX File \n"); 

    // Copy the bitmap into the temp array, transposed. 
    for (rows = 0; rows < 128; ++rows) 
        for (cols = 0; cols < 128; ++cols) 
            temp[(128*rows)+cols] = Bitmap[(128*cols)+rows]; 
 
  // Now, copy it back... 
    memcpy(Bitmap,temp,16384); 
    free(temp); 

	return 1;
} 

//
// This function loads in a 320x200, 256 color PCX file, and 
// places it into the BkgBuffer. 
// 
int load_bkgd(char *filename) 
{ 
    // Step 1. Clear the background buffer. 
    if (BkgBuffer) 
        free(BkgBuffer); 
    // Step 2. Load the PCX file. 
    BkgBuffer = Load_PCX(filename,64000l,1);
	if (NULL == BkgBuffer)
		return -1;
	return 1;
} 

// 
// This function loads in a wall texture map. This will be a 256 color .PCX 
// file. The palette must be the same as that which is loaded in the background 
// The routine also calls Transpose, to order the rows and cols correctly. 
// 
//  filename is the name of the PCX file on disk.
//  offset is the texture number (as specified in the map file. 
// 
int Load_Wall(char *filename, int offset) 
{ 
    // Step 1. Decrement the offset since the array starts at 0. 
    --offset; 
 
    // Step 2. Check to see if a texture is already there. If so, then delete it. 
    if (WallTextures[offset] != NULL) 
        free(WallTextures[offset]); 
 
  // Step 3. Load the PCX file. 
    WallTextures[offset] = Load_PCX(filename,16384l,0); 
	if (NULL == WallTextures[offset])
		return -1;
 
    // Step 4. We need to transpose the rows and coumns now, because the renderer 
    // traverses a ROW of the texture to draw a COLUMN of the screen (faster). 
    return transpose(WallTextures[offset]); 
} 

// 
// This function loads in the light tile bitmap. This will be a 256 color .PCX 
// file. The palette is unimportant. The color numbers will add to the lighting of the area. 
//  offset is the bitmap number (as specified in the map file.) 
// 
int Load_Light_Tile(char *filename,int offset) 
{ 
    // Step 1. Check to see if a texture is already there. If so, then delete it. 
    if (LightMapTiles[offset] != NULL) 
        free(LightMapTiles[offset]); 
    // Step 2. Load the PCX file. 
    LightMapTiles[offset] = Load_PCX(filename,16384l,0); 
	if (NULL == LightMapTiles[offset])
	{
		return -1;
	}

    // Step 3. We need to transpose the rows and coumns now, because the renderer 
    // traverses a ROW of the texture to draw a COLUMN of the screen (faster). 
    return transpose(LightMapTiles[offset]); 
} 

//
// Loads the bitmaps for a sprite object. The files are called [Filename][1-8].pcx 
// where [Filename] is the base filename (up to 7 characters), and [1-8] is a 
// single digit from 1 to 8 that is appended to the filename which represents the 
// frame number (each frame represents a 45 degree view of the object. This name formatting 
// is not done here, but in Load_Map ( ). This routine just loads in 1 bitmap. 
// The function malloc's space for the bitmap, loads it in, and returns a pointer to it. 
// 
char *Load_Sprite (char *filename) 
{ 
    char *Pointer;     // Points to the sprite data. 
 
    // Step 1. Load the PCX file. 
    Pointer = Load_PCX(filename,16384l,0); 
	
	// Step 2. We need to transpose the rows and coumns now, because the renderer 
    // traverses a ROW of the texture to draw a COLUMN of the screen (faster). 
	if (Pointer != NULL)
		transpose(Pointer); 
 
  // Step 3. Return the pointer to the bitmap. 
    return Pointer; 
} 

//
// Translates a character in a map definition to the numeric value that is to be stored in
// the map table. 0-9 are 0-9, A-Z are 10 - 34, and space is also 0.
//
unsigned char translateChar(unsigned char ch)
{
	if (ch == ' ') // Space maps to zone 0. 
		return 0; 
	else if ((ch >= '0') && (ch <= '9')) // 0-9 is 0-9 
		return ch - '0'; 
	else if ((ch >= 'A') && (ch <= 'Z')) // A-Z is 10-34 
		return ch - 'A' + 10; 

	// Default, this shold never happen.
	return 0;
}

//
//  Clears out the previously loaded map, and prepares the system for a new one.
//
void Clear_Map(void)
{
	int i,j;  // Generic loop counters.
	
	// Step 1. Reset the triggers and objects.
    Num_Triggers = 0; 
    Num_Objects = 0; 

	// Step 2. Reset the sprites, and free the memory for the frames.
    for (j  = 0; j < Num_Sprites; ++j) 
        for (i=1; i<= 8; ++i) 
            if (Sprites[j].Frames[i-1] != NULL) 
            { 
                free(Sprites[j].Frames[i-1]); 
                Sprites[j].Frames[i-1] = NULL; 
            } 
    Num_Sprites = 0; 

	// Step 3. Clear the wall / door textures, their animations, texture flags, etc.
	
	for (i = 0; i < Num_Animations; ++i) 
		WallTextures[Animations[i].Txt_Nbr] = NULL;

    for (i = 0; i < Num_LightAnimations; ++i) 
		LightMapTiles[Light_Animations[i].Txt_Nbr] = NULL;

    for (j = 0; j < Num_Textures; ++j) 
    { 
        if (WallTextures[j] != NULL) 
        { 
            free(WallTextures[j]); 
            WallTextures[j] = NULL; 
        } 
        if (TextureFlags[j].AnimNbr > -1) 
        { 
            if (Animations[TextureFlags[j].AnimNbr].Frames != NULL ) 
            { 
                for (i = 0; i < Animations[TextureFlags[j].AnimNbr].Nbr_Frames; ++i) 
                    if (Animations[TextureFlags[j].AnimNbr].Frames[i] != NULL) 
                    { 
                        free(Animations[TextureFlags[j].AnimNbr].Frames[i]); 
                        Animations[TextureFlags[j].AnimNbr].Frames[i] = NULL; 
                    } 
                free(Animations[TextureFlags[j].AnimNbr].Frames); 
                Animations[TextureFlags[j].AnimNbr].Frames = NULL; 
            } 
        } 
    } 
    Num_Textures = 0; 
	
    // Step 4. Clear the lighting tiles, and the animations, etc.
	for (j = 0; j <= Num_Light_Tiles ; ++j) 
    { 
        if (LightMapTiles[j] != NULL) 
        { 
            free(LightMapTiles[j]); 
            LightMapTiles[j] = NULL; 
        } 
        if (LightFlags[j].PulsePattern != NULL) 
        { 
            free(LightFlags[j].PulsePattern); 
            LightFlags[j].PulsePattern = NULL; 
        } 
        if (LightFlags[j].AnimNbr > -1) 
        { 
            if (Light_Animations[LightFlags[j].AnimNbr].Frames != NULL ) 
            { 
                for (i = 0; i < Light_Animations[LightFlags[j].AnimNbr].Nbr_Frames; ++i) 
                    if (Light_Animations[LightFlags[j].AnimNbr].Frames[i] != NULL) 
                    { 
                        free(Light_Animations[LightFlags[j].AnimNbr].Frames[i]); 
                        Light_Animations[LightFlags[j].AnimNbr].Frames[i] = NULL; 
                    } 
                free(Light_Animations[LightFlags[j].AnimNbr].Frames); 
                Light_Animations[LightFlags[j].AnimNbr].Frames = NULL; 
            } 
        } 
    } 
    Num_Light_Tiles = 0; 	

	// Step 5. Clear out the zone map, and set up the default zone.
    memset(ZoneMap,0,4096); 
	ZoneAttr[0].ambient = 16;
	ZoneAttr[0].inside = 1;
	ZoneAttr[0].fog = 0;

	return;
}


// 
//   This routine reads and translates the map file, creates and fills the map arrays, 
//  and loads the textures into memory. 
// 
int Load_Map(long &initx, long &inity, long &initang, char *MapFileName) 
{ 
    FILE *fp;          // Filepointer for the map file. 
    int  row,          // Row number that we are readng in. 
         column,       // Column number that we are reading in. 
         txtnum,       // The texture to be assigned to the cell. 
         cs,           // The following is Door info, and has already been described in detail. 
         rs, 
         ce, 
         re, 
         de, 
         tp, 
         tl, 
         sec, 
         type, 
         sidetxt, 
         spd, 
         i,               // Generic loop index. 
         sp_num,          // The number of the sprite we're loading. 
         sp_txt; 
    long sp_initx,        // The sprite's initial X coordinate. 
         sp_inity,        // The sprite's initial Y coordinate. 
         sp_initang;      // The sprite;s initial angle. 
    char line[257],       // Temporary buffer to hold the current line of the map file. 
         typ[100],        // This holds string data we're reading from the line. 
         ch,              // Temporary holding var. for cell data during translation. 
         trg_parms[100], 
         sp_basename[12]; // Base filename for the sprite's PCX files. 
    
	
	// Step 1. Clear out the old map.
	Clear_Map();
 
    // Step 2. Set up the 1st light map (map 0). This is used as the default.
    if (LightMapTiles[0] != NULL) 
        free(LightMapTiles[0]); 
    LightMapTiles[0] = (char  *)calloc(1,16384l); 
    
    // Step 3.  Read in world data. 
    
	// Step 3a. Open the map file.
    if ((fp = fopen(MapFileName,"r")) == NULL) 
        return Die("Error loading map file.");
    
	// Step 3b. Read the file in, line by line.
    fgets(line,256,fp); 
    while (!feof(fp)) 
    { 
        memset(typ,0,100); 
		memset(trg_parms,0,100);
        sscanf(line,"%s",typ); // This step cleans up much garbage that can be present. 
		
		// Step 3c. Skip over any comment lines.
        if (!strcmpi(typ,"//"))
        { 
            fgets(line,256,fp); // Get the next line here.. (We test for EOF at the beginning..) 
            continue; 
        } 
        // Step 3d. Load the player's initial position and orientation 
        if (!strcmpi(typ,"P")) 
        { 
            sscanf(line,"%*s %ld %ld %ld",&initx, &inity, &initang); 
            // Translate into the fine coordinate system. 
            initx = initx * 128+64; 
            inity = inity * 128+64; 
            initang *= 4; 
        } 
        // 3e. Load in a sprite definition.. 
        else if (!strcmpi(typ,"S")) 
        { 
            // Parameters are: [Number] [Base Filename] [Translucent] 
            sscanf(line,"%*s %d %7s %d",&sp_num, sp_basename,&tp); 
            for (i=1; i<= 8; ++i) 
            { 
                sprintf(typ,"%s%d.pcx",sp_basename,i); 
                if (Sprites[sp_num].Frames[i-1] != NULL) 
                    free(Sprites[sp_num].Frames[i-1]); 
                Sprites[sp_num].Frames[i-1] = Load_Sprite(typ); 
            } 
            Sprites[sp_num].translucent = tp; 
            if (sp_num > Num_Sprites) 
                Num_Sprites = sp_num; 
        } 
        // 3f.  Load in an object definition. (an instance of a sprite) 
        else if (!strcmpi(typ,"O")) 
        { 
            // Parameters are: [Number] [Initial X] [Initial Y] [Initial Angle] 
            sscanf(line,"%*s %d %d %d %d %d ",&sp_num, &sp_txt, &sp_initx, &sp_inity, &sp_initang ); 
            Objects[sp_num].sprite_num = sp_txt; 
            Objects[sp_num].x = sp_initx*128+64; 
            Objects[sp_num].y = sp_inity*128+64; 
            Objects[sp_num].angle = sp_initang * 4; 
            Objects[sp_num].state = 0; 
            if (sp_num > Num_Objects) 
                Num_Objects = sp_num; 
        } 
        // Step 3g. Load a texture definition. 
        else if (!strcmpi(typ,"T")) 
        { 
            // Parameters are: [NUMBER] [Filename] [transparent] [translucent] [recessed] [Walk Through]
            sscanf(line,"%*s %d %s %d %d %d %d",&txtnum,typ,&tp,&tl,&re,&de); 
            // Track how many we've loaded. 
            if (txtnum > Num_Textures) 
                Num_Textures = txtnum; 
            // We need to clear these, since this is normal texture. 
            TextureFlags[txtnum].IsTransparent = tp; 
            TextureFlags[txtnum].IsTranslucent = tl; 
            TextureFlags[txtnum].IsRecessed = re; 
            TextureFlags[txtnum].CanWalkThru = de; 
            TextureFlags[txtnum].IsDoor = 0; 
            TextureFlags[txtnum].AnimNbr = -1; 
            // Load the bitmap. 
            Load_Wall(typ,txtnum); 
        } 
        // Step 3h. Load a door definition. 
        else if (!strcmpi(typ,"D")) 
        { 
            // Parameters are: [Number] [Door Filename] [Side Number] [Col Start] [Row Start] [Col End] [Row End] [Speed] [Delay] [transparent] [Translucent] [Secret] 
            sscanf(line,"%*s %d %s %d %d %d %d %d %d %d %d %d %d %d",&txtnum,typ,&sidetxt,&type,&cs,&rs,&ce,&re,&spd,&de,&tp,&tl,&sec); 
            // Still, keep trck of the number, 
            if (txtnum > Num_Textures) 
                Num_Textures = txtnum; 
            // Since this is a door, we have lots more to set... 
            TextureFlags[txtnum].IsTransparent = tp; 
            TextureFlags[txtnum].CanWalkThru = 0; 
            TextureFlags[txtnum].IsDoor = 1; 
            TextureFlags[txtnum].DoorType = type; 
            TextureFlags[txtnum].DoorSideTxt = sidetxt; 
            TextureFlags[txtnum].bc = cs; 
            TextureFlags[txtnum].br = rs; 
            TextureFlags[txtnum].ec = ce; 
            TextureFlags[txtnum].er = re; 
            TextureFlags[txtnum].delay = de; 
            TextureFlags[txtnum].speed = spd; 
            TextureFlags[txtnum].IsTranslucent = tl; 
            TextureFlags[txtnum].IsSecretDoor = sec; 
            TextureFlags[txtnum].IsRecessed = (sec == 1?0:1); 
            TextureFlags[sp_num].AnimNbr = -1; 
            Load_Wall(typ,txtnum); 
        } 
        // Step 3i. Load in the actual wall data. 
        else if (!strcmpi(typ,"M")) 
        { 
            sscanf(line,"%*s %d %d",&WORLD_ROWS,&WORLD_COLUMNS); 
            // Make sure the map size is OK. We set a maximum value above... 
            if ((WORLD_ROWS < 10) || (WORLD_ROWS > MAX_WORLD_ROWS) || (WORLD_COLUMNS < 0) || (WORLD_COLUMNS > MAX_WORLD_COLS)) 
				Die("Invalid map size.\n"); 
            
            // Convert to fine coordinates. 
            WORLD_X_SIZE = 128l*WORLD_COLUMNS; 
            WORLD_Y_SIZE = 128l*WORLD_ROWS; 
            // Now read in the cell data. 
            for (row=0; row<WORLD_ROWS; row++) // For all rows... 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) // For all columns... 
                { 
                    while((ch = getc(fp))==10) // filter out CR 
                    {} 
                    *(world+(row<<6)+column) = translateChar(ch); // Place the converted value in the map. 
                } 
            } 
        } 
		// Step 3j. Load in the Floor texture map.
        else if (!strcmpi(typ,"F"))
        { 
			// The floor must have the same dimensions as the walls. 
            for (row=0; row<WORLD_ROWS; row++) 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) 
                { 
                    while((ch = getc(fp))==10) // filter out CR 
                    {} 
                    *(FloorMap+(row<<6)+column) = translateChar(ch);  // Add it to the floor map. 
                } 
            } 
        } 
		// Step 3k. Load in the ceiling texture map.
        else if (!strcmpi(typ,"C")) 
        { 
            for (row=0; row<WORLD_ROWS; row++) // For every row 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) // For every column 
                { 
                    while((ch = getc(fp))==10) // read in a char, and filter out CR 
                    {} 
                    *(CeilMap+(row<<6)+column) = translateChar(ch); // Add it to the ceiling map. 
                } 
            } 
        } 
		// Step 3l. Load in the zone map.
        else if (!strcmpi(typ,"Z"))
        { 
            for (row=0; row<WORLD_ROWS; row++) // For every row 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) // For every column 
                { 
                    while((ch = getc(fp))==10) // read in a char, and filter out CR 
                    {} 
                    *(ZoneMap+(row<<6)+column) = translateChar(ch); // Add it to the ceiling map. 
                } 
            } 
        } 
		// Step 3m. Load in the Floor Lighting map.
        else if (!strcmpi(typ,"L"))
        { 
            for (row=0; row<WORLD_ROWS; row++) // For every row 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) // For every column 
                { 
                    while((ch = getc(fp))==10) // read in a char, and filter out CR 
                    {} 
                    *(FloorLightMap+(row<<6)+column) = translateChar(ch); // Add it to the ceiling map. 
                } 
            } 
        } 
		// Step 3n. Load in the Ceiling Lighting Map.
        else if (!strcmpi(typ,"I"))
        { 
            for (row=0; row<WORLD_ROWS; row++) // For every row 
            { 
                for (column=0; column<WORLD_COLUMNS; column++) // For every column 
                { 
                    while((ch = getc(fp))==10) // read in a char, and filter out CR 
                    {} 
                    *(CeilLightMap+(row<<6)+column) = translateChar(ch); // Add it to the ceiling map. 
                } 
            } 
        } 
		// Step 3o. Load in a lighting tile definition.
        else if (!strcmpi(typ,"G"))
        { 
            // Parameters are: [Tile Number] [Filename] [Pulsed] [Pulse type] [Args] (1 or 2 args) 
            //  Pulse type: regular -> args is [pulse width] and [Period] 
            //              custom  -> arg is : [pattern] (all 1s and 0s) 
            sscanf(line,"%*s %d %s %d",&txtnum,typ,&tp); 
            // Track how many we've loaded. 
            if (txtnum > Num_Light_Tiles) 
                Num_Light_Tiles = txtnum; 
            // Load the bitmap. 
            Load_Light_Tile(typ,txtnum); 
            LightFlags[txtnum].OriginalBitmap = LightMapTiles[txtnum]; 
            LightFlags[txtnum].AnimNbr = -1; 
            // Is the light a pulsed light? 
            if (tp == 1) 
            { 
                // Get the pulse type. 
                sscanf(line,"%*s %*d %*s %*d %d",&tp); // Pulse type will be either 1 or 2 
                if ( tp == 1) // Regular pulse. Get pulse width and period... 
                { 
                    sscanf(line,"%*s %*d %*s %*d %*d %d %d",&tp,&re); 
                    LightFlags[txtnum].LightType = 1; 
                    LightFlags[txtnum].PulseWidth = tp; 
                    LightFlags[txtnum].Period = re; 
                } 
                else if ( tp == 2 ) // Custom pulse type. Get the mask. 
                { 
                    sscanf(line,"%*s %*d %*s %*d %*d %s",typ); 
                    LightFlags[txtnum].LightType = 2; 
                    if (LightFlags[txtnum].PulsePattern != NULL) 
                        free(LightFlags[txtnum].PulsePattern); 
                    LightFlags[txtnum].PulsePattern = (char  *)malloc(strlen(typ)+1); 
                    strcpy(LightFlags[txtnum].PulsePattern,typ); 
                } 
            } 
        } 
		// Step 3p. Load in a Zone Option.
        else if (!strcmpi(typ,"N")) // Load in the zone options. 
        { 
            // N [NUMBER] [Ambient] [Inside] [Fog] 
            sscanf(line,"%*s %d %d %d",&txtnum, &de, &tp, &re); 
            ZoneAttr[txtnum].ambient = de; 
            ZoneAttr[txtnum].inside = tp; 
            ZoneAttr[txtnum].fog = re;
        } 
		// Step 3q. Load in an animated texture.
        else if (!strcmpi(typ,"A"))
        { 
            // A [Number] [Base File name] [Number of Textures] [Delay] [Triggered] [Transparent] [Translucent] [Recessed] [Walk Through] 
            sscanf(line,"%*s %d %7s %d %d %d %d %d %d %d",&sp_num, sp_basename,&tp,&de,&re,&cs,&ce,&rs,&tl); 
            // Now, clear out any previously loaded animation for this texture... 
            if ((TextureFlags[sp_num].AnimNbr > -1) && (Animations[TextureFlags[sp_num].AnimNbr].Frames != NULL )) 
            { 
                for (i = 0; i < Animations[TextureFlags[sp_num].AnimNbr].Nbr_Frames; ++i) 
                    if (Animations[TextureFlags[sp_num].AnimNbr].Frames[i] != NULL) 
                    { 
                        free(Animations[TextureFlags[sp_num].AnimNbr].Frames[i]); 
                        Animations[TextureFlags[sp_num].AnimNbr].Frames[i] = NULL; 
                    } 
                free(Animations[TextureFlags[sp_num].AnimNbr].Frames); 
                Animations[TextureFlags[sp_num].AnimNbr].Frames = NULL; 
            } 
            Animations[Num_Animations].Flagged = re; 
            Animations[Num_Animations].Txt_Nbr = sp_num-1; 
            Animations[Num_Animations].Delay = de; 
            Animations[Num_Animations].Timer = de; 
            Animations[Num_Animations].Curr_Frame = 0; 
            Animations[Num_Animations].Nbr_Frames = tp; 
            TextureFlags[sp_num].IsTransparent = cs; 
            TextureFlags[sp_num].CanWalkThru = tl; 
            TextureFlags[sp_num].IsDoor = 0; 
            TextureFlags[sp_num].delay = de; 
            TextureFlags[sp_num].IsTranslucent = ce; 
            TextureFlags[sp_num].IsSecretDoor = 0; 
            TextureFlags[sp_num].IsRecessed = rs; 
            TextureFlags[sp_num].AnimNbr = Num_Animations; 
            // Allocate space for the pointers. **Frames 
            Animations[Num_Animations].Frames = (char  **)malloc((sizeof(char *))*tp); 
            // Load the bitmaps. 
            for (i=1; i<= tp; ++i) 
            { 
                sprintf(typ,"%s%d.pcx",sp_basename,i); 
                Animations[Num_Animations].Frames[i-1] = Load_Sprite(typ); 
            } 
            WallTextures[sp_num-1] = Animations[Num_Animations].Frames[0]; 
            ++Num_Animations; 
        } 
		// Step 3r. Load in an animated light tile.
        else if (!strcmpi(typ,"H"))
        { 
            // [Txt Number] [Base File name] [Number of Textures] [Delay] [Triggered] 
            sscanf(line,"%*s %d %7s %d %d %d",&sp_num, sp_basename,&tp,&de,&re); 
            // Now, clear out any previously loaded animation for this texture... 
            if ((LightFlags[sp_num].AnimNbr > -1) && (Light_Animations[LightFlags[sp_num].AnimNbr].Frames != NULL )) 
            { 
                for (i = 0; i < Light_Animations[LightFlags[sp_num].AnimNbr].Nbr_Frames; ++i) 
                    if (Light_Animations[LightFlags[sp_num].AnimNbr].Frames[i] != NULL) 
                    { 
                        free(Light_Animations[LightFlags[sp_num].AnimNbr].Frames[i]); 
                        Light_Animations[LightFlags[sp_num].AnimNbr].Frames[i] = NULL; 
                    } 
                free(Light_Animations[LightFlags[sp_num].AnimNbr].Frames); 
                Light_Animations[LightFlags[sp_num].AnimNbr].Frames = NULL; 
            } 
            Light_Animations[Num_LightAnimations].Flagged = re; 
            Light_Animations[Num_LightAnimations].Txt_Nbr = sp_num; 
            Light_Animations[Num_LightAnimations].Delay = de; 
            Light_Animations[Num_LightAnimations].Timer = de; 
            Light_Animations[Num_LightAnimations].Curr_Frame = 0; 
            Light_Animations[Num_LightAnimations].Nbr_Frames = tp; 
            // Allocate space for the pointers. **Frames 
            Light_Animations[Num_LightAnimations].Frames = (char  **)malloc((sizeof(char *))*tp); 
            LightFlags[txtnum].AnimNbr = Num_LightAnimations; 
            // Load the bitmaps. 
            for (i=1; i<= tp; ++i) 
            { 
                sprintf(typ,"%s%d.pcx",sp_basename,i); 
                Light_Animations[Num_LightAnimations].Frames[i-1] = Load_Sprite(typ); 
            } 
            LightMapTiles[sp_num] = Light_Animations[Num_LightAnimations].Frames[0]; 
            ++Num_LightAnimations; 
        } 
		// Step 3s. Load in a trigger.
        else if (!strcmpi(typ,"R"))
        { 
            // [Trgr Nbr] [Trgr Area Type] [Area Params]... [Space Flag] [Effect Type] [Effect Params]... 
            sscanf(line,"%*s %d %d ",&sp_num,&tp); 
            Num_Triggers++; 
            Triggers[Num_Triggers].Trigger_Nbr = sp_num; 
            Triggers[Num_Triggers].Trigger_Area_Type = tp; 
            // Now depending on the trigger area type, we have 3 types of area parameters. 
            switch (tp) 
            { 
            case 1: // Rectangle 
                sscanf(line,"%*s %*d %*d %d %d %d %d %d %98c",&cs,&rs,&ce,&re,&tl,trg_parms); 
                Triggers[Num_Triggers].X1 = cs; 
                Triggers[Num_Triggers].Y1 = rs; 
                Triggers[Num_Triggers].X2 = ce; 
                Triggers[Num_Triggers].Y2 = re; 
                break; 
            case 2: // Circle 
                sscanf(line,"%*s %*d %*d %d %d %d %d %98c",&cs,&rs,&ce,&tl,trg_parms); 
                Triggers[Num_Triggers].X1 = cs; 
                Triggers[Num_Triggers].Y1 = rs; 
                Triggers[Num_Triggers].Radius = ce; 
                break; 
            case 3: // Time --- Not implemented yet... 
                break; 
            } 
            // Now we need to mark the Trigger Map with the location of this trigger. 
            if (Triggers[Num_Triggers].Trigger_Area_Type == 1) 
                for (column = Triggers[Num_Triggers].X1>>7; column <= Triggers[Num_Triggers].X2>>7; ++ column) 
                    for (row = Triggers[Num_Triggers].Y1>>7; row <= Triggers[Num_Triggers].Y2>>7; ++row) 
                        if (TriggerMap[column][row] != 0) 
                            TriggerMap[column][row] = 100; 
                        else 
                            TriggerMap[column][row] = Triggers[Num_Triggers].Trigger_Nbr; 
            else if (Triggers[Num_Triggers].Trigger_Area_Type == 2) 
                for (column = (Triggers[Num_Triggers].X1 - Triggers[Num_Triggers].Radius )>>7; column <= (Triggers[Num_Triggers].X1 + Triggers[Num_Triggers].Radius )>>7; ++ column) 
                    for (row = (Triggers[Num_Triggers].Y1-Triggers[Num_Triggers].Radius)>>7; row <= (Triggers[Num_Triggers].Y1+Triggers[Num_Triggers].Radius)>>7; ++row) 
                        if (TriggerMap[column][row] != 0) 
                            TriggerMap[column][row] = 100; 
                        else 
                            TriggerMap[column][row] = Triggers[Num_Triggers].Trigger_Nbr; 
 
            // Now, depending on the trigger type, read in the rest of the parameters... 
            Triggers[Num_Triggers].Trigger_Type = tl; 
            switch(tl) 
            { 
            case 1: 
            case 2: 
            case 3: // X/Y Coords. ( open / close / lock door ) 
                sscanf(trg_parms,"%d %d",&cs,&rs); 
                Triggers[Num_Triggers].MapX = cs; 
                Triggers[Num_Triggers].Mapy = rs; 
                break; 
            case 4: 
            case 5: 
            case 16: 
            case 17: // Texture Number ( Start / stop animated texture / Light ) 
                sscanf(trg_parms,"%d",&cs); 
                Triggers[Num_Triggers].TxtNbr = cs; 
                break; 
            case 6: // Change ambient light level. 
                sscanf(trg_parms,"%d",&cs); 
                Triggers[Num_Triggers].NewLightLvl = cs; 
                break; 
            case 7: // Change Zone light level. 
                sscanf(trg_parms,"%d %d",&cs, &rs); 
                Triggers[Num_Triggers].ZoneNbr = cs; 
                Triggers[Num_Triggers].NewLightLvl = rs; 
                break; 
            case 8: // Load new map. 
                sscanf(trg_parms,"%s",sp_basename); 
                strncpy(Triggers[Num_Triggers].NewFileName,sp_basename,13); 
                *(Triggers[Num_Triggers].NewFileName + 13) = 0x00; 
                break; 
            case 9: // Teleport player. 
                sscanf(trg_parms,"%d %d %d",&cs, &rs, &ce); 
                Triggers[Num_Triggers].MapX = cs; 
                Triggers[Num_Triggers].Mapy = rs; 
                Triggers[Num_Triggers].NewAngle = ce; 
                break; 
            case 10: // Load new texture. --- Not implemented yet. 
 
 
 
                break; 
            case 11: 
            case 12: 
            case 13: 
            case 14: 
            case 15: // Change Map. 
                sscanf(trg_parms,"%d %d %d",&cs, &rs, &ce); 
                Triggers[Num_Triggers].MapX = cs; 
                Triggers[Num_Triggers].Mapy = rs; 
                Triggers[Num_Triggers].TxtNbr = ce; 
                break; 
            } 
        } 
		// Step 3t. Load in the ambient light level.
        else if (!strcmpi(typ,"B"))
        { 
            sscanf(line,"%*s %d",&sp_num); 
            ambient_level = sp_num; 
        } 
        // Finally, This quits... along with EOF. 
        else if (!strcmpi(typ,"#END"))
            break; 
        
		// Step 4. Get the next line.
        fgets(line,256,fp);
    } 

    fclose(fp); // Close the file, 
    return 1;     // and return. 
} 
 
// The following functions render the world... 
 
// 
// This function draws a sliver of wall texture. It reads from a texture (sliver_map) row 
// (sliver_row) and draws from the top down in the column (sliver_column). It is scaled 
// to (sliver_scale) and is centered on the screen vertically by the calling routing, 
// starting at (sliver_start). All bytes that are 0 (zero) are transparent. 
// Scaling is done using a pre-calculated array of fixed point fractions. These are added 
// to Tcount each time through the loop, and the whole part is used as the index into the 
// texture. (i.e. if Tcount = 0.5, then the texture is now 2x as large.) 
// 
void draw_sliver_transparent(void) 
{ 
    int           level; 
    extern int    sliver_column; // Screen column to draw in. 
    extern int    sliver_start;  // Screen row to start drawing in. 
    extern int    sliver_scale;  // how many pixels (rows) long the sliver is to be. 
    extern int    sliver_row;    // Which row from the texture to read. 
    extern int    sliver_map;    // Which texture to use. 
    extern int    sliver_light; 
    extern long   sliver_dist; 
    extern int    sliver_sptlt; 
    extern int    sliver_zoneamb; 
    extern int    sliver_usefog; 
    register int  y;          // Current screen row we're rendering 
    register long Scale,      // Amount (fixed Pt) to increment Tcount each row. 
                  Tcount,     // The offset into the texture / lightmap row (Fixed point) 
                  sliver_end; // The last screen row to render 
    char	      *base,      // Beginning of the current texture row in memory. 
                  *lightbase; // Beginning of the current light tile row in memory. 
    unsigned char *bufptr;    // Pointer to the beginning of screen memory for this sliver. 
    register char pixl,       // The pixel value we're going to plot. 
                  lgt;        // The lighting value for the current pixel. 
 
    // Reset Tcount 
    Tcount = 0; 
    // Preload the scaling factor, and the pointer to the texture. (Faster this way). 
    Scale = sliver_factor[sliver_scale]; 
    base = WallTextures[sliver_map]+(sliver_row<<7); 
    if (sliver_light > -1) 
		if (sliver_light <= Num_Light_Tiles) 
            lightbase = LightMapTiles[sliver_light]+(sliver_row<<7); 
	
    // Calculate the end screen row, and clip it if necessary. 
    sliver_end = sliver_start+sliver_scale; 
    if (sliver_end > 149) 
        sliver_end = 149; 
    // Clip the top of the sliver, and adjust Tcount appropriately. 
    if (sliver_start < 0) 
    { 
        Tcount = (((long)(-sliver_start)) * Scale); 
        sliver_start = 0; 
    } 
    // Set the double buffer pointer  to the starting row and column. 
    bufptr = double_buffer+*(row+sliver_start)+sliver_column; 
    // Are we using constant shading, or are we using the tile ? 
    if (sliver_light > -1) 
    { 
        // Here, were using a lighting tile... 
        // Now, for the entire sliver run, 
        for (y = sliver_start; y <= sliver_end ; ++y) 
        { 
            // Get the pixel value from the texture, using the whole part of Tcount as the index. 
            pixl = *(base+(long)(Tcount>>16)); 
            // If the value isn't zero, then draw it. 
            if ((pixl != 0)) 
            { 
                lgt  = *(lightbase+(Tcount>>16)); 
                level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + lgt + sliver_zoneamb; 
                if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
                level <<= 8; 
                if (sliver_usefog == 1) 
                    *bufptr = *(FogMap + level + pixl); 
                else 
                    *bufptr = *(LightMap + level + pixl); 
            } 
            // And increment Tcount and the double buffer pointer. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
    else // sliver_light = -1, constant shading... 
    { 
        // Here we're using constant lighting for the whole run... 
        level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + sliver_sptlt + sliver_zoneamb; 
        if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
        level <<= 8; 
        // Now, for the entire sliver run, 
        for (y = sliver_start; y <= sliver_end ; ++y) 
        { 
            // Get the pixel value from the texture, using the whole part of Tcount as the index. 
            pixl = *(base+(long)(Tcount>>16)); 
            // If the value isn't zero, then draw it. 
            if ((pixl != 0)) 
                if (sliver_usefog == 1) 
                    *bufptr = *(FogMap + level + pixl); 
                else 
                    *bufptr = *(LightMap + level + pixl); 
            // And increment Tcount and the double buffer pointer. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
} 
 
// 
// This function draws a sliver of wall texture. It reads from a texture (sliver_map) row 
// (sliver_row) and draws from the top down in the column (sliver_column). It is scaled 
// to (sliver_scale) and is centered on the screen vertically by the calling routing, 
// starting at (sliver_start). All bytes that are 0 (zero) are transparent. 
// Scaling is done using a pre-calculated array of fixed point fractions. These are added 
// to Tcount each time through the loop, and the whole part is used as the index into the 
// texture. (i.e. if Tcount = 0.5, then the texture is now 2x as large.) 
// 
void draw_sliver_trans(void) 
{ 
    int           level; 
    extern int    sliver_column; // Screen column to draw in. 
    extern int    sliver_start;  // Screen row to start drawing in. 
    extern int    sliver_scale;  // how many pixels (rows) long the sliver is to be. 
    extern int    sliver_row;    // Which row from the texture to read. 
    extern int    sliver_map;    // Which texture to use. 
    extern int    sliver_light; 
    extern long   sliver_dist; 
    extern int    sliver_sptlt; 
    extern int    sliver_zoneamb; 
    extern int    sliver_usefog; 
    register int  y;          // Current screen row we're rendering 
    register long Scale,      // Amount (fixed Pt) to increment Tcount each row. 
                  Tcount,     // The offset into the texture row (Fixed point) 
                  sliver_end; // The last screen row to render 
    char          *base,      // Beginning of the current texture row in memory. 
                  *lightbase;
    unsigned char *bufptr;    // Pointer to the beginning of screen memory for this sliver. 
    register char pixl,       // The pixel value we're going to plot. 
                  lgt; 
 
  // Reset Tcount 
    Tcount = 0; 
    // Preload the scaleing factor, and the pointer to the texture. (Faster this way). 
    Scale = sliver_factor[sliver_scale]; 
    base = WallTextures[sliver_map]+(sliver_row<<7); 
    if (sliver_light > -1)
        if (sliver_light <= Num_Light_Tiles) 
            lightbase = LightMapTiles[sliver_light]+(sliver_row<<7); 

    // Calculate the end screen row, and clip it if necessary. 
    sliver_end = sliver_start+sliver_scale; 
    if (sliver_end > 149) 
        sliver_end = 149; 
    // Clip the top of the sliver, and adjust Tcount appropriately. 
    if (sliver_start < 0) 
    { 
        Tcount = (((long)(-sliver_start)) * Scale); 
        sliver_start = 0; 
    } 
    // Set the double buffer pointer  to the starting row and column. 
    bufptr = double_buffer+*(row+sliver_start)+sliver_column; 
 
    if (sliver_light > -1) 
    { 
        // Now, for the entire sliver run, 
        for (y = sliver_start; y <= sliver_end ; ++y) 
        { 
            // Get the pixel value from the texture, using the whole part of Tcount as the index. 
            pixl = *(base+(long)(Tcount>>16)); 
            // If the value isn't zero, then draw it. 
            if ((pixl != 0)) 
            { 
                lgt  = *(lightbase+(Tcount>>16)); 
                level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + lgt + sliver_zoneamb; 
                if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
                level <<= 8; 
                if (sliver_usefog == 1) 
                    *bufptr =  *(Translucency + ((((unsigned int)*(FogMap + level + pixl)) & 0x00ff) << 8) + (((unsigned int)*bufptr) & 0x00ff)); 
                else 
                    *bufptr =  *(Translucency + ((((unsigned int)*(LightMap + level + pixl)) & 0x00ff) << 8) + (((unsigned int)*bufptr) & 0x00ff)); 
            } 
            // And increment Tcount and the double buffer pointer. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
    else 
    { 
        level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + sliver_sptlt + sliver_zoneamb; 
        if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
        level <<= 8; 
        // Now, for the entire sliver run, 
        for (y = sliver_start; y <= sliver_end ; ++y) 
        { 
            // Get the pixel value from the texture, using the whole part of Tcount as the index. 
            pixl = *(base+(long)(Tcount>>16)); 
            // If the value isn't zero, then draw it. 
            if ((pixl != 0)) 
            { 
                if (sliver_usefog == 1) 
                    *bufptr =  *(Translucency + ((((unsigned int)*(FogMap + level + pixl)) & 0x00ff) << 8) + (((unsigned int)*bufptr) & 0x00ff)); 
                else 
                    *bufptr =  *(Translucency + ((((unsigned int)*(LightMap + level + pixl)) & 0x00ff) << 8) + (((unsigned int)*bufptr) & 0x00ff)); 
            } 
            // And increment Tcount and the double buffer pointer. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
} 
 
// 
// This function draws a sliver of wall texture. It reads from a texture (sliver_map) row 
// (sliver_row) and draws from the top down in the column (sliver_column). It is scaled 
// to (sliver_scale) and is centered on the screen vertically by the calling routing, 
// starting at (sliver_start). 
// Scaling is done using a pre-calculated array of fixed point fractions. These are added 
// to Tcount each time through the loop, and the whole part is used as the index into the 
// texture. (i.e. if Tcount = 0.5, then the texture is now 2x as large.) 
// 
void draw_sliver(void) 
{ 
    int level; 
    extern int    sliver_column; // Screen column to draw in. 
    extern int    sliver_start;  // Screen row to start drawing in. 
    extern int    sliver_scale;  // how many pixels (rows) long the sliver is to be. 
    extern int    sliver_row;    // Which row from the texture to read. 
    extern int    sliver_map;    // Which texture to use. 
    extern int    sliver_light; 
    extern long   sliver_dist; 
    extern int    sliver_sptlt; 
    extern int    sliver_zoneamb; 
    extern int    sliver_usefog; 
    register int  y;          // Current screen row we're rendering 
    register long Scale,      // Amount (fixed Pt) to increment Tcount each row. 
                  Tcount,     // The offset into the texture row (Fixed point) 
                  sliver_end; // The last screen row to render 
    char          *base,      // Beginning of the current texture row in memory. 
                  *lightbase;
    unsigned char *bufptr;    // Pointer to the beginning of screen memory for this sliver. 
    register char lgt; 
 
  // Reset Tcount for this run. 
    Tcount = 0; 
    // Preload the scaleing factor, and the pointer to the texture. (Faster this way). 
    Scale = sliver_factor[sliver_scale]; 
    base = WallTextures[sliver_map]+(sliver_row<<7); 
    if (sliver_light > -1)
        if (sliver_light <= Num_Light_Tiles) 
            lightbase = LightMapTiles[sliver_light]+(sliver_row<<7); 

    // Calculate the ending screen row, and clip it if necessary. 
    sliver_end = sliver_start+sliver_scale; 
    if (sliver_end > 149) 
        sliver_end = 149; 
    // Clip the beginning of the slice, if necessary. Adjust Tcount to it's new offset. 
    if (sliver_start < 0) 
    { 
        Tcount = (((long)(-sliver_start)) * Scale); 
        sliver_start = 0; 
    } 
    // Calculate the beginning point in the double buffer for the slice. 
    bufptr = double_buffer+*(row+sliver_start)+sliver_column; 
    if (sliver_light > -1) 
    { 
        // Now, for each screen row, 
        for (y = sliver_start; y <= sliver_end; y ++) 
        { 
            lgt  = *(lightbase+(Tcount>>16)); 
            level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + lgt + sliver_zoneamb; 
            if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
            level <<= 8; 
            // Get the byte from the texture and render it. 
            if (sliver_usefog == 1) 
                *bufptr = *(FogMap + level + *(base + (Tcount>>16))); 
            else 
                *bufptr = *(LightMap + level + *(base + (Tcount>>16))); 
            // Update Tcount and bufptr. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
    else 
    { 
        level = ((unsigned char)*(LightLevel+sliver_dist)) + ambient_level + sliver_sptlt + sliver_zoneamb; 
        if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
        level <<= 8; 
        // Now, for each screen row, 
        for (y = sliver_start; y <= sliver_end; y ++) 
        { 
            // Get the byte from the texture and render it. 
            if (sliver_usefog == 1) 
                *bufptr = *(FogMap + level + *(base + (Tcount>>16))); 
            else 
                *bufptr = *(LightMap + level + *(base + (Tcount>>16))); 
            // Update Tcount and bufptr. 
            Tcount += Scale; 
            bufptr += 320; 
        } 
    } 
} 

// 
//    This is the heart of the program. It casts a mathematical ray from the 
// players position along the view_angle. The ray stops when it hits a wall, 
// saving the x and y coordinates in yi_save and x_save, and returns the type 
// of wall hit. 
//    This routine finds the HORIZONTAL cells along the ray.
// 
int Cast_X_Ray(long x, long y, long view_angle, long &yi_save, long &x_save, int &lighttile, int &zone) 
{ 
    register long x_bound,      // X intercept at the cell boundaries, in fine coords. 
                  x_delta,      // Amount to add to x_bound for each iteration. 
                  next_x_cell,  // Correction for "1-off" error for getting map cell. 
                  hit_type,     // Cell value at current intersection. 
                  ystep;        // Amount to add to yi for each iteration. 
    long          yi;           // The Y intercept (at x_bound) in fine coords. 
 
    // If the ray is going to the RIGHT, 
    if ((view_angle < ANGLE_90) || (view_angle > ANGLE_270)) 
    { 
        // Determine where the current cell edge is (in fine coords.) 
        x_bound = (x & 0xff80)+CELL_X_SIZE; 
        // This is the amount to add to x_bound for each step. 
        x_delta = CELL_X_SIZE; 
        // Amount to compensate for the "1-off" error. 
        next_x_cell = 0; 
    } 
    // Else, if it's going to the LEFT. 
    else 
    { 
        // Determine where the current cell edge is (in fine coords.) 
        x_bound = (x & 0xff80); 
        // This is the amount to add to x_bound for each step. 
        x_delta = -CELL_X_SIZE; 
        // Amount to compensate for the "1-off" error. 
        next_x_cell = -1; 
    } 
    // The initial Y intercept, at x_bound. 
    yi = ((long)x_bound-(long)x)*(*(tan_table+view_angle))+((long)y<<16); 
    // The amount to add to yi for each step. (the same each time, so we bring it out here). 
    ystep = *(y_step+view_angle); 
    // Now we cast the ray... 
    while(1) 
    { 
        // Calculate the map coordinates for the current intersection. 
        cell_xx = ((x_bound+next_x_cell)>>7); 
        cell_yx = ((long)(yi>>16)) >> 7; 
        // Check boundaries. 
        if (cell_xx > 63) cell_xx = 63; 
        if (cell_yx > 63) cell_yx = 63; 
        if (cell_xx < 0) cell_xx = 0; 
        if (cell_yx < 0) cell_yx = 0; 
 
         // Check to see if we hit a wall or not. (A wall is >= 1). 
        if ((hit_type = *(world+(cell_yx<<6)+cell_xx))!=0) 
        { 
            // Save the fine coordinates sso we can return them... 
            yi_save = (long)((yi+32768L)>>16); 
            x_save = x_bound; 
            lighttile = *(FloorLightMap+(cell_yx<<6)+cell_xx); 
            zone = *(ZoneMap+(cell_yx<<6)+cell_xx); 
            if(TextureFlags[hit_type].IsRecessed) 
            { 
                x_save += ( x_delta / 2 ) ; 
                yi_save = (long) (((yi + ystep / 2 ) + 32768L ) >> 16 ) ; 
            } 
            // and return, with the texture number for the map cell. 
            return hit_type; 
        } 
        // Otherwise, if we havn't hit a wall, increment to the next boundary intersection, 
        // and go again. 
        yi += ystep; 
        x_bound += x_delta; 
    } 
} 

// 
//    This is the heart of the program. It casts a mathematical ray from the 
// players position along the view_angle. The ray stops when it hits a wall, 
// saving the x and y coordinates in xi_save and y_save, and returns the type 
// of wall hit. 
//    This routine finds the VERTICAL cells along the ray.
//  
int Cast_Y_Ray(long x, long y, long view_angle, long &xi_save, long &y_save, int &lighttile, int &zone) 
{ 
    register long y_bound,         // Y intercept at the cell boundaries, in fine coords. 
                  y_delta,         // Amount to add to y_bound each iteration of the loop. 
                  next_y_cell,     // Fixes the "1 off" error while calculating cell locations. 
                  hit_type,        // Texture value in the map cell. 
                  xstep;           // Amount to add to xi each iteration of the loop. 
    long          xi;              // X intercept ay y_bound, in fine coords. 
 
 
    // If the ray is looking UP, 
    if ((view_angle > ANGLE_0) && (view_angle < ANGLE_180)) 
    { 
        // Calculate the top cell boundary (Y intercept) 
        y_bound = CELL_Y_SIZE + y & 0xff80; 
        // Set y_delta. This is added to y_bound each time through the loop. 
        y_delta = CELL_Y_SIZE; 
        // and this fixes the "1 off" error in getting the map cell location (array x&y). 
        next_y_cell = 0; 
    } 
    // Else, if we are looking DOWN... 
    else 
    { 
        // Calculate the lower cell boundary. 
        y_bound = y & 0xff80; 
        // Set y_delta. 
        y_delta = -CELL_Y_SIZE; 
        // Again, this is to fix the "1 off" error. 
        next_y_cell = -1; 
    } 
    // Now we calculate the x intercept on the Y boundary (See TGPG) 
    xi =((long)y_bound-(long)y)*(*(inv_tan_table+view_angle))+((long)x<<16); 
    // Get the value to increment xi from the x_step array. 
    xstep = *(x_step+view_angle); 
    // Now we enter into the main casting loop... 
    while(1) 
    { 
        // Get the map array indices (X and Y) 
        cell_yy = (y_bound+next_y_cell) >> 7; 
        cell_xy = ((int)(xi>>16)) >> 7; 
        // Check boundaries. 
        if (cell_yy > 63) cell_yy = 63; 
        if (cell_xy > 63) cell_xy = 63; 
        if (cell_yy < 0) cell_yy = 0; 
        if (cell_xy < 0) cell_xy = 0; 
        // And determine if there is a texture in that cell. 
        if ((hit_type = *(world+(cell_yy<<6)+cell_xy))!=0) 
        { 
            // If there is, let's save the location (fine coords), 
            xi_save = (long)((xi+32768L)>>16); 
            y_save = y_bound; 
            lighttile = *(FloorLightMap+(cell_yy<<6)+cell_xy); 
            zone = *(ZoneMap+(cell_yy<<6)+cell_xy); 
            if(TextureFlags[hit_type].IsRecessed) 
            { 
                y_save += ( y_delta / 2 ) ; 
                xi_save = (long) (((xi + xstep / 2 ) + 32768L ) >> 16 ) ; 
            } 
            // and indicate what type of wall we hit. 
            return hit_type; 
        } 
        // If we didn't hit a wall, we increment the intercept variables, and check again. 
        xi += xstep; 
        y_bound += y_delta; 
    } 
} 
 
// 
//    This function handles rendering the floor and ceiling. It is done in a somewhat 
// different manner than the walls. This routine will use similar triangles to find out 
// how far away a screen row (on which will be drawn the ceiling and floor) is from the 
// viewer's eye. The distance is then divided by the cosine of the view angle to find 
// out how far away a specific pixel in that row (corresponding to the angle) is from 
// the eye. The X and Y are then determined (using sine and cosine tables), and the 
// proper pixel is grabbed from the proper texture. The ceiling and floor mirror each 
// other most of the time, so we don't need to re-calculate. We do, a little bit though, 
// just to make sure we cover everything. 
// 
void Texture_FloorCeil(long x, long y, long view_angle) 
{ 
    register int   col,          // The screen column currently being processed. 
                   ROW,          // The screen row currently being processed. 
                   size;         // The row on the screen where the floor ends and the wall begins. 
    unsigned int            flor_level, 
                   ceil_level, 
                   ZoneAmb; 
    register long  LightDist,    // How  away the point is from the eye. 
                   distance,     // How  away the point is from the eye (adjusted). 
                   MapOff,       // Offset into the floor map of the current pixel. (see below) 
                   TxtOff,       // Offset into the texture map of the current pixel. (see below) 
                   inv_cost;     // Holds the inverse cosine. (22:10) It doesn't change by row, so we only get it once. 
    long           xv,           // X coordinate of the screen pixel (ROW,COL) in texture space. 
                   yv,           // Y coordinate of the screen pixel (ROW,COL) in texture space. 
                   angle,        // the angle (absolute) the ray for the column is pointing. 
                   sintab,       // The sine value for the current angle. (22:10 fixed) 
                   costab;       // The cosine value for the current angle. (22:10 fixed) 
    struct col_obj *line;        // This saves us a lot in array dereferencing! only have to do it once. 
    
    // For each column in the viewport (ANGLE_0 to ANGLE_60) 
    for (col = 0; col < 240; ++col) 
    { 
        // Calculate the absolute angle of the ray. Since col=[0,240), col-120 = [-ANGLE_30, ANGLE_30) 
        angle = view_angle+(col-120); 
        // Check the bounds of the angle. 
        if (angle < ANGLE_0) 
            angle += ANGLE_360; 
        if (angle >= ANGLE_360) 
            angle -= ANGLE_360; 
        // Get the line structure for the current column. 
        // We do this here to save time. Array dereferencing takes lots of time. 
        line = &(scan_lines[col].line[scan_lines[col].num_objs-1]); 
        // Now get the trig values we need for this angle. This doesn't change with the 
        // row, so we get it here to save time. 
        inv_cost =*(Floor_inv_cos_table+col); 
        sintab =*(Floor_sin_table+angle); 
        costab =*(Floor_cos_table+angle); 
        // Figure out where we can stop drawing the floor for this column. No sense in 
        // drawing behind a solid wall. 
        size = (line->top+line->scale); 
        // Now, we cast from the bottom, up to the beginning of the the wall. 
        for (ROW = 149; ROW >= size; --ROW) 
        { 
            // Get the distance of the floor pixel for this row, dead ahead, and then rotate it out 
            // to the proper angle. (the inv_cost). 
            LightDist = ((*(Floor_Row_Table+ROW))>>10); 
            distance =((*(Floor_Row_Table+ROW)*inv_cost)>>10); 
            // Figure out the x and y from trig, (hyp*sin=y, hyp*cos=x). 
            yv = (long)((distance*sintab)>>20)+y; 
            xv = (long)((distance*costab)>>20)+x; 
            // Check boundaries. 
            if (xv < 0) xv = 0; 
            if (xv > 8191) xv = 8191; 
            if (yv < 0) yv = 0; 
            if (yv > 8191) yv = 8191; 
            if (LightDist > 8191) LightDist = 8191; 
            if (LightDist < 1) LightDist = 1; 
            // We pre-calculate these to save time. Calculate once, use twice... 
            MapOff = (((int)yv>>7)<<6)+((int)xv>>7); 
            TxtOff = (((yv&127)<<7)+(xv&127)); 
            // Calculate the zone, and zone options. 
            // light level for light sourcing 
            ZoneAmb = ZoneAttr[*(ZoneMap+MapOff)].ambient; 
            flor_level = (unsigned char)LightLevel[LightDist] + ambient_level + *(LightMapTiles[*(FloorLightMap+MapOff)]+TxtOff) + ZoneAmb; 
            if (ZoneAttr[*(ZoneMap+MapOff)].inside == 1) 
                ceil_level = (unsigned char)ambient_level + *(LightMapTiles[*(CeilLightMap+MapOff)]+TxtOff) + ZoneAmb; 
            else 
                ceil_level = (unsigned char)LightLevel[LightDist] + ambient_level + *(LightMapTiles[*(CeilLightMap+MapOff)]+TxtOff) + ZoneAmb; 
            if(flor_level > MAX_LIGHT_LEVELS - 1) flor_level = MAX_LIGHT_LEVELS - 1; 
            if(ceil_level > MAX_LIGHT_LEVELS - 1) ceil_level = MAX_LIGHT_LEVELS - 1; 
            flor_level <<= 8; 
            ceil_level <<= 8; 
            // Now we draw the ceiling and floor at the same time. 
            if (ZoneAttr[*(ZoneMap+MapOff)].fog == 1) 
            { 
                *(double_buffer+*(row+ROW)+col) = *(FogMap + flor_level + *(WallTextures[*(FloorMap+MapOff)-1]+TxtOff)); 
                *(double_buffer+*(row+149-ROW)+col) = *(FogMap + ceil_level + *(WallTextures[*(CeilMap+MapOff)-1]+TxtOff)); 
            } 
            else 
            { 
                *(double_buffer+*(row+ROW)+col) = *(LightMap + flor_level + *(WallTextures[*(FloorMap+MapOff)-1]+TxtOff)); 
                *(double_buffer+*(row+149-ROW)+col) = *(LightMap + ceil_level + *(WallTextures[*(CeilMap+MapOff)-1]+TxtOff)); 
            } 
        } 
        // Now we find out where the ceiling really ends. It isn't always even with the floor. 
        size = line->top; 
        // And we do the above rendering again. This time, just on the ceiling. 
        for (ROW = 148-ROW; (ROW <= size) && (ROW > 0); ++ROW) 
        { 
            LightDist = ((*(Floor_Row_Table+ROW))>>10); 
            distance =((*(Floor_Row_Table+ROW)*inv_cost)>>10); 
            yv = (long)((distance*sintab)>>20)+y; 
            xv = (long)((distance*costab)>>20)+x; 
            // Check boundaries. 
            if (xv < 0) xv = 0; 
            if (xv > 8191) xv = 8191; 
            if (yv < 0) yv = 0; 
            if (yv > 8191) yv = 8191; 
            MapOff = (((int)yv>>7)<<6)+((int)xv>>7); 
            TxtOff = (((yv&127)<<7)+(xv&127)); 
            if (LightDist > 8191) LightDist = 8191; 
            if (LightDist < 1) LightDist = 1; 
            // light level for light sourcing 
            if (ZoneAttr[*(ZoneMap+MapOff)].inside == 1) 
                ceil_level = (unsigned char)ambient_level + *(LightMapTiles[*(CeilLightMap+MapOff)]+TxtOff) + ZoneAmb; 
            else 
                ceil_level = (unsigned char)LightLevel[LightDist] + ambient_level + *(LightMapTiles[*(CeilLightMap+MapOff)]+TxtOff) + ZoneAmb; 
            if(ceil_level > MAX_LIGHT_LEVELS - 1) ceil_level = MAX_LIGHT_LEVELS - 1; 
            ceil_level <<= 8; 
            if (ZoneAttr[*(ZoneMap+MapOff)].fog == 1) 
                *(double_buffer+*(row+ROW)+col) = *(FogMap + ceil_level + *(WallTextures[*(CeilMap+MapOff)-1]+TxtOff)); 
            else 
                *(double_buffer+*(row+ROW)+col) = *(LightMap + ceil_level + *(WallTextures[*(CeilMap+MapOff)-1]+TxtOff)); 
        } 
    } 
} 
 
// 
//  This routine handles the rendering calculations of the sprites. It is work 
// in progress. 
// 
void process_sprites(long x, long y, long view_angle) 
{ 
    int  Current_Sprite, // The current object we're rendering. 
         Sprite_Num;     // The sprite used for the current object. 
    long ang,            // The angle the sprite makes with the view_angle. 
         ang_clipped,    // The relative angle, clipped to +- ANGLE_30. 
         ang_saved,      // Saved copy of the angle. Used in scaling and fisheye fix. 
         dist,           // The distance from the viewer. 
         size,           // The size of the sprite in pixels (horiz and vert). 
         left,           // The starting column of the sprite. 
         right,          // The ending column of the sprite. 
         sx,sy,          // The camera coordinates of the sprite. 
         tx,ty,          // Temporary holding for the coords. of the sprite during the transform. 
         VA_Neg,         // The negative of the player's view angle. 
         Scale_Factor;   // Fixed point value used in scaling the slices. 
    long Frame,          // The frame of the sprite we are going to display. 
         sliver,         // The column of the texture to draw for the scan line. 
         col,            // The scan line (col) we are in. 
         Col_Depth,      // The number of objects in the scan line buffer for this column. 
         Indx;           // Loop index used to insert sprite sliver in scan line buffer. 
    int  level, 
         zoneamb, 
         zonefog; 
    register long  MapOff,      // Offset into the floor map of the current pixel. (see below) 
        TxtOff;      // Offset into the texture map of the current pixel. (see below) 
 
    // We start adding sprites to the global WallTextures at 51. 
    Sprite_Num = 50; 
 
    // Loop through all the world's objects. 
    for (Current_Sprite = 1; Current_Sprite <= Num_Objects; ++Current_Sprite) 
    { 
        // Step 1. Translate the position of the sprite by -[x,y] (Integer math) 
        sx = Objects[Current_Sprite].x - x; 
        sy = Objects[Current_Sprite].y - y; 
        // Step 2. Rotate the sprite by -View_Angle. (Integer X Fixed = 22:10 fixed point) 
        VA_Neg = ANGLE_360 - view_angle; 
        tx = sx*Floor_cos_table[VA_Neg] - sy*Floor_sin_table[VA_Neg]; 
        ty = sx*Floor_sin_table[VA_Neg] + sy*Floor_cos_table[VA_Neg]; 
        sx = (long)(tx >> 10); 
        sy =  (long)(ty >> 10); 
        // Step 3. Now cull the sprite if behind viewer. 
        if (sx < 1) 
            continue; 
        // Step 4. Get the angle of the sprite. (Fixed / Fixed = Integer) 
        ang_clipped = ang_saved = ang = (sy * 226l)/sx; 
        if (ang < ANGLE_0) ang += ANGLE_360; 
        // Step 5 Cull the sprite if it's too  off the side of the screen 
        if ((ang_saved < -ANGLE_45) || (ang_saved > ANGLE_45)) 
            continue; 
        // Step 6 Get the distance, and size of the sprite (22:10 Fixed) 
        dist = sx * inv_cos_table[ang] >> 13; //  Integer Result (Table = 19:13) 
        // Step 6a. If it's too close, we can't draw it. 
        if (dist < 5) continue; 
 
         // Step 6b. Clip the angle to +/- ANGLE_30. The cos_table only goes that . 
        if (ang_clipped > ANGLE_30) ang_clipped = ANGLE_30; 
        if (ang_clipped < -ANGLE_30) ang_clipped = -ANGLE_30; 
 
         // Step 6c. Calculste the size of the bitmap on the screen. 
        size = (int)((cos_table[ang_clipped+ ANGLE_30]/dist) >> 8) << 1 ; // Table is 25:7 
        if (size > 600) size = 599; 
        // Step 7. Now we calculate the starting and ending columns of the sprite, and get the scaling factor. 
        left  = (120+ang_saved)-(size >> 1); 
        right = (120+ang_saved)+(size >> 1)-1; 
        Scale_Factor = sliver_factor[size]; 
        // Step 8 Determine the angle we are viewing the sprite from (Frame number). 
        Frame = (((view_angle+ang_clipped) / ANGLE_45)+ 4) % 8 - (Objects[Current_Sprite].angle / ANGLE_45); 
        if (Frame < 0) Frame += 8; 
        if (Frame > 7) Frame -= 8; 
        // Step 9. Now we sort the slices into the column buffer. 
        Sprite_Num++; // Get the next available WallTextures slot. 
        WallTextures[Sprite_Num] = Sprites[Objects[Current_Sprite].sprite_num].Frames[Frame]; 
        TextureFlags[Sprite_Num+1].IsTranslucent = Sprites[Objects[Current_Sprite].sprite_num].translucent; 
        TextureFlags[Sprite_Num+1].IsTransparent = 1; 
        TextureFlags[Sprite_Num+1].IsDoor = 0; 
 
         // light level for light sourcing 
        MapOff = (((int)sy>>7)<<6)+((int)sx>>7); 
        TxtOff = (((sy&127)<<7)+(sx&127)); 
 
        level = LightLevel[dist] + ambient_level + *(LightMapTiles[*(FloorLightMap+MapOff)]+TxtOff); 
        if(level > MAX_LIGHT_LEVELS - 1) level = MAX_LIGHT_LEVELS - 1; 
        zoneamb = ZoneAttr[*(ZoneMap+MapOff)].ambient; 
        zonefog = ZoneAttr[*(ZoneMap+MapOff)].fog; 
 
         // Loop through all the slivers(columns) of the sprite's bitmap. 
        for (sliver = 0, col=left; col <= right; ++col, sliver += Scale_Factor)  // Start at the left most slice of the sprite. 
        { 
            if ((col >= 0) && (col < 240))   //  If the slice is within the view window, 
                // start at the beginning of the list. If the end is reached, break out. do not add the slice. 
                for (Col_Depth = 0; Col_Depth <= scan_lines[col].num_objs; ++Col_Depth) 
                    // else if the depth of the current slice is greater than 'dist', 
                { 
                    if (scan_lines[col].line[Col_Depth].dist > dist) 
                    { 
                        //  shift all info down, and insert slice. 
                        for (Indx = scan_lines[col].num_objs; Indx > Col_Depth; --Indx) 
                        { 
                            scan_lines[col].line[Indx] = scan_lines[col].line[Indx-1]; 
                        } 
                        scan_lines[col].line[Indx].top = 75 - (size >> 1); 
                        scan_lines[col].line[Indx].col = col; 
                        scan_lines[col].line[Indx].scale = size; 
                        scan_lines[col].line[Indx].dist = dist; 
                        scan_lines[col].line[Indx].row = (sliver >> 16); 
                        scan_lines[col].line[Indx].texnum = Sprite_Num; 
                        scan_lines[col].line[Indx].lightnum = -1; 
                        scan_lines[col].line[Indx].sptlight = level; 
                        scan_lines[col].line[Indx].zoneambient = zoneamb; 
                        scan_lines[col].line[Indx].usefog = zonefog; 
                        scan_lines[col].num_objs++; 
                        break; 
                    } 
                } 
        } 
    } 
} 
 
//
//    This function renders the automap screen to the double buffer. It is 
// positioned in the middle of the screen, and is 70x70 pixels in size. 
//
void DrawAutomap(long Xpos, long Ypos) 
{ 
    long     x,y; 
 
  // Frame in the automap. 
    for (x=0; x<70; ++x) 
        for (y=0; y<70; ++y) 
            *(double_buffer+*(row+(20+y))+20+x) = *(AutoMapBkgd+y*70+x); 
 
        // Draw the walls. 
    for (x=0; x<64; ++x) 
        for (y=0; y<64; ++y) 
            if (*(world+((y<<6)+x)) != 0) 
                if (TextureFlags[*(world+(y<<6)+(x))].IsDoor) 
                { 
                    *(double_buffer+*(row+(23+y))+23+x) = 112; 
                } 
                else if (TextureFlags[*(world+(y<<6)+(x))].IsTransparent) 
                { 
                    *(double_buffer+*(row+(23+y))+23+x) = 121; 
                } 
                else 
                { 
                    *(double_buffer+*(row+(23+y))+23+x) = 2; 
                } 
 
    // Locate the player. 
    *(double_buffer+*(row+(23+(Ypos>>7)))+23+(Xpos>>7)) = 198; 
} 
 
// 
//   Ok, here's where the magic comes together to render the full viewport... 
//   This function loops through all colums in the viewport. Each column (0-240) represents 
// an angle between -ANGLE_30 and ANGLE_30 (60 degree FOV). At each column, we cast 2 rays. 
// one tracks textures on horizontal boundaries, the other on vertical. Once both rays are cast, 
// we determine which is closer to the viewpoint, and store it in the column buffer. If the wall 
// was tagged as transparent, we 'erase' the texture from the map, keeping track of it, and go 
// again. (We keep casting until a SOLID wall is hit). When we hit a solid wall, the casting 
// stops, we replace the 'erase' textures, and go on to the next column. Once all the columns 
// have been cast, we process the sprites, and sort the slivers of thier bitmaps into the column 
// buffer. 
//   When all the setup is done, we begin drawing the viewport. We call a function to cast and 
// draw the floors and ceilings, and then we loop through the column buffer, sending each level's 
// texture and scaling info to the draw_sliver and draw_sliver_trans in reverse order (back to front). 
// 
void Render_View(long x,long y,long view_angle) 
{ 
    extern int sliver_start;    // These are the Sliver renderer globals. They are global for 
    extern int sliver_column;   // SPEED considerations. They have been described quite enough so . 
    extern int sliver_scale; 
    extern int sliver_row; 
    extern int sliver_map; 
    extern int sliver_light; 
 
    long ray,              // the current ray being cast 0-240 (Corresponds to both the screen col, and the angle.) 
         x_hit_type,       // records the texture type that was found by the X caster. 
         y_hit_type,       // records the texture type that was found by the Y caster. 
         dist_x,           // The distance from the player to the texture found by the X caster. 
         dist_y,           // The distance from the player to the texture found by the Y caster. 
         xi,               // The X intercept, as calculated by the Y caster. 
         yi,               // The Y intercept, as calculated by the X caster. 
         xs,               // The X Boundary value, as found by the X caster. 
         ys,               // The Y Boundary value, as found by the Y caster. 
         view_angle_saved; // Saves the passed-in view angle, since we modify the original. 
    int  x_light_type, 
         y_light_type, 
         x_zone, 
         y_zone; 
 
    struct erased_t { int x,y,type; } erased[30];    // Used to track 'erased' semi-transparent textures. 
    struct col_obj *line;                            // Used to save time in array indexing. 
    struct scan_line_t *s_line;                      // Used to save time in array indexing. 
    int numerased;                                   // The number of 'erased' textures. 
    int indx;                                        // Loop coounter used when replacing erased blocks. 
 
  // Keep a copy of view_angle, we're going to be changing it. 
    view_angle_saved = view_angle; 
    // Subtract ANGLE_30 from the view_angle (left most angle). Wrap it around if necessary. 
    if ((view_angle-=ANGLE_30) < 0) 
        view_angle=ANGLE_360 + view_angle; 
    // Now we can start casting the rays from view_angle-ANGLE_30 to view_angle+ANGLE_30 
    // For the current viewport size, that's 240 rays. 
    for (ray=0; ray<240; ray++) 
    { 
        // We start out with no 'transparent' textures 'erased'. 
        numerased = 0; 
        // And no items in the column buffer. 
        scan_lines[ray].num_objs = 0; 
        // Keep going until we hit a solid wall, or fill the column buffer. 
        while (1) 
        { 
            // If we're looking straight up and down, make dist_x so large, it won't be considered. 
            if ((view_angle == ANGLE_90)||(view_angle == ANGLE_270)) 
            { 
                dist_x = 1e8; 
                x_hit_type = -1; 
            } 
            // Otherwise, let's cast some X rays... 
            else 
            { 
                // First, we cast the ray. 
                x_hit_type = Cast_X_Ray(x,y,view_angle,yi,xs,x_light_type,x_zone); 
                // Use trig to get the distance (hyp = delta X / Cos(angle). 
                dist_x  = (long)(((xs-x) * inv_cos_table[view_angle])>>13); 
                // Here we chack for fixed point math overflow or underflow. 
                if (dist_x == 0) 
                    dist_x = 1; 
                if (dist_x < 0) 
                    dist_x = 1e8; 
            } 
            // Ok, now that we have the X distance, let's get the Y distance... 
            // If we are looking due left or right, then make it so that we ignore the Y distance. 
            if ((view_angle == ANGLE_0)||(view_angle == ANGLE_180)) 
            { 
                dist_y=1e8; 
                y_hit_type = -1; 
            } 
            // Else, let's cast some Y rays. 
            else 
            { 
                // First, we cast the ray. 
                y_hit_type = Cast_Y_Ray(x,y,view_angle,xi,ys,y_light_type,y_zone); 
                // Then we calculate the distance to the boundary. (Trig again) 
                dist_y  = (long)(((ys-y)*inv_sin_table[view_angle])>>13); 
                // And then check for overflow or underflow. 
                if (dist_y == 0) 
                    dist_y = 1; 
                if (dist_y < 0) 
                    dist_y = 1e8; 
            } 
 
            // Now we determine which ray hit something closer, and save it the column buffer... 
            if (dist_x < dist_y) 
            { 
                // The vertical wall (Horiz. distance and ray) was closer. Store it's info. 
 
                // We stick it at the end of the column buffer. 
                indx =scan_lines[ray].num_objs; 
                // Get the address of the structure position. It's quicker to use a pointer than a big array. 
                line = &(scan_lines[ray].line[indx]); 
                // Calculate the scaling and correct for fish-eye. 
                line->scale =(int)((cos_table[ray]/dist_x)>>8)<<1; 
                // Crop the scaling to a max and min (for overflow/underflow) 
                if (line->scale > 599) line->scale = 599; 
                if (line->scale <= 0) line->scale = 1; 
                // Store the texture row to use for the sliver 
                line->row = ((int)yi)&127; 
                // and it's position in the textures array. 
                line->texnum = x_hit_type-1; 
                line->lightnum = x_light_type; 
                // Center the sliver about the middle of the viewport 
                line->top = 75 - (line->scale>>1); 
                // Store the column number 
                line->col = ray; 
                // and it's distance 
                line->dist = (int)dist_x; 
                // and the zone ambient light 
                line->zoneambient = ZoneAttr[x_zone].ambient; 
                // And the FOG flag. 
                line->usefog = ZoneAttr[x_zone].fog; 
                // and then increment the count. 
                scan_lines[ray].num_objs++; 
                // If the final texture was a transparent one, we have a little more to do... 
                if (TextureFlags[x_hit_type].IsTransparent == 1) 
                { 
                    // We store the texture's map location and value in the erased array, 
                    erased[numerased].x = cell_xx; 
                    erased[numerased].y = cell_yx; 
                    erased[numerased].type = x_hit_type; 
                    // And clear it's entry in the map. This is so that the next ray will pass 
                    // through this location, and continue on. 
                    *(world+(cell_yx<<6)+cell_xx) = 0; 
                    ++numerased; 
                } 
            } 
            else 
            { 
                // The Horizontal wall (Vertical Ray) was closer, store it's info in the buffer. 
                // This process is the same as the one above. 
                indx =scan_lines[ray].num_objs; 
                line = &(scan_lines[ray].line[indx]); 
                line->scale =(int)((cos_table[ray]/dist_y)>>8)<<1; 
                if (line->scale > 599) line->scale = 599; 
                if (line->scale <= 0) line->scale = 1; 
                line->row = ((int)xi)&127; 
                // Note: If the other hit was a door, we use the ALT texture here... 
                line->texnum = y_hit_type-1; 
                line->lightnum = y_light_type; 
                line->top = 75 - (line->scale>>1); 
                line->col = ray; 
                line->dist = (int)dist_y; 
                // and the zone ambient light 
                line->zoneambient = ZoneAttr[y_zone].ambient; 
                // And the FOG flag. 
                line->usefog = ZoneAttr[y_zone].fog; 
                scan_lines[ray].num_objs++; 
                if (TextureFlags[y_hit_type].IsTransparent == 1) 
                { 
                    erased[numerased].x = cell_xy; 
                    erased[numerased].y = cell_yy; 
                    erased[numerased].type = y_hit_type; 
                    *(world+(cell_yy<<6)+cell_xy) = 0; 
                    ++numerased; 
                } 
            } 
            // Now, if we've hit a solid wall, we can quit with this cast, and go on with the 
            // process. Else, we loop back and cast again for the wall behind the transparent one. 
            if (TextureFlags[scan_lines[ray].line[indx].texnum+1].IsTransparent == 0) // Non-transparent Texture. 
                break; 
        } 
 
        // Here's where we put the erased blocks back into the map array. 
        for (indx = 0; indx < numerased; ++indx) 
            *(world+(erased[indx].y<<6)+erased[indx].x) = erased[indx].type; 
 
         // Now we increment the view angle, wraping back around at ANGLE_360. 
        if (++view_angle>=ANGLE_360) 
            view_angle=0; 
        // And we loop back again to do the next screen column. 
    } 
 
    //   Process the sprites and sort them into the column array. 
    process_sprites(x,y,view_angle_saved); 
 
    // Here's where we render the display... 
    // Cast and draw the floor and ceiling. 
#ifdef CAST_FLOORCIEL 
    Texture_FloorCeil(x,y,view_angle_saved); 
#endif 
    // Now, loop through all the screen columns stored in the col buffer. 
 
    for (ray=0; ray<240; ray++) 
    { 
        // We take the address of the next entry here to save time. 
        s_line = &scan_lines[ray]; 
        // Now we loop backwards (depth wise) theough the list for that column. 
        //      printf("Drawing ray %d...",(int)ray); 
        for (indx = s_line->num_objs-1; indx >= 0; --indx) 
        { 
            //             printf("%d..",(int) indx); 
            // Now put the numbers into the globals. 
            line = &(s_line->line[indx]); 
            sliver_start = line->top; 
            sliver_column = line->col; 
            sliver_scale = line->scale; 
            sliver_dist = line->dist; 
            sliver_row = line->row; 
            sliver_map = line->texnum; 
            sliver_light = line->lightnum; 
            sliver_zoneamb = line->zoneambient; 
            sliver_usefog = line->usefog; 
 
            if (sliver_light == -1) sliver_sptlt = line->sptlight; 
 
            // And call the rendering routines. 
            if (TextureFlags[sliver_map+1].IsTranslucent == 1) 
            { 
                if (TextureFlags[sliver_map+1].IsTransparent == 1) 
                    draw_sliver_trans(); 
            } 
            else 
            { 
                if (TextureFlags[sliver_map+1].IsTransparent == 1) 
                    draw_sliver_transparent(); 
                else 
                    draw_sliver(); 
            } 
        } 
    } 
} 
 
// The last few functions perform the game logic... 

// 
//  This routine is called each time the player tries to open a door. It finds an empty 
// spot in the Opening_Doors struct to insert necessary information, Inserts the info, 
// allocates a new spot for the temporary texture info (which is animated), and sets 
// it's flags. 
// 
int start_door(int x, int y) 
{ 
    int i;                                  // Generic loop index variable. 
    register int TxtNum;                    // Texture number of the original door. 
    register struct Open_Doors_Info * Door; // Pointer to the used entry of Opening_Doors[]. 
 
    // Check to see if the specified block is in fact a real door... 
    if (!TextureFlags[*(world+(((long)y)<<6)+x)].IsDoor) 
        return 1; 
    // Check to see if we are ALREADY opening the current door. 
    for (i=0; i<10; ++i) 
        if ((Opening_Doors[i].USED != 0) && (Opening_Doors[i].x == x) && (Opening_Doors[i].y == y)) 
            return 1; 
    // Now, find an unused entry in the array. 
    for (i=0; i<10; ++i) 
        if (Opening_Doors[i].USED == 0) 
            break; 
    // If the array is full, we can't open it. Return adn make the user wait... 
    if (i >= 10) 
        return 1; 
    // Get the address of the empty spot. Again, this saves time later on. 
    Door = &Opening_Doors[i]; 
    // Get the map texture number for the original door. 
    TxtNum = *(world+y*64+x); 
 
  // Now, we start to fill in the Door info. 
    Door->USED = 1;                           // This one is now used. 
    Door->x = x;                              // It is located at these map coords. 
    Door->y = y; 
    Door->Orig_Txt_Num = TxtNum-1;            // And uses this texture. 
    Door->New_Txt_Num = 40+i;                 // We're putting the animated texture here 
    Door->Pcnt_opened = 0;                    // It's not open yet. 
    Door->Num_Col_Opnd = 0; 
    Door->Opening = 1;                        // But it's going to be. 
    Door->delay = TextureFlags[TxtNum].delay; // Set the door's delay value. 
 
    // OK. Now we make a new copy of the door. 
    if (!(WallTextures[40+i] = (char  *)malloc(16384l))) 
		return Die("Error allocating memory for opening door. \n"); 
    
	// Copy the texture over. 
    memcpy(WallTextures[40+i],WallTextures[TxtNum-1],16384l); 
 
    // Now, copy the textures parameters over... 
    TextureFlags[i+41].IsTransparent = 1;               // It's definately transparent now. 
    TextureFlags[i+41].CanWalkThru = 0;                 // Can't go through it yet. 
    TextureFlags[i+41].IsDoor = 1;                      // It's a door for sure. 
    TextureFlags[i+41].DoorType = TextureFlags[TxtNum].DoorType; 
    TextureFlags[i+41].IsTranslucent = TextureFlags[TxtNum].IsTranslucent; 
    TextureFlags[i+41].IsSecretDoor = TextureFlags[TxtNum].IsSecretDoor; 
    TextureFlags[i+41].IsRecessed  = TextureFlags[TxtNum].IsRecessed; 
    TextureFlags[i+41].bc = TextureFlags[TxtNum].bc;    // These define the rectangle to animate. 
    TextureFlags[i+41].br = TextureFlags[TxtNum].br; 
    TextureFlags[i+41].ec = TextureFlags[TxtNum].ec; 
    TextureFlags[i+41].er = TextureFlags[TxtNum].er; 
    TextureFlags[i+41].speed = TextureFlags[TxtNum].speed; // This is the amt. to move the door each frame. 
 
  // Now we place the new texture information into the map. 
    *(world+y*64+x) = 41+i; 

	return 1;
} 
 
// 
//   This function loops through the Opening_Doors aray, and opens each active door a 
// little bit more, based on it's speed. When 90% opened, it sets the "CanWalkThru" 
// flag. When 100% opened, it clears the Opening flag. 
// 
void open_doors() 
{ 
    int i,          // Outer loop index variable. 
        x,          // Loop variable for the animation routine. 
        y, 
        bc,         // These define the rectangle that is to be animated in the texture. 
        br, 
        ec, 
        er, 
        mc, 
        spd;        // This is the amount to move the animated part of the texture. 
    char  *base; // This points to the door's texture. 
    struct TextureFlagsTYPE *TextUsed; // Points to the door's texture info. 
 
  // For each entry in the Opening_Doors array, 
    for (i=0; i<10; ++i) 
        // If it is active, and opening... 
        if ((Opening_Doors[i].USED) && (Opening_Doors[i].Opening) && (Opening_Doors[i].Pcnt_opened < 100)) 
        { 
            // Get local copies of it's texture innfo. This is, again, faster. 
            TextUsed = &(TextureFlags[Opening_Doors[i].New_Txt_Num+1]); 
            bc = TextUsed->bc; 
            br = TextUsed->br; 
            ec = TextUsed->ec; 
            er = TextUsed->er; 
            mc = (ec-bc)/2 + bc; 
            // Get a pointer to the bitmap. 
            base = WallTextures[Opening_Doors[i].New_Txt_Num]; 
            // and it's speed. 
            spd = TextureFlags[Opening_Doors[i].New_Txt_Num+1].speed; 
            // Now animate the door. 
            switch ( TextUsed->DoorType ) // 1=normal, 2=elevator, 3=garage 
            { 
            case 1: // Normal door. 
                // Move bitmap of door area over by 1 column . 
                for (x=ec; x >= bc+spd; --x) 
                    memcpy(base+(x*128l)+br,base+br+((x-spd)*128l),er-br+1); 
                // Clear out the edge, so we can see through it. 
                for ( ; x>= bc; --x) 
                    memset(base+(x*128l)+br,0,er-br); 
                // Update the percent opened. 
                Opening_Doors[i].Pcnt_opened += ((100*spd)/(ec-bc)); 
                break; 
            case 2: // Elevator style door. Splits in the middle, scrolling to the sides. 
                // Move bitmap towards left side from the middle... 
                for (x=ec; x > mc+spd; --x) 
                    memcpy(base+(x*128l)+br,base+br+((x-spd)*128l),er-br+1); 
                // Clear out the middle, so we can see through it. 
                for ( ; x > mc; --x) 
                    memset(base+(x*128l)+br,0,er-br); 
                // Move bitmap towards right side from the middle... 
                for (x=bc; x <= mc-spd; ++x) 
                    memcpy(base+(x*128l)+br,base+br+((x+spd)*128l),er-br+1); 
                // Clear out the middle, so we can see through it. 
                for ( ; x <= mc; ++x) 
                    memset(base+(x*128l)+br,0,er-br); 
                // Update the percent opened. (these open twice as fast) 
                Opening_Doors[i].Pcnt_opened += ((200*spd)/(ec-bc)); 
                break; 
            case 3: // Garage style door. Slides up from the bottom. 
                // Move bitmap towards top side from the bottom... 
                for (x=bc ; x <= ec; ++x) 
                { 
                    for (y=br; y <= er - spd; ++y ) 
                        *(base+y+(x*128l)) = *(base+(y+spd)+(x*128l)); 
                    for ( ; y <= er ; ++y) 
                        *(base+y+(x*128l)) = 0; 
                } 
                // Update the percent opened. 
                Opening_Doors[i].Pcnt_opened += ((100*spd)/(er-br)); 
                break; 
            } 
            // If it's 90% opened, let people walk through it. 
            if (Opening_Doors[i].Pcnt_opened >= 90) 
                TextUsed->CanWalkThru = 1; 
            // If it's 100% open, mark it as closing. 
            if (Opening_Doors[i].Pcnt_opened >= 100) 
                Opening_Doors[i].Opening = 0; 
        } 
} 
 
// 
//    This function closes all Active doors that are marked with "Opening=0". It copies 
//  part of the original bitmap back into the animated texture, and updates it's state. 
//  When the Percent_Opened is less than 90% it clears the CanWalkThru flag, which keeps 
//  anyone from ealking through the texture anymore. When it reaches 0%, the original door 
//  is put back into the map, and the animated texture space is deleted. 
// 
void close_doors() 
{ 
    int i,                   // Outer loop index variable. 
        x,                   // Bitmap copy loop index variable. 
        bc,                  // These define the rectangle to be animated on the texture. 
        br, 
        ec, 
        er, 
        mc, 
        spd;                 // This is how much to shift the door each time. 
    char  *base,          // Pointer to the animated texture. 
         *oldbase;       // Pointer to the original texture. 
    struct TextureFlagsTYPE *TextUsed; // Pointer to the texture info for the door. 
 
    // For each entry in the Opening_Doors array... 
    for (i=0; i<10; ++i) 
        // If we are REALLY closing this door... 
        if ((Opening_Doors[i].USED) && (!Opening_Doors[i].Opening) && (Opening_Doors[i].Pcnt_opened > 0)) 
        { 
            // We check the delay first. Decrement it until it reaches zero first. 
            if (Opening_Doors[i].delay > 0) 
                --Opening_Doors[i].delay; 
            // Afterwards, start animating the texture. 
            else 
            { 
                // We copy the texture's information into local variables to save time. 
                TextUsed = &(TextureFlags[Opening_Doors[i].New_Txt_Num+1]); 
                bc = TextUsed->bc; 
                br = TextUsed->br; 
                ec = TextUsed->ec; 
                er = TextUsed->er; 
                mc = (ec-bc)/2 + bc; 
                spd = TextUsed->speed; 
                base = WallTextures[Opening_Doors[i].New_Txt_Num]; 
                oldbase = WallTextures[Opening_Doors[i].Orig_Txt_Num]; 
 
                switch ( TextUsed->DoorType ) // 1=normal, 2=elevator, 3=garage 
                { 
                case 1: // Normal door. 
                    // We copy part of the bitmap of the old door to the new door. 
                    // The part is defined bu Num_Col_Opnd. 
                    for (x = Opening_Doors[i].Num_Col_Opnd; x >= 0; --x) 
                        memcpy(base+(ec-Opening_Doors[i].Num_Col_Opnd+x)*128l+br,oldbase+(x+bc)*128l+br,er-br); 
                    // We update the size of the "copy wndow". 
                    Opening_Doors[i].Num_Col_Opnd += spd; 
                    // and clip it to a maximum amount (the size of the rectangle) 
                    if (Opening_Doors[i].Num_Col_Opnd > (ec-bc)) 
                        Opening_Doors[i].Num_Col_Opnd = ec-bc; 
                    // And we update the percent opened. 
                    Opening_Doors[i].Pcnt_opened -= ((100 * spd)/(ec-bc)); 
                    break; 
                case 2: // Elevator style doors. 
                    // We copy part of the bitmap of the old door to the new door. 
                    // The part is defined bu Num_Col_Opnd. 
                    for (x = 0 ; x <= Opening_Doors[i].Num_Col_Opnd; ++x) 
                    { 
                        memcpy(base+(ec-x)*128l+br,oldbase+(mc+Opening_Doors[i].Num_Col_Opnd-x)*128l+br,er-br); 
                        memcpy(base+(bc+x)*128l+br,oldbase+(mc-Opening_Doors[i].Num_Col_Opnd+x)*128l+br,er-br); 
                    } 
                    // We update the size of the "copy wndow". 
                    Opening_Doors[i].Num_Col_Opnd += spd; 
                    // and clip it to a maximum amount (the size of a side door.) 
                    if (Opening_Doors[i].Num_Col_Opnd > (ec-bc)/2 ) 
                        Opening_Doors[i].Num_Col_Opnd = (ec-bc)/2; 
                    // And we update the percent opened. 
                    Opening_Doors[i].Pcnt_opened -= ((200 * spd)/(ec-bc)); 
                    break; 
                case 3: // Garage style doors. 
                    // We copy part of the bitmap of the old door to the new door. 
                    // The part is defined bu Num_Col_Opnd. 
                    for(x=bc; x<=ec ; ++x) 
                        memcpy(base+x*128l+br,oldbase+x*128l+er-Opening_Doors[i].Num_Col_Opnd,Opening_Doors[i].Num_Col_Opnd+1); 
                    // We update the size of the "copy wndow". 
                    Opening_Doors[i].Num_Col_Opnd += spd; 
                    // and clip it to a maximum amount (the size of a door.) 
                    if (Opening_Doors[i].Num_Col_Opnd > (er-br)) 
                        Opening_Doors[i].Num_Col_Opnd = (er-br); 
                    // And we update the percent opened. 
                    Opening_Doors[i].Pcnt_opened -= ((100 * spd)/(er-br)); 
                    break; 
                } 
                // If we've closed enough, no one can walk through... 
                if (Opening_Doors[i].Pcnt_opened < 90) 
                    TextureFlags[Opening_Doors[i].New_Txt_Num+1].CanWalkThru = 0; 
                // If we're completely closed, put the old door back. 
                if (Opening_Doors[i].Pcnt_opened <= 0) 
                { 
                    free(base); 
					WallTextures[Opening_Doors[i].New_Txt_Num] = NULL;
                    Opening_Doors[i].USED = 0; // Mark this array entry as available... 
                    *(world+Opening_Doors[i].y*64+Opening_Doors[i].x) = Opening_Doors[i].Orig_Txt_Num+1; 
                } 
            } 
        } 
} 

//
//	  This method performs the action associated with a trigger that has
//  been activated. This is still an area of expansion.
// 
void ProcessTrigger(long &x, long &y, long &view_angle, int TriggerNbr) 
{ 
    switch (Triggers[TriggerNbr].Trigger_Type) 
    { 
    case 1: // Open door at location. 
        start_door(Triggers[TriggerNbr].MapX,Triggers[TriggerNbr].Mapy); 
        break; 
    case 2: // Lock door at location. 
        break; 
    case 3: // Close door at location. 
        break; 
    case 4: // Start animated texture. 
        Animations[TextureFlags[Triggers[TriggerNbr].TxtNbr].AnimNbr].Flagged = 1; 
        break; 
    case 5: // Stop animated texture. 
        Animations[TextureFlags[Triggers[TriggerNbr].TxtNbr].AnimNbr].Flagged = 0; 
        break; 
    case 6: // Set ambient light level. 
        ambient_level = Triggers[TriggerNbr].NewLightLvl; 
        break; 
    case 7: // Set zone light level. 
        ZoneAttr[Triggers[TriggerNbr].ZoneNbr].ambient = Triggers[TriggerNbr].NewLightLvl; 
        break; 
    case 8: // Load new map. 
        Load_Map(x,y,view_angle,Triggers[TriggerNbr].NewFileName); 
        // Now how about a special effect... 
        copyscrn(BkgBuffer); 
        Render_View(x,y,view_angle); 
        blit();
        break; 
    case 9: // Teleport player. 
        x = Triggers[TriggerNbr].MapX; 
        y = Triggers[TriggerNbr].Mapy; 
        view_angle = Triggers[TriggerNbr].NewAngle; 
                                // Now how about a special effect... 
        break; 
    case 10: // Load new texture. 
        break; 
    case 11: // Change wall map at location. 
        *(world + (Triggers[TriggerNbr].Mapy-1)*64l + Triggers[TriggerNbr].MapX-1) = Triggers[TriggerNbr].TxtNbr; 
        break; 
    case 12: // Change floor map at location. 
        *(FloorMap + (Triggers[TriggerNbr].Mapy-1) * 64l + Triggers[TriggerNbr].MapX-1) = Triggers[TriggerNbr].TxtNbr; 
        break; 
    case 13: // Change ceiling map at location. 
        *(CeilMap + (Triggers[TriggerNbr].MapX-1)*64l + Triggers[TriggerNbr].MapX-1) = Triggers[TriggerNbr].TxtNbr; 
        break; 
    case 14: // Change floor lighting map at location 
        *(FloorLightMap+(Triggers[TriggerNbr].MapX-1)*64l+Triggers[TriggerNbr].MapX-1) = Triggers[TriggerNbr].TxtNbr; 
        break; 
    case 15: // Change ceiling lighting map at location. 
        *(CeilLightMap+(Triggers[TriggerNbr].MapX-1)*64l+Triggers[TriggerNbr].MapX-1) = Triggers[TriggerNbr].TxtNbr; 
        break; 
    case 16: // Start animated light texture. 
        Light_Animations[LightFlags[Triggers[TriggerNbr].TxtNbr].AnimNbr].Flagged = 1; 
        break; 
    case 17: // Stop animated light texture. 
        Light_Animations[LightFlags[Triggers[TriggerNbr].TxtNbr].AnimNbr].Flagged = 0; 
        break; 
    } 
} 
 
//
// This function checks to see if the player (or other creature later on) sets off a trigger. 
// If not, then nothing happens. If so, then watch out... 
//
void CheckTriggers(long &x, long &y, long &view_angle) 
{ 
    int Trigger_Nbr; 
    int indx; 
 
    if ((Trigger_Nbr = TriggerMap[x>>7][y>>7]) != 0) 
    { 
        for ( indx = ((Trigger_Nbr < 100)?Trigger_Nbr:1); indx <= Num_Triggers; ++indx) 
        { 
            // Is the player inside the area of the current trigger? 
            if (Triggers[indx].Trigger_Area_Type == 1) 
            { 
                if (((x >=  Triggers[indx].X1) && (x <=  Triggers[indx].X2)) && ((y >=  Triggers[indx].Y1) && (y <=  Triggers[indx].Y2))) 
                    ProcessTrigger(x,y,view_angle,indx); 
            } 
            else 
                if (((x - Triggers[indx].X1) * (x - Triggers[indx].X1) + (y - Triggers[indx].Y1) * (y - Triggers[indx].Y1)) <= Triggers[indx].Radius * Triggers[indx].Radius ) 
                    ProcessTrigger(x,y,view_angle,indx); 
            if (Trigger_Nbr < 100 ) 
                break; 
        } 
    } 
} 

//
//     This function loops through all of the animations loaded into the map, 
// updates all of the timers, and if needed, cycles the animation to the next frame. 
// 
int Rotate_Animations(void) 
{ 
    int index;   // Generic loop index. 
    // 
    // Wall / Floor / Ceiling animations. 
    // 
    // Step 1. Loop through the animations array (for all defined animations) 
    for (index = 0; index < Num_Animations; ++index) 
    { 
        // Step 1a. Is the current animation triggered? 
        if ( Animations[index].Flagged == 1 ) 
        { 
            // Step 1b. Decrement the timer for the current animation. 
            if ( --Animations[index].Timer == 0) 
            { 
                // Step 1c. If = 0 then increment the frame counter. 
                Animations[index].Curr_Frame++; 
                // Step 1d. Make sure to reset to the 1st frame if we're at the end. 
                if ( Animations[index].Curr_Frame >= Animations[index].Nbr_Frames ) 
                    Animations[index].Curr_Frame = 0; 
                // Step 1e. Set the new frame. 
                WallTextures[Animations[index].Txt_Nbr] = Animations[index].Frames[Animations[index].Curr_Frame]; 
                // Step 1f. Reset the delay. 
                Animations[index].Timer =  Animations[index].Delay; 
            } 
        } 
    } 
    // 
    // Light Animations 
    // 
    for (index = 0; index < Num_LightAnimations; ++index) 
    { 
        // Step 1a. Is the current animation triggered? 
        if ( Light_Animations[index].Flagged == 1 ) 
        { 
            // Step 1b. Decrement the timer for the current animation. 
            if ( --Light_Animations[index].Timer == 0) 
            { 
                // Step 1c. If = 0 then increment the frame counter. 
                Light_Animations[index].Curr_Frame++; 
                // Step 1d. Make sure to reset to the 1st frame if we're at the end. 
                if ( Light_Animations[index].Curr_Frame >= Light_Animations[index].Nbr_Frames ) 
                    Light_Animations[index].Curr_Frame = 0; 
                // Step 1e. Set the new frame. 
                LightMapTiles[Light_Animations[index].Txt_Nbr] = Light_Animations[index].Frames[Light_Animations[index].Curr_Frame]; 
                // Step 1f. Reset the delay. 
                Light_Animations[index].Timer =  Light_Animations[index].Delay; 
            } 
        } 
    } 
    return 1; 
} 
 
//
// This function will rotate the blinking lights through thier patterns... 
// 
void processblinklights(void) 
{ 
    int i; 
 
    for (i=1; i <= Num_Light_Tiles; ++i) 
    { 
        ++LightFlags[i].CurrPatternIndx; 
        if (LightFlags[i].LightType == 1) 
        { 
            if ( LightFlags[i].CurrPatternIndx < LightFlags[i].PulseWidth ) // Light is ON 
                LightMapTiles[i] = LightFlags[i].OriginalBitmap; 
            else 
                LightMapTiles[i] = LightMapTiles[0]; 
            if ( LightFlags[i].CurrPatternIndx >= LightFlags[i].Period ) LightFlags[i].CurrPatternIndx = 0; 
        } 
        else if ( LightFlags[i].LightType == 2 ) 
        { 
            if ( LightFlags[i].PulsePattern[LightFlags[i].CurrPatternIndx] == NULL ) LightFlags[i].CurrPatternIndx = 0; 
            if ( LightFlags[i].PulsePattern[LightFlags[i].CurrPatternIndx] == '0' ) 
                LightMapTiles[i] = LightMapTiles[0]; 
            else 
                LightMapTiles[i] = LightFlags[i].OriginalBitmap; 
        } 
    } 
} 

// 
//     This method performs the user movement and collision detection.
//
void ProccessUserMovement( void )
{
    long x,                    // The players X position, in fine coordinates. 
         y,                    // The players Y position, in fine coordinates. 
         view_angle,           // The angle which the player is facing. 
         view_angle_90,        // The angle the player is facing + 90 degreess. (Used for collision & door processing) 
         x_cell,               // The map cell COL the player is in. (Used for collision & door processing) 
         y_cell,               // The map cell ROW the player is in. 
         x_sub_cell,           // The fine coordinate inside the cell (0-63) of the player. 
         y_sub_cell,           // The fine coordinate inside the cell (0-63) of the player.
         dx,                   // The amount to move the player in X this round. 
         dy;                   // The amount to move the player in Y this round. 

	x = Player_1.x;
	y = Player_1.y;
	view_angle = Player_1.view_angle;
	x_cell = Player_1.x>>7; 
	y_cell = Player_1.y>>7; 
	x_sub_cell = Player_1.x & 127; 
    y_sub_cell = Player_1.y & 127;
	
    // Step 3. Check for movement. The lower nibble of KeyState is for movement ONLY. ( Arrow Keys )
    if ( KEY_DOWN(VK_LEFT) || KEY_DOWN(VK_RIGHT) || KEY_DOWN(VK_UP) || KEY_DOWN(VK_DOWN) )
    { 
        // Reset dx and dy. 
        dx=dy=0; 
        // If the player is pressing the Right arrow, 
        if (KEY_DOWN(VK_LEFT)) 
        { 
            // Decrement the view angle by 6 degrees, wrap at zero. 
            if ((view_angle-=ANGLE_6)<ANGLE_0) 
                view_angle+=ANGLE_360; 
        } 
        // Else, if the player is pressing the Left arrow. 
        if (KEY_DOWN(VK_RIGHT)) 
        { 
            // Increment the view angle by 6 degrees, Wrap at 360. 
            if ((view_angle+=ANGLE_6)>=ANGLE_360) 
                view_angle-=ANGLE_360; 
        } 
        // Else, if the player wants to go FORWARD... 
        if (KEY_DOWN(VK_UP)) 
        { 
            // Set view_angle_90, and wrap it around at 360 deg. 
            view_angle_90 = view_angle + ANGLE_90; 
            if (view_angle_90 > ANGLE_360) 
                view_angle_90 = view_angle_90 - ANGLE_360; 
            // Calculate the dx and dy to move forward, at the current angle. (Trig) 
            dx= (long)((sin_table[view_angle_90]*20)>>16); 
            dy= (long)((sin_table[view_angle]*20)>>16); 
        } 
        // Else, if the player wants to move BACKWARDS 
        if (KEY_DOWN(VK_DOWN)) 
        { 
            // Set view_angle_90, and wrap it around at 360 deg. 
            view_angle_90 = view_angle + ANGLE_90; 
            if (view_angle_90 > ANGLE_360) 
                view_angle_90 = view_angle_90 - ANGLE_360; 
            // Calculate the dx and dy to move backward, at the current angle. (Trig) 
            dx= -(long)((sin_table[view_angle_90]*20)>>16); 
            dy= -(long)((sin_table[view_angle]*20)>>16); 
        } 
        // Now , add the deltas to the current location, 
        x+=dx; 
        y+=dy; 
        // Calculate the x and y cell and subcell values, 
        x_cell = x>>7; 
        y_cell = y>>7; 
        x_sub_cell = x & 127; 
        y_sub_cell = y & 127; 
            
        // Step 4. Now, we do collision detection... (Bumping into walls..) 
        // If we're moving WEST, 
        if (dx>0) 
        { 
            if ((*(world+(y_cell<<6)+x_cell+1) != 0) &&      // If there'a wall there, 
                (!TextureFlags[*(world+(y_cell<<6)+x_cell+1)].CanWalkThru) && // And we can't walk through it, 
                ( ((x_sub_cell > CELL_X_SIZE-TOOCLOSE ) && (!TextureFlags[*(world+(y_cell<<6)+x_cell+1)].IsRecessed)) || //And we're too close to it, 
				  ((x_sub_cell > CELL_X_SIZE-TOOCLOSE + 30 ) && (TextureFlags[*(world+(y_cell<<6)+x_cell+1)].IsRecessed)))) // (don't forget recessed panels)
			{
				if (TextureFlags[*(world+(y_cell<<6)+x_cell+1)].IsRecessed)
					x-= (x_sub_cell-(CELL_X_SIZE-TOOCLOSE +30 )); // then MOVE US BACK A BIT. 
				else
					x-= (x_sub_cell-(CELL_X_SIZE-TOOCLOSE )); // then MOVE US BACK A BIT. 
			}
        } 
        // Same as above, but going EAST. 
        else 
        { 
            if ((*(world+(y_cell<<6)+x_cell-1) != 0) &&        // If there'a wall there, 
                (!TextureFlags[*(world+(y_cell<<6)+x_cell-1)].CanWalkThru) &&    // And we can't walk through it, 
                ( ((x_sub_cell < TOOCLOSE ) && (!TextureFlags[*(world+(y_cell<<6)+x_cell-1)].IsRecessed)) || //And we're too close to it, 
				  ((x_sub_cell < TOOCLOSE - 30 ) && (TextureFlags[*(world+(y_cell<<6)+x_cell-1)].IsRecessed)))) // (don't forget recessed panels)
			{
				if (TextureFlags[*(world+(y_cell<<6)+x_cell-1)].IsRecessed)
					x+= (TOOCLOSE - x_sub_cell - 30) ;     // then MOVE US BACK A BIT. 
				else
					x+= (TOOCLOSE - x_sub_cell) ;     // then MOVE US BACK A BIT. 
			}
        } 
        // If we're going NORTH, 
        if (dy>0 ) 
        { 
            if ((*(world+((y_cell+1)<<6)+x_cell) != 0) &&     // If there'a wall there, 
                (!TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].CanWalkThru) &&   // And we can't walk through it, 
                ( ((y_sub_cell > CELL_Y_SIZE-TOOCLOSE ) && (!TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].IsRecessed)) || //And we're too close to it, 
				  ((y_sub_cell > CELL_Y_SIZE-TOOCLOSE + 30 ) && (TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].IsRecessed)))) // (don't forget recessed panels)
			{
				if (TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].IsRecessed)
					y-= (y_sub_cell-(CELL_Y_SIZE-TOOCLOSE + 30 ));           // then MOVE US BACK A BIT. 
				else
					y-= (y_sub_cell-(CELL_Y_SIZE-TOOCLOSE ));           // then MOVE US BACK A BIT. 
			}
        } 
        // Else, if we're going SOUTH 
        else 
        { 
            if ((*(world+((y_cell-1)<<6)+x_cell) != 0) &&       // If there'a wall there, 
                (!TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].CanWalkThru) &&     // And we can't walk through it, 
                ( ((y_sub_cell < TOOCLOSE ) && (!TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].IsRecessed)) || //And we're too close to it, 
				  ((y_sub_cell < TOOCLOSE - 30 ) && (TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].IsRecessed)))) // (don't forget recessed panels)
			{
				if (TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].IsRecessed)
					y+= (TOOCLOSE-y_sub_cell - 30);           // then MOVE US BACK A BIT. 
				else
					y+= (TOOCLOSE-y_sub_cell);           // then MOVE US BACK A BIT. 
			}
        } 
	}

	Player_1.x = x;
	Player_1.y = y;
	Player_1.view_angle = view_angle;
	
	return;
}

//
//   This method sets up the rendering engine.
//    
int StartEngine(void) 
{ 
    // make all the trig and support tables. 
    if ( Build_Tables() < 0 )
		return -1; 
    
	// Now, load the automap image.
    AutoMapBkgd = Load_PCX("automap.pcx",70*70,0); 
	if(NULL == AutoMapBkgd )
		return -1;
    
	// Load in the map, textures, etc. and set the player's initital position. 
    if ( Load_Map(Player_1.x,Player_1.y,Player_1.view_angle,"raymap.dat") < 0 )
		return -1;
    
	// Load the background image.
	if ( load_bkgd("BKGD.PCX") < 0)
		return -1;

	//Initialize the double buffer
	init_double_buffer();

    // Copy it to the double buffer, 
    copyscrn(BkgBuffer); 
    
	// Dump it to the screen, 
    blit();

	return 1;
} 

int InitTitleAnimation (void)
{

	// Load the game title.
	if ( load_bkgd("TITLE.PCX") < 0)
		return -1;

   	//Initialize the double buffer
	init_double_buffer();

    // Copy it to the double buffer, 
    copyscrn(BkgBuffer); 
    	
	// Dump it to the screen, 
    blit();

	// Allocate memory for the fire animation.
	FireSrc = (char far *)malloc((unsigned)64321); 
    if (FireSrc == NULL) 
 	{
		Die("Unable to allocate the fire animation buffer."); 
		return -1;
 	}
    memset(FireSrc,0,64320); 
 
	// Seed the random number generator.
 	srand( (unsigned)time( NULL ) );
 
 	return 1;
}

int EndTitleAnimation (void )
{
	if (FireSrc != NULL)
		free(FireSrc);
 
 	return 1;
}
 
void AnimateFireScreen (void) 
{ 
    unsigned short I; 
    unsigned long  x; 
    unsigned long  y; 
    int            value; 
  
    copyscrn(BkgBuffer); 
    
	for (I=0; I < 319; ++I)                // Place a random bottom on Src. 
	{
		if ((rand() % 6) > 1)
			*(FireSrc+64000+I) = 112; 
        else 
			*(FireSrc+64000+I) = 0; 
	}
 
    for (y=100; y<200; ++y)                  // Average pixels, moving up 1 row 
 	{
 		for (x=1; x<319; ++x)                 // adding the decay. 
         { 
 			value = (*(FireSrc+x-1+(y * 320ul))+*(FireSrc+x+1+(y * 320ul)) 
                    +*(FireSrc+x-1+((y+1) * 320ul))+*(FireSrc+x+((y+1) * 320ul)) 
                    +*(FireSrc+x+1+((y+1) * 320ul )))/5 - 1; 
            if ((value > 0) && y < 196) 
				*(double_buffer+x+((y+4) * 320ul)) = value; 
            *(FireSrc+x+((y-1) * 320ul)) = value; 
        } 
	}

	blit();
}


//
//     This method performs the main game loop. It calls a set of methods to do background processing,
//  then it performs the user movement, and then processing of triggers. Finally, it renders the view.
//
void MainLoop(void)
{
    // Step 1. Process background things (doors, animations, lights, etc.)
    open_doors(); // Open all opening doors a little more. 
    close_doors(); // close all closing doors a little more. 
    Rotate_Animations(); // Cycle the animations. 
    processblinklights();  // Do the next frame in linking lights.
    CheckTriggers(Player_1.x,Player_1.y,Player_1.view_angle); // Check the triggers, and perform any actions.
        
	// Step 2. Allow the user to move
	ProccessUserMovement();
	
    // Step 3. Render the new view.
    copyscrn(BkgBuffer); // Clear the double buffer to the blank background.
    Render_View(Player_1.x,Player_1.y,Player_1.view_angle); // Render the view on top of the background.
    if (ShowAutomap) DrawAutomap(Player_1.x,Player_1.y); // Render the automap, if selected.
    blit();// Send it all to the screen. 

	return;
}

//
//    The main entry function. It creates the main widow, and calls the necessary functions to 
// initialize the engine, and then enters the main message loop.
//
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	HWND hWnd; 
	WNDCLASSEX wc;
	bIsActive = FALSE;
	bRunGame = FALSE;

	// Register the main application window class. 
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)WndProc;       
	wc.cbClsExtra = 0;                      
	wc.cbWndExtra = 0;                      
	wc.hInstance = hInstance;              
	wc.hIcon = LoadIcon(hInstance, lpszTitle); 
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = lpszAppName;              
	wc.lpszClassName = lpszAppName;              
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIconSm = (HICON)LoadImage(hInstance, lpszTitle, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	if(!RegisterClassEx(&wc))
	{
		return(FALSE);
	}

	hInst = hInstance; 

	// Create the main application window.
	hWnd = CreateWindowEx(WS_OVERLAPPED, lpszAppName, lpszTitle, WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME, 
			      50, 50, (320 + 6), (200 + 44), NULL, NULL, hInstance, NULL);

	if(!hWnd)
	{
		return(FALSE);
	}

	ShowWindow(hWnd, nCmdShow); 
	UpdateWindow(hWnd); 
	ghWnd = hWnd;
	DblBuff = NULL;

	// Prepare for the title screen and the animation.
	if ( InitTitleAnimation() < 0 )
		return(-1); 

	// We're going to try and use the windows timer to set the upper limit to 60 frames / sec.
	SetTimer(ghWnd, 1, 20, NULL);

	// enter main event loop
	while(1) 
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// test if this is a quit
			if(msg.message == WM_QUIT)
			{
				break;
			}
	
			// translate any accelerator keys
			TranslateMessage(&msg);

			// send the message to the window proc
			DispatchMessage(&msg);
		}
	}

	KillTimer(ghWnd, 1);
	free_tables();

	return(msg.wParam); 
}

//
//     The winproc for the main window handles messages from the menu bar, and from user input.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	long x_cell;
	long y_cell;

	switch(message){
		case WM_ACTIVATEAPP:
			bIsActive = (BOOL) wParam;
			break;

		case WM_COMMAND:
			switch (wParam)
			{
				case IDM_ABOUT:
					DialogBox(hInst,"AboutBox", hWnd,(DLGPROC)About);
					break;

				case IDM_SHOWKEYS:
					DialogBox(hInst, "KeysList", hWnd,(DLGPROC)About);
					break;

				case IDM_RUN:
					StartEngine() ;
					bRunGame = TRUE;
					break;

				case IDM_EXIT:
					PostMessage(hWnd, WM_CLOSE, 0, 0);
					break;

				case IDM_DISPLAY_320:
					MoveWindow(ghWnd, ClientxPos, ClientyPos, 320, 240, TRUE);
					break;

				case IDM_DISPLAY_640:
					MoveWindow(ghWnd, ClientxPos, ClientyPos, 640, 480, TRUE);
					break;

				case IDM_DISPLAY_960:
					MoveWindow(ghWnd, ClientxPos, ClientyPos, 960, 720, TRUE);
					break;
			}
			break;
	
		case WM_KEYDOWN:
			// Check to see if the user wants to see the auto map.
			if (((int)wParam == VK_TAB ) && (bRunGame == TRUE ))
				ShowAutomap = !ShowAutomap;
			
			// Check to see if the user wants to quit (ESC KEY).
			if((int)wParam == VK_ESCAPE)
				PostMessage(ghWnd,WM_CLOSE,0,0);

			// Check to se if the user is requesting a screen shot ( F1 Key ). 
		    if ((int)wParam == VK_F1)
			    Save_Pcx("scrnsht.pcx",double_buffer,320ul,200ul); 

		    // Now check to see if the player wants to change the torch level ( F2 Key ). 
			if (((int)wParam == VK_F2) && (bRunGame == TRUE ))
			{ 
				--TorchLevel; 
				if (TorchLevel == 0) 
					TorchLevel = 8; 
				CalcTorchLevel(TorchLevel); 
			} 
			
		    // Check to see if the player wants to open a door ( SPACE key ). 
			if (((int)wParam == VK_SPACE) && (bRunGame == TRUE ))
			{ 
				x_cell = Player_1.x>>7; 
				y_cell = Player_1.y>>7; 

				// If the player is facing to the WEST 
				if ((Player_1.view_angle >  ANGLE_330) || (Player_1.view_angle < ANGLE_30)) 
				{ 
					// If it really is a door right next to him (1 cell away), 
					if (TextureFlags[*(world+(y_cell<<6)+x_cell+1)].IsDoor) 
						// Then start it opening. 
						start_door(x_cell+1,y_cell); 
				} 
				// Else see if it's to to the north. 
				else if ((Player_1.view_angle >  ANGLE_60) && (Player_1.view_angle < ANGLE_120)) 
				{ 
					if (TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].IsDoor) 
						start_door(x_cell,y_cell+1); 
				} 
				// Else, see if it's to the west. 
				else if ((Player_1.view_angle >  ANGLE_150) && (Player_1.view_angle < ANGLE_210)) 
				{ 
					if (TextureFlags[*(world+(y_cell<<6)+(x_cell-1))].IsDoor) 
						start_door(x_cell-1,y_cell); 
				} 
				// Else, see if it's to the south. 
				else if ((Player_1.view_angle >  ANGLE_240) && (Player_1.view_angle < ANGLE_300)) 
				{ 
					if (TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].IsDoor) 
						start_door(x_cell,y_cell-1); 
				} 
			} 
			
			break;

		case WM_SIZE:
			ClientWidth = LOWORD(lParam);
			ClientHeight = HIWORD(lParam);
			break;

		case WM_MOVE :
			ClientxPos = (int) LOWORD(lParam); 
			ClientyPos = (int) HIWORD(lParam);
			break;

		case WM_PAINT:
			BeginPaint(hWnd, &ps );
			EndPaint(hWnd, &ps );
			break;

		case WM_TIMER:
			if (bRunGame == TRUE )
				MainLoop();  // call main logic module.
			else
				AnimateFireScreen(); // Call title screen animation module.
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
 
//
//     The windproc for the ABOUT box.
//
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
		case WM_INITDIALOG: 
			return(TRUE);
		break;

		case WM_COMMAND:                              
			if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, TRUE);        
				return(TRUE);
			}
		break;
	}

	return(FALSE);
}


