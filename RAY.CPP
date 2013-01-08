/************************************************************************/
/***                                                                  ***/
/***      R A Y . C P P                    (C) 1995 NPS Software      ***/
/***                                                                  ***/
/***                Writen By:        Brad Broerman                   ***/
/***                Last Modified:    May 6, 1997                     ***/
/***                Version:          0.5                             ***/
/***                                                                  ***/
/***                                                                  ***/
/***    A 3-D Raycasting graphics engine with up to a 64x64 cell map  ***/
/***  35 textures, 20 levels of transparency, animated doors, floor   ***/
/***  and ceiling texturing.                                          ***/
/***                                                                  ***/
/***   To be added: Extended memory to hold textures and sprite data  ***/
/***  4 more door types, light mapping, and depth cueing.             ***/
/***                                                                  ***/
/************************************************************************/

#include <time.h>
#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#define CAST_FLOORCIEL
// How close you can get to a wall.
#define OVERBOARD 30

// Our definitions for the angles.  ANGLE_60 = width of viewport. (60 deg. FOV)
// All the other angles are based on this.
#define ANGLE_0     0
#define ANGLE_1     4
#define ANGLE_2     8
#define ANGLE_4     16
#define ANGLE_5     20
#define ANGLE_6     24
#define ANGLE_10	40
#define ANGLE_15    60
#define ANGLE_20	80
#define ANGLE_30    120
#define ANGLE_45    180
#define ANGLE_60    240
#define ANGLE_90    360
#define ANGLE_120   480
#define ANGLE_135   540
#define ANGLE_150   600
#define ANGLE_180   720
#define ANGLE_210   840
#define ANGLE_225   900
#define ANGLE_240   960
#define ANGLE_270   1080
#define ANGLE_300   1200
#define ANGLE_315   1260
#define ANGLE_330   1320
#define ANGLE_360   1440

// The size of each cell. 64 makes the math easier. (also textures are 64 pixels wide)
#define CELL_X_SIZE   64
#define CELL_Y_SIZE   64

// The maximum size of our world 64x64 cells.
#define MAX_WORLD_ROWS 64
#define MAX_WORLD_COLS 64

// We need a large stack.
extern unsigned _stklen = 20480U;

// The generic PCX header structure. I've listed some important fields, but
// I won't check for them until this code is "officially" released.
typedef struct PCX_HEADER_TYPE
 {
  char  manufacturer;    // We don't care about these...
  char  version;
  char  encoding;
  char  bits_per_pixel;  // We want an 8 here...
  int   x,y;             // We ignore these.
  int   width,height;    // Will be either 64x64 or 320x200
  int   horiz_rez;       // Forget about these...
  int   vert_rez;
  char  EGA_palette[48]; // We won't even touch this.
  char  reserved;
  char  Num_Planes;      // We want 1
  int   bytes_per_line;  // Either 64 or 320
  int   palette_type;    // We can forget about these too..
  char  padding[58];
 } PCX_HEADER;

// Stores slice information for rendering. This is used so that we can
// add sprites, transparency, etc. and have the rendering engine draw them too.
struct col_obj
 {
  int top,     // Wihch row to begin drawing this slice.
	  col,     // Which screen column this slice is in.
	  scale,   // How much do I scale this slice
	  dist,    // How far away is it (Used later on, for sorting in sprites)
	  row,     // Which texture column to render for this slice.
	  texnum;  // The number of the texture to use.
 };

// This implements the multiple levels of transparency we can use. The
// objecs are ordered from nearest to farthest.
struct scan_line_t
 {
  col_obj line[20];  // We can go to 20 levels of transparency, sprites, etc.
  int num_objs;
 };

// This structure is kept for every texture map loaded into the game. It is
// used for doors and transparent items.
struct TextureFlagsTYPE
 {
  char IsTransparent, // 1=transparent, 0=not.
	   IsDoor,        // 1= door, 0= regular wall.
	   CanWalkThru,   // 1 when you can walk trough it.
	   speed,         // How fast the door opens and closes. (cols per frame)
	   delay,         // How many frames to wait before closing the door.
	   bc,            // Upper left corner of the door
	   br,
	   ec,            // Lower right corner of the door
	   er;
 };

// This structure keeps track of all the doors that are opening and closing
// at any particular time.
struct Open_Doors_Info
 {
  char USED;  // Used in finding an empty spot to use. See the code.
  int x,             // Map location of the door.
	  y,
	  delay,         // The door's delay from above.
	  Orig_Txt_Num,  // The texture number of the original (un-opened) door.
	  New_Txt_Num,   // Points to the working copy of the door tex.
	  Pcnt_opened,   // Percent opened.
	  Num_Col_Opnd,  // The number of columns the door is opening.
	  Opening;       // 1=opening, 0=closing.
 };


// This is the basic structure for the sprites.
// It is work-in-progress.
struct Sprite_Type
 {
  char *Frames[8]; // Stores the views (angles object can be viewed from)
 };

struct object
 {
  int  sprite_num, // The sprite array object we're using.
	   x,          // The sprites position in the map (fine coordiates)
	   y,
	   angle,      // The angle the sprite is facing.
	   state;      // he sprites state (To be used later)
 };

// These globals are used for speeding up the tracing and rendering parts.
// These are copied from the col_obj structure above.
int sliver_start,  // Where (y) to begin drawing the slice.
	sliver_column, // Screen column to draw in.
	sliver_scale,  // Height of the sliver (pixels).
	sliver_row,    // Column of the texture to draw.
	sliver_map,    // Which texture to use.

// These are used in the tracing process. They are global for the same reason as above.
	cell_xx,       // These store the coordinates for the horiz. trace.
	cell_yx,       //
	cell_xy,       // These store the coordinates for the vert. trace.
	cell_yy;       //

// These are for the new keyboard ISR.
// They are globals BECAUSE they are used in the ISR.
void interrupt (*OldInt9)(...);
int volatile   KeyScan = 0;
int volatile   KeyState = 0;
char volatile  inProgress = 0;

// These are global because they're used all over, and I wanted to generate them ONLY
// at the beginning of the program. It also saves us on stack usage.
char far       *world;   // pointer to matrix of cells that make up world
char far       *FoorMap; // pointer to the matrix of cells that define the floor textures.
char far       *CeilMap; // pointer to the matrix of cells that define the ceiling textures.
int            Num_Textures = 0; // The number of textures we've loaded.
char far       *WallTextures[70];                    // The 64x64 wall textures, door textures, and visible sprites.
char far       *BkgBuffer;                           // The background picture.
long far       *tan_table = NULL;                    // tangent tables used to compute initial
long far       *inv_tan_table = NULL;                // intersections with ray
long far       *y_step = NULL;                       // x and y steps, used to find intersections
long far       *x_step = NULL;                       // after initial one is found
long far       *cos_table = NULL;                    // used to cacell out fishbowl effect
long far       *sin_table = NULL;                    // Used for movement in different directions.
long far       *inv_cos_table = NULL;                // used to compute distances by calculating
long far       *inv_sin_table = NULL;                // the hypontenuse
long far       *sliver_factor = NULL;                // Scale factor for texture slivers.
long           Floor_Row_Table[151];                 // Pre-calculated row start values.
long far       *Floor_inv_cos_table = NULL;          // The following tables are used in the math for the floor/ceiling.
long far       *Floor_cos_table = NULL;
long far       *Floor_sin_table = NULL;
long far       *Floor_Dx_table = NULL;
long           row[200];                             // Pre-calculates 320*row.
int            oldmode;                              // Original grahics/text mode when the game starts.
char far       *scrnbuf,                             // Pointer to video memory
			   *dblbuf;                              // Pointer to the double buffer ram.
int            WORLD_ROWS = 64;                      // Current size of the map.
int            WORLD_COLUMNS = 64;
long           WORLD_X_SIZE = 4096;                  // Total size of the world (64cells x 64 per cell)
long           WORLD_Y_SIZE = 4096;

struct scan_line_t scan_lines[240];                  // Stores the scan-line info.
struct TextureFlagsTYPE TextureFlags[70];            // Stores the texture flags for each loaded texture.
struct Open_Doors_Info Opening_Doors[10];            // Moving door info.

char   palette[256][3];                              // The current palette.

struct Sprite_Type Sprites[15];  						  // The sprites. These are the bitmaps.
int    Num_Sprites = 0;
struct object Objects[30];									  // The actual objects that use the sprites.
int    Num_Objects = 0;
int    Sprite_Frame[361];                            // Determines frame to display for sprites.

// Graphics functions...
void initscreen(int buf)
 {
  // Initialize the grapphics display, and the double buffer.
  // IF buf = 0, then NO double buffering.
  int om; // The old mode - from the BIOS call.

  dblbuf = NULL;
  scrnbuf=(char far *)MK_FP(0xa000,0); // Make a pointer to screen memory.
  if (buf) // If we want one, allocate a double buffer.
	{
	 dblbuf =(char far *)farmalloc(64000l);
	 if (!dblbuf)
	  {
		printf("Error allocating screen buffer. Exiting. \n");
		exit(1);
	  }
	}
  else    // Otherwise, just skip it.
	dblbuf = scrnbuf;

  // Now we get the old graphics mode, and set the new one.
  asm { mov ah,0xf
	int 0x10
	xor ah,ah
	mov om,ax
	mov ax,0x0013
	int 0x10
		}
  oldmode = om;
  return;
 }

void txtscreen(void)
 {
  // This function resets the VGA to the old mode.
  char om = oldmode&0xff;

  asm { mov ah,0x00
	mov al,om
	int 0x10
	int 0x10
		}
  farfree(dblbuf);
 }

void clearscrn(int c)
 {
  // This function clears te screen to the color "C", by blasting the
  // byte to the double buffer as fast as possible!
  unsigned int Sg, // These are declared here for the asm call.
			 Of,
			 color;

  Sg = FP_SEG(dblbuf); // Get the address of the double buffer.
  Of = FP_OFF(dblbuf);
  color = (c<<8)|c; // Replicate the color value in the upper byte.
  asm { mov ax,Sg   // Now blast it to the buffer.
	mov es,ax
	mov di,Of
	mov ax,color
	mov cx,32000
	rep stosw
      }
 }

void copyscrn (char far *Buffer)
 {
  // This function copies a bitmap from a buffer to the double buffer.
  unsigned int Sg,
			 Of,
	       Bsg,
	       Bof;

  Sg = FP_SEG(dblbuf); // Get the adderess of the double buffer.
  Of = FP_OFF(dblbuf);
  Bsg = FP_SEG(Buffer); // Get the address of the input buffer.
  Bof = FP_OFF(Buffer);

  // Slam it to the doubl buffer.
  asm { push ds
	mov ax,Bof
	mov si,ax
	mov ax,Bsg
	mov ds,ax
	mov ax,Sg
	mov es,ax
	mov di,Of
	mov cx,32000
	rep movsw
	pop ds
		}
 }

void dissolveto(char far *Buffer)
 {
  // This routine does a "dissolve" from the curent screen to that stored
  // in Buffer. It writes directly to the screen.
  unsigned long index,
		x,y,
		offset;

  // Do LOTS of copies at random locations..
  // (We only need the top 1/2 of the screen, IE Dirty animation...)
  for (index=0; index<=500000; index++)
	{
    x = rand()%320;
    y = rand()%100;
	 offset = x+(y<<6)+(y<<8);
	 *(scrnbuf+offset) = *(Buffer+offset);
	}
 }

void swapdblbuf()
 {
  // This routine copies the double buffer to the screen.

  unsigned int Sg,
			 Of;

  Sg = FP_SEG(dblbuf);
  Of = FP_OFF(dblbuf);
  // This snippet waits for the beginning of a verticle blank interval.
//  while(inportb(0x3da) & 0x08) // Wait for any current VBI to end.
//   {};
//  while(!(inportb(0x3da) & 0x08)) // Wait for the beginning of the next one.
//   {};
  // Now we can blast the buffer to the screen.
  asm { push ds
	mov ax,Of
	mov si,ax
	mov ax,Sg
	mov ds,ax
	mov ax,0xa000
	mov es,ax
	xor di,di
	mov cx,32000
	rep movsw
	pop ds
		}
 }

void fadeout(void)
 {
  // This function fades the picture on the screen to black, using the
  // palette.
  //
  int r,g,b,    // The color values
		pl,       // The pallete entry number
      index;    // Loop counter... used as a divisor.

  // Loop through 16 stages of darkening...
  for (index = 1; index <= 16; ++index)
	{
    // And do it for all 256 colors...
	 for (pl = 0; pl < 256; ++pl)
     {
		// Divide the current palette entry by the divisor,
		r = palette[pl][0]/index;
      g = palette[pl][1]/index;
      b = palette[pl][2]/index;
		// And stick it in the VGA
		outportb(0x3c6,0xff);
		outportb(0x3c8,pl);
		outportb(0x3c9,r);
      outportb(0x3c9,g);
      outportb(0x3c9,b);
     }
	 // Without this, the whole thing's too fast...
	 delay(25);
	}
  // Finally, we black it all out.
  for (pl = 0; pl < 256; ++pl)
	{
	 outportb(0x3c6,0xff);
    outportb(0x3c8,pl);
    outportb(0x3c9,0);
	 outportb(0x3c9,0);
	 outportb(0x3c9,0);
   }
 }

void fadein(void)
 {
  //
  // This function fades in from Black to the current palette values.
  //
  int r,g,b, // The color values to plug into the VGA
      pl,    // The index into the palette structure.
		index; // loop counter, used as the divisor...

  // Make sure we're starting out from black.
  for (pl = 0; pl < 256; ++pl)
	{
    outportb(0x3c6,0xff);
    outportb(0x3c8,pl);
	 outportb(0x3c9,0);
	 outportb(0x3c9,0);
    outportb(0x3c9,0);
	}
	// Now, go through all 16 levels of darkness.
  for (index = 16; index >= 1; --index)
   {
    for (pl = 0; pl < 256; ++pl)
     {
		r = palette[pl][0]/index;
      g = palette[pl][1]/index;
      b = palette[pl][2]/index;
      outportb(0x3c6,0xff);
		outportb(0x3c8,pl);
      outportb(0x3c9,r);
		outportb(0x3c9,g);
		outportb(0x3c9,b);
     }
    delay(25);
   }
 }

// New interrupt handler...

void interrupt NewInt9(...)
 {
  register keybyte;
  if (!inProgress)  // Let's not do is twice at the same time...
	{
	 inProgress = 1;
	 KeyScan =inp(0x60);   // Get the scan code.
	 keybyte =inp(0x61);   // Get the byte code.
	 outp(0x61,(keybyte|0x80)); // Clear the flag.
    outp(0x61,keybyte);
    switch(KeyScan) // Now process it...
     {
		case 57:  KeyState |= 0x10; break;  // Pressed the space bar
		case 185: KeyState &= 0xef; break;  // Released the space bar
      case 77:  KeyState |= 0x08; break;  // Pressed left
      case 205: KeyState &= 0xf7; break;
		case 75:  KeyState |= 0x04; break;  // Pressed right
      case 203: KeyState &= 0xfb; break;
		case 72:  KeyState |= 0x02; break;  // Pressed up
		case 200: KeyState &= 0xfd; break;
      case 80:  KeyState |= 0x01; break;  // Pressed down
		case 208: KeyState &= 0xfe; break;
     }
	 inProgress = 0;
	}
  outp(0x20,0x20); // Signal the end of the interrupt.
 }

// Misc. Functions.

void delay2(long time) // time is in seconds.
 {
  // This is a delay that can be cancelled by pressing SPACE or ESCape.
  // It is used during the start-up screens.
  time_t start;

  start= clock();
  while ((KeyScan == 129) || (KeyScan == 57)) // Wait for the keys to clear.
	;
  while ((clock() - start) < time*CLK_TCK) // Start the delay
	{
    if ((KeyScan == 129) || (KeyScan == 57))// if space or ESC, break out.
     break;
	}
 }

// The following functions set up the world...

void free_tables(void)
 {
  // Freee all the tables, if they were allocated.
  int i,j;

  for (i=0; i<Num_Textures; ++i)
	if (WallTextures[i] != NULL)
	 farfree (WallTextures[i]);
  if (tan_table != NULL)
	farfree(tan_table);
  if (inv_tan_table != NULL)
	farfree(inv_tan_table);
  if (y_step != NULL)
	farfree(y_step);
  if (x_step != NULL)
	farfree(x_step);
  if (cos_table != NULL)
	farfree(cos_table);
  if (sin_table != NULL)
	farfree(sin_table);
  if (inv_cos_table != NULL)
	farfree(inv_cos_table);
  if (inv_sin_table != NULL)
	farfree(inv_sin_table);
  if (sliver_factor != NULL)
	farfree(sliver_factor);
  if (Floor_inv_cos_table != NULL)
	farfree(Floor_inv_cos_table);
  if (Floor_cos_table != NULL)
	farfree(Floor_cos_table);
  if (Floor_sin_table != NULL)
	farfree(Floor_sin_table);
  if (Floor_Dx_table != NULL)
	farfree (Floor_Dx_table);
  for (j = 0; j <= Num_Sprites; ++j)
	for (i=0; i < 8; ++i)
	 if (Sprites[j].Frames[i] != NULL)
	  farfree (Sprites[j].Frames[i]);
 }

void HndleAllocError(void)
 {
  // If an allocation error was found, this function is called. It informs us of the
  // error, frees the tables, and quits.
  fprintf(stderr,"Memory allocation error.\n");
  free_tables();
  getch();
  exit(1);
 }

void Build_Tables(void)
 {
  //   Here is were we build all the math tables. This is MUCH faster than
  // using the trig functions. We will also pre-calculate a couple of other
  // things here as well.
  //
  int ang; // The angle (from ANGLE_0 to ANGLE_360)
  float rad_angle; // The angle in radians.

  // Step 1: Allocate the room for the tables. If there's an error, report it
  //         and clean up.
  if (!(tan_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(inv_tan_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(y_step = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(x_step = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(cos_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(sin_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(inv_cos_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(inv_sin_table = (long far *)farmalloc(sizeof(long) * (ANGLE_360+1))))
	HndleAllocError();
  if (!(sliver_factor = (long far *)farmalloc(sizeof(long)*600)))
	HndleAllocError();
  if (!(Floor_inv_cos_table = (long far *)farmalloc(sizeof(long)*(ANGLE_360+1))))
	HndleAllocError();
  if (!(Floor_cos_table = (long far *)farmalloc(sizeof(long)*(ANGLE_360+1))))
	HndleAllocError();
  if (!(Floor_sin_table = (long far *)farmalloc(sizeof(long)*(ANGLE_360+1))))
	HndleAllocError();
  if (!(Floor_Dx_table = (long far *)farmalloc(sizeof(long)*10000L)))
	HndleAllocError();

  // Step 2: Calculate scaling tables, and row tables. 16:16 and 22:10 fixed point
  for (ang=1; ang < 600; ++ang)
	sliver_factor[ang] = (long)(((float)64/ang)*65536l); // Used in scaling the slivers of texture
  for (ang = 150; ang > 76 ; --ang)
	Floor_Row_Table[ang]=(long)((5738.140625*1024.0)/(ang-75.0)); // Distance of floor row, dead ahead center.
  for (ang = 74; ang >=0 ; --ang)
	Floor_Row_Table[ang]=(long)((5738.140625*1024.0)/(75.0-ang)); // Distance of ceiling row, dead ahead center.
  for (ang = 0; ang < 10000; ++ang)
	Floor_Dx_table[ang] = (long)((1024.0 * ang)/239); // Not used yet. Will be for linear interp. of floor/ceil row rendering
  for (ang = 0; ang < 200; ++ang)
	row[ang] = ang*320;              // Pre-calculated beginning of the row in screen memory.
  // Determine the sprite frame to display based on the angle.
  for (ang=0; ang < 361; ++ang)
	{
	 if ((ang > 157) && (ang <= 202)) Sprite_Frame[ang] = 0;
	 if ((ang > 202) && (ang <= 247)) Sprite_Frame[ang] = 1;
	 if ((ang > 247) && (ang <= 292)) Sprite_Frame[ang] = 2;
	 if ((ang > 292) && (ang <= 337)) Sprite_Frame[ang] = 3;
	 if ((ang > 337) && (ang <= 22))  Sprite_Frame[ang] = 4;
	 if ((ang > 22)  && (ang <= 67))  Sprite_Frame[ang] = 5;
	 if ((ang > 67)  && (ang <= 112)) Sprite_Frame[ang] = 6;
	 if ((ang > 112) && (ang <= 157)) Sprite_Frame[ang] = 7;
	}
  // Step 3: Now calculate the trig tables.
  for (ang=ANGLE_0; ang<=ANGLE_360; ang++)
	{
	 // Calculate the angle n radians (for the trig funcions...
	 rad_angle = (3.272e-4)+ang*2*3.141592654/ANGLE_360;
	 // Make the basic trig tables.
	 tan_table[ang] = (long)(tan(rad_angle)*65536L);       // Used to get the y step for a specific angle.
	 inv_tan_table[ang] = (long)(65536L/tan(rad_angle));   // Used to get the x step for a specific angle.
	 Floor_cos_table[ang] = (long) (1024L*cos(rad_angle)); // Used to get the Y position for the angle and distance.
	 Floor_sin_table[ang] = (long) (1024L*sin(rad_angle)); // Used to get the X position for the angle and distance.
	 // Make the X and Y step tables, used while casting the rays.
	 if (ang>=ANGLE_0 && ang<ANGLE_180)
	  {
		y_step[ang] = (tan_table[ang])*CELL_Y_SIZE;
		if (y_step[ang] < 0)
		 y_step[ang] = -y_step[ang];
	  }
	 else
	  {
		y_step[ang] = (tan_table[ang])*CELL_Y_SIZE;
		if (y_step[ang] > 0)
		 y_step[ang] = -y_step[ang];
	  }
	 if (ang>=ANGLE_90 && ang<ANGLE_270)
	  {
		x_step[ang] = -(inv_tan_table[ang])*CELL_X_SIZE;
		if (x_step[ang] > 0)
		 x_step[ang] = -x_step[ang];
	  }
	 else
	  {
		x_step[ang] = inv_tan_table[ang]*CELL_X_SIZE;
		if (x_step[ang] < 0)
		 x_step[ang] = -x_step[ang];
	  }
	 // Back to the basic tables...
	 inv_cos_table[ang] = (long)(8192L/cos(rad_angle)); // Get Y for wall position from angle and distance.
	 inv_sin_table[ang] = (long)(8192L/sin(rad_angle)); // Get X for wall position from andle and distance.
	 sin_table[ang] = (long)(65536L*sin(rad_angle));    // Used for movement. % to move in X and Y for certain angle.
	}
  // These tables arre for scaling and the anti-fisheye stuff.
  for (ang=-ANGLE_30; ang<=ANGLE_30; ang++)
	{
	 rad_angle = (3.272e-4)+ang*2*3.141592654/ANGLE_360;
	 Floor_inv_cos_table[ang+ANGLE_30] = (long)(1024.0/cos(rad_angle)); // Anti-fisheye for floor.
	 cos_table[ang+ANGLE_30] = (long)((5738.1406*256.0)/cos(rad_angle)); // Anti-fisheye and scaling for walls.
	}
 }

void load_bkgd(char *filename, int i)
 {
  // This function loads in a 320x200, 256 color PCX file, and
  // places it into the BkgBuffer.

  unsigned long count,   // the count for the RLE run.
					 index;   // Overall byte count. main FOR loop.
  unsigned char data,    // The data byte read in.
					 r,g,b;   // Color data.
  FILE          *infile; // The file pointer.
  PCX_HEADER    header;  // The PCX header info.

  // Step 1: Allocate the buffer.
  if (BkgBuffer)
	farfree(BkgBuffer);
  if (!(BkgBuffer = (char far *)farmalloc(64000l)))
	{
	 fprintf(stderr,"Error loading background graphic.\n");
	 getch();
	 exit(1);
	}
  // Step 2: Open the file
  infile = fopen(filename,"rb");
  if (!infile)
	{
	 printf("Error loading file '%s', Exiting.\n",filename);
	 farfree(BkgBuffer);
	 getch();
	 exit(1);
	}
  // Step 3: Read in the header, and get it out of the way...
  fread((char *)&header,128,1,infile);
  // Step 4: Read in the bitmap data.
  index = 0;
  while(index < 64000l) // 320x200 = 64000.
	{
    data = getc(infile); // Get the byte.
    if (data >= 192) // If it's an RLE run,
     {
      count = data-192; // Get the count,
		data = getc(infile); // and the data,
      while (count-- > 0) // and blast it out.
		 {
        *(BkgBuffer+index) = (char)data;
		  ++index;
		 }
	  }
	 else // Else if it isn't an RLE,
	  {
      *(BkgBuffer+index) = (char)data; // just write it.
		++index;
	  }
   }
  // Now seek to the palette data, and read that in.
  fseek(infile,-768l,SEEK_END);
  for (index = 0; index < 256; ++index)
	{
	 // Get the data,
	 palette[index][0] = r = getc(infile)>>2;
	 palette[index][1] = g = getc(infile)>>2;
    palette[index][2] = b = getc(infile)>>2;
	 // And set the VGA if requested.
    if (i)
	  {
		outportb(0x3c6,0xff);
      outportb(0x3c8,index);
      outportb(0x3c9,r);
      outportb(0x3c9,g);
		outportb(0x3c9,b);
	  }
   }
  // Step 5: Close the file.
  fclose(infile);
 }

void transpose(char far *Bitmap)
 {
  //
  // This function takes a bitmap and transposes the columns with the rows. This is
  // because the sliver renderer reads across a row of the bitmap, and draws a column.
  //
  register int rows, // These two are loop counters.
               cols;
  char temp[64][64]; // A temporary holding buffer.

  // Copy the bitmap into the temp array, transposed.
  for (rows = 0; rows < 64; ++rows)
   for (cols = 0; cols < 64; ++cols)
	 temp[rows][cols] = Bitmap[64*cols+rows];

  // Now, copy it back...
  memcpy(Bitmap,temp,4096);
 }

void Load_Wall(char *filename,int offset)
 {
  //
  // This function loads in the wall texture map. This will be a 256 color .PCX
  // file. The palette must be the same as that which is loaded in the background
  // The routine also calls Transpose, to order the rows and cols correctly.
  //
  //  offset is the texture number (as specified in the map file.
  //
  unsigned long count,        // The count for RLE runs.
		index;        // Overall byte count (Loop index)
  unsigned char data;         // The data read from the file.
  FILE          *infile;      // The file pointer.
  char          header[129];  // Buffer to store the header info.

  // Decrement the offset since the array starts at 0.
  --offset;
  // Check to see if a texture is already there. If so, then delete it.
  if (WallTextures[offset] != NULL)
   farfree(WallTextures[offset]);
  // Now allocate a new bit of memory for it.
  if (!(WallTextures[offset] = (char far *)farmalloc(4096)))
   {
	 // If there was a problem, quit and complain!
    printf("Error loading texture. \n");
    free_tables();
	 getch();
    exit(1);
	}
  // Open the file.
  infile = fopen(filename,"r");
  if (!infile)
	{
	 // If there was a problem, quit and complain!
    printf("Error loading file %s.. Exiting.\n",filename);
	 free_tables();
    getch();
	 exit(1);
	}
  // Now, we read in the header, and discard it. (We trust the user...)
  fread((char *)&header,128,1,infile);
  // Now let's rean in the PCX data.
  index = 0;
  while(index < 4096)
	{
    data = getc(infile); // Get the data
    if (data >= 192)     // and check for an RLE run.
	  {
		count = data-192;    // If so, then get the count,
		data = getc(infile); // and the color byte,
      while (count-- > 0)  // and blast it to the buffer.
       {
	*(WallTextures[offset]+index) = (char)data;
	++index;
       }
	  }
	 else         // Else, if it isn't an RLE run, just put the byte in the buffer.
	  {
		*(WallTextures[offset]+index) = (char)data;
		++index;
	  }
	}
  fclose (infile);
  // We need to transpose the rows and coumns now, because the renderer
  // traverses a ROW of the texture to draw a COLUMN of the screen (faster).
  transpose(WallTextures[offset]);
 }

char far *Load_Sprite (char *filename)
 {
  // Loads the bitmaps for a sprite object. The files are called [Filename][1-8].pcx
  // where [Filename] is the base filename (up to 7 characters), and [1-8] is a
  // single digit from 1 to 8 that is appended to the filename which represents the
  // frame number (each frame represents a 45 degree view of the object. This name formatting
  // is not done here, but in Allocate_World ( ). This routine just loads in 1 bitmap.
  // The function malloc's space for the bitmap, loads it in, and returns a pointer to it.
  unsigned long count,        // The count for RLE runs.
					 index;        // Overall byte count (Loop index)
  unsigned char data;         // The data read from the file.
  FILE          *infile;      // The file pointer.
  char          header[129];  // Buffer to store the header info.
  char far      *Pointer;     // Points to the sprite data.

  // Allocate a new bit of memory for the sprite.
  if (!(Pointer = (char far *)farmalloc(4096)))
	{
	 // If there was a problem, quit and complain!
	 printf("Error loading Sprite texture. \n");
	 free_tables();
	 getch();
	 exit(1);
	}
  // Open the file.
  infile = fopen(filename,"r");
  if (!infile)
	{
	 // If there was a problem, quit and complain!
	 printf("Error loading file %s.. Exiting.\n",filename);
	 free_tables();
	 getch();
	 exit(1);
	}
  // Now, we read in the header, and discard it. (We trust the user...)
  fread((char *)&header,128,1,infile);
  // Now let's rean in the PCX data.
  index = 0;
  while(index < 4096)
	{
	 data = getc(infile); // Get the data
	 if (data >= 192)     // and check for an RLE run.
	  {
		count = data-192;    // If so, then get the count,
		data = getc(infile); // and the color byte,
		while (count-- > 0)  // and blast it to the buffer.
		 {
		  *(Pointer+index) = (char)data;
		  ++index;
		 }
	  }
	 else         // Else, if it isn't an RLE run, just put the byte in the buffer.
	  {
		*(Pointer+index) = (char)data;
		++index;
	  }
	}
  fclose (infile);
  // We need to transpose the rows and coumns now, because the renderer
  // traverses a ROW of the texture to draw a COLUMN of the screen (faster).
  transpose(Pointer);
  return Pointer;
 }

void Allocate_World(long &initx, long &inity, long &initang)
 {
  //
  //   This routine reads and translates the map file, creates and fills the map arrays,
  //  and loads the textures into memory.
  //
  FILE *fp;          // Filepointer for the map file.
  int  row,          // Row number that we are readng in.
		 column,       // Column number that we are reading in.
		 txtnum,       // The texture to be assigned to the cell.
		 cs,           // Te following is Door info, and has already been described in detail.
		 rs,
		 ce,
		 re,
		 de,
		 tp,
		 spd,
		 i,            // Generic loop index.
		 sp_num,       // The number of the sprite we're loading.
		 sp_txt;
  long sp_initx,     // The sprite's initial X coordinate.
		 sp_inity,     // The sprite's initial Y coordinate.
		 sp_initang;   // The sprite;s initial angle.
  char line[257],    // Temporary buffer to hold the current line of the map file.
		 typ[80],      // This holds the actual data we're reading from the line.
		 ch,           // Temporary holding var. for cell data during translation.
		 sp_basename[12]; // Base filename for the sprite's PCX files.

  // Step 1. Allocate the three maps. 4K each.
  if (!(world = (char *)malloc(4096)))
	{
	 fprintf(stderr,"Memory allocation error.\n");
	 getch();
	 exit(1);
	}
  if (!(FoorMap = (char *)malloc(4096)))
	{
	 fprintf(stderr,"Memory allocation error.\n");
	 getch();
	 exit(1);
	}
  if (!(CeilMap = (char *)malloc(4096)))
	{
	 fprintf(stderr,"Memory allocation error.\n");
	 getch();
	 exit(1);
	}

  // Step 2. Open the map file, and
  if ((fp = fopen("raymap.dat","r")) == NULL)
	{
	 fprintf(stderr,"Error opening world file.\n");
	 free_tables();
	 getch();
	 exit(1);
	}
  // Read in world data.
  fgets(line,256,fp);
  while (!feof(fp))
	{
	 memset(typ,0,80);
	 sscanf(line,"%s",typ); // This step cleans up much garbage that can be present.
	 // Step 2a. Load the player's initial position and orientation
	 if (!strcmpi(typ,"P"))
	  {
		sscanf(line,"%*s %ld %ld %ld",&initx, &inity, &initang);
		// Translate into the fine coordinate system.
		initx *= 64;
		inity *= 64;
		initang *= 4;
	  }
	 // 2b. Load in a sprite definition..
	 else if (!strcmpi(typ,"S"))
	  {
		// [Number] [Base Filename]
		sscanf(line,"%*s %d %7s",&sp_num, sp_basename);
		for (i=1; i<= 8; ++i)
		 {
		  sprintf(typ,"%s%d.pcx",sp_basename,i);
		  Sprites[sp_num].Frames[i-1] = Load_Sprite(typ);
		 }
		if (sp_num > Num_Sprites)
		 Num_Sprites = sp_num;
	  }
	 // 2c.  Load in an object definition.
	 else if (!strcmpi(typ,"O"))
	  {
		// [Number] [Initial X] [Initial Y] [Initial Angle]
		sscanf(line,"%*s %d %d %d %d %d",&sp_num, &sp_txt, &sp_initx, &sp_inity, &sp_initang );
		Objects[sp_num].sprite_num = sp_txt;
		Objects[sp_num].x = sp_initx*64+32;
		Objects[sp_num].y = sp_inity*64+32;
		Objects[sp_num].angle = sp_initang * 4;
		Objects[sp_num].state = 0;
		if (sp_num > Num_Objects)
		 Num_Objects = sp_num;
	  }
	 // Step 2d. Load a normal texture definition.
	 else if (!strcmpi(typ,"T"))
	  {
		sscanf(line,"%*s %d %s",&txtnum,typ); // [NUMBER] [Filename]
		// Track how many we've loaded.
		if (txtnum > Num_Textures)
		 Num_Textures = txtnum;
		// We need to clear these, since this is normal texture.
		TextureFlags[txtnum].IsTransparent = 0;
		TextureFlags[txtnum].CanWalkThru = 0;
		TextureFlags[txtnum].IsDoor = 0;
		// Load the bitmap.
		Load_Wall(typ,txtnum);
	  }
	 // Step 2e. Load a definition for a TRANSPARENT texture.
	 else if (!strcmpi(typ,"X"))
	  {
		sscanf(line,"%*s %d %s",&txtnum,typ);
		// This partis the same as above,
		if (txtnum > Num_Textures)
		 Num_Textures = txtnum;
		// except that the transparnt flag is set.
		TextureFlags[txtnum].IsTransparent = 1;
		TextureFlags[txtnum].CanWalkThru = 0;
		TextureFlags[txtnum].IsDoor = 0;
		Load_Wall(typ,txtnum);
	  }
	 // Step 2f. Load a door definition.
	 else if (!strcmpi(typ,"D"))
	  {
		// Door format is: D [Number] [Filename] [Col Start] [Row Start] [Col End] [Row End] [Speed] [Delay] [transparent]
		sscanf(line,"%*s %d %s %d %d %d %d %d %d %d",&txtnum,typ,&cs,&rs,&ce,&re,&spd,&de, &tp);
		// Still, keep trck of the number,
		if (txtnum > Num_Textures)
		 Num_Textures = txtnum;
		// Since this is a door, we have lots more to set...
		TextureFlags[txtnum].IsTransparent = tp;
		TextureFlags[txtnum].CanWalkThru = 0;
		TextureFlags[txtnum].IsDoor = 1;
		TextureFlags[txtnum].bc = cs;
		TextureFlags[txtnum].br = rs;
		TextureFlags[txtnum].ec = ce;
		TextureFlags[txtnum].er = re;
		TextureFlags[txtnum].delay = de;
		TextureFlags[txtnum].speed = spd;
		Load_Wall(typ,txtnum);
	  }
	 // Load in the actual wall map data.
	 else if (!strcmpi(typ,"M"))
	  {
		sscanf(line,"%*s %d %d",&WORLD_ROWS,&WORLD_COLUMNS);
		// Make sure the map size is OK. We set a maximum value above...
		if ((WORLD_ROWS < 10) || (WORLD_ROWS > MAX_WORLD_ROWS) || (WORLD_COLUMNS < 0) || (WORLD_COLUMNS > MAX_WORLD_COLS))
		 {
		  fprintf(stderr,"Invalid map size.\n");
		  free_tables();
		  getch();
		  exit(1);
		 }
		// Convert to fine coordinates.
		WORLD_X_SIZE = 64l*WORLD_COLUMNS;
		WORLD_Y_SIZE = 64l*WORLD_ROWS;
		// Now read in the cell data.
		for (row=0; row<WORLD_ROWS; row++) // For all rows...
		 {
	for (column=0; column<WORLD_COLUMNS; column++) // For all columns...
	 {
	  while((ch = getc(fp))==10) // filter out CR
			  {}
	  if (ch == ' ')  // If blank, default to zero (No wall)
		ch=0;
	  else if (ch == '0') // zero is ten
		ch = 10;
	  else if ((ch > '0') && (ch <= '9')) // 1 - 9 is 1 - 9
		ch = ch - '0';
	  else if ((ch >= 'A') && (ch <= 'Z')) // A - Z is 11 - 35
		ch = ch - 'A' + 11;
	  *(world+(row<<6)+column) = ch; // Place the converted value in the map.
	 }
		 }
	  }
	else if (!strcmpi(typ,"F")) // Load in the Floor map.
	 {
	  for (row=0; row<WORLD_ROWS; row++) // The floor must have the same dimensions as the walls.
		{
		 for (column=0; column<WORLD_COLUMNS; column++)
	 {
	  while((ch = getc(fp))==10) // filter out CR
		{}
	  if (ch == ' ') // Spaces map out to 'no texture' (default Bkg color)
		ch=0;
	  else if (ch == '0')   // 0 is 10
		ch = 10;
	  else if ((ch > '0') && (ch <= '9'))  // 1-9 is 1-9
		ch = ch - '0';
	  else if ((ch >= 'A') && (ch <= 'Z')) // A-Z is 11-35
		ch = ch - 'A' + 11;
	  *(FoorMap+(row<<6)+column) = ch;  // Add it to the floor map.
	 }
		 }
	  }
	 else if (!strcmpi(typ,"C")) // Load in the ceiling map. It must be the same size as the wall map.
	  {
		for (row=0; row<WORLD_ROWS; row++) // For every row
		 {
	for (column=0; column<WORLD_COLUMNS; column++) // For every column
	 {
	  while((ch = getc(fp))==10) // read in a char, and filter out CR
		{}
	  if (ch == ' ') // Space maps to 'no texture', default Bkg color.
		ch=0;
	  else if (ch == '0') // 0 is 10
		ch = 10;
	  else if ((ch > '0') && (ch <= '9')) // 1-9 is 1-9
		ch = ch - '0';
	  else if ((ch >= 'A') && (ch <= 'Z')) // A-Z is 11-35
			  ch = ch - 'A' + 11;
	  *(CeilMap+(row<<6)+column) = ch; // Add it to the ceiling map.
	 }
		 }
	  }
	 else if (!strcmpi(typ,"#END")) // This quits... along with EOF.
	  break;
	 fgets(line,256,fp); // Get the next line here.. (We test for EOF at the beginning..)
	}
  fclose(fp); // Close the file,
  return;     // and return.
 }

// The following functions render the world...

void draw_sliver_trans(void)
 {
  //
  // This function draws a sliver of wall texture. It reads from a texture (sliver_map) row
  // (sliver_row) and draws from the top down in the column (sliver_column). It is scaled
  // to (sliver_scale) and is centered on the screen vertically by the calling routing,
  // starting at (sliver_start). All bytes that are 0 (zero) are transparent.
  // Scaling is done using a pre-calculated array of fixed point fractions. These are added
  // to Tcount each time through the loop, and the whole part is used as the index into the
  // texture. (i.e. if Tcount = 0.5, then the texture is now 2x as large.)
  //
  extern int sliver_column; // Screen column to draw in.
  extern int sliver_start;  // Screen row to start drawing in.
  extern int sliver_scale;  // how many pixels (rows) long the sliver is to be.
  extern int sliver_row;    // Which row from the texture to read.
  extern int sliver_map;    // Which texture to use.
  register int  y;          // Current screen row we're rendering
  register long Scale,      // Amount (fixed Pt) to increment Tcount each row.
		Tcount,     // The offset into the texture row (Fixed point)
		sliver_end; // The last screen row to render
  char far      *base,      // Beginning of the current texture row in memory.
		*bufptr;    // Pointer to the beginning of screen memory for this sliver.
  register char pixl;       // The pixel value we're going to plot.

  // Reset Tcount
  Tcount = 0;
  // Preload the scaleing factor, and the pointer to the texture. (Faster this way).
  Scale = sliver_factor[sliver_scale];
  base = WallTextures[sliver_map]+(sliver_row<<6);
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
  bufptr = dblbuf+*(row+sliver_start)+sliver_column;
  // Now, for the entire sliver run,
  for (y = sliver_start; y <= sliver_end ; ++y)
   {
    // Get the pixel value from the texture, using the whole part of Tcount as the index.
	 pixl = *(base+(long)(Tcount>>16));
	 // If the value isn't zero, then draw it.
    if ((pixl != 0))
     *bufptr = pixl;
	 // And increment Tcount and the double buffer pointer.
	 Tcount += Scale;
    bufptr += 320;
   }
 }

void draw_sliver(void)
 {
  //
  // This function draws a sliver of wall texture. It reads from a texture (sliver_map) row
  // (sliver_row) and draws from the top down in the column (sliver_column). It is scaled
  // to (sliver_scale) and is centered on the screen vertically by the calling routing,
  // starting at (sliver_start).
  // Scaling is done using a pre-calculated array of fixed point fractions. These are added
  // to Tcount each time through the loop, and the whole part is used as the index into the
  // texture. (i.e. if Tcount = 0.5, then the texture is now 2x as large.)
  //
  extern int sliver_column; // Screen column to draw in.
  extern int sliver_start;  // Screen row to start drawing in.
  extern int sliver_scale;  // how many pixels (rows) long the sliver is to be.
  extern int sliver_row;    // Which row from the texture to read.
  extern int sliver_map;    // Which texture to use.
  register int  y;          // Current screen row we're rendering
  register long Scale,      // Amount (fixed Pt) to increment Tcount each row.
		Tcount,     // The offset into the texture row (Fixed point)
		sliver_end; // The last screen row to render
  char far      *base,      // Beginning of the current texture row in memory.
		*bufptr;    // Pointer to the beginning of screen memory for this sliver.

  // Reset Tcount for this run.
  Tcount = 0;
  // Preload the scaleing factor, and the pointer to the texture. (Faster this way).
  Scale = sliver_factor[sliver_scale];
  base = WallTextures[sliver_map]+(sliver_row<<6);
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
  bufptr = dblbuf+*(row+sliver_start)+sliver_column;
  // Now, for each screen row,
  for (y = sliver_start; y <= sliver_end; y ++)
	{
    // Get the byte from the texture and render it.
	 *bufptr = *(base+(long)(Tcount>>16));
    // Update Tcount and bufptr.
	 Tcount += Scale;
    bufptr += 320;
	}
 }

int Cast_X_Ray(long x, long y, long view_angle, long &yi_save, long &x_save)
 {
  //
  //    This is the heart of the program. It casts a mathematical ray from the
  // players position along the view_angle. The ray stops when it hits a wall,
  // saving the x and y coordinates in yi_save and x_save, and returns the type
  // of wall hit.
  //    This routine finds the HORIZONTAL cells along the ray. (SEE TGPG).
  //
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
	 x_bound = (x & 0xffc0)+CELL_X_SIZE;
	 // This is the amount to add to x_bound for each step.
    x_delta = CELL_X_SIZE;
	 // Amount to compensate for the "1-off" error.
	 next_x_cell = 0;
   }
  // Else, if it's going to the LEFT.
  else
	{
    // Determine where the current cell edge is (in fine coords.)
    x_bound = (x & 0xffc0);
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
	 cell_xx = ((x_bound+next_x_cell)>>6);
	 cell_yx = ((long)(yi>>16)) >> 6;
    // Check to see if we hit a wall or not. (A wall is >= 1).
	 if ((hit_type = *(world+(cell_yx<<6)+cell_xx))!=0)
	  {
      // Save the fine coordinates sso we can return them...
      yi_save = (long)((yi+32768L)>>16);
		x_save = x_bound;
		// and return, with the texture number for the map cell.
      return hit_type;
     }
	 // Otherwise, if we havn't hit a wall, increment to the next boundary intersection,
    // and go again.
    yi += ystep;
	 x_bound += x_delta;
	}
 }

int Cast_Y_Ray(long x, long y, long view_angle, long &xi_save, long &y_save)
 {
  //
  //    This is the heart of the program. It casts a mathematical ray from the
  // players position along the view_angle. The ray stops when it hits a wall,
  // saving the x and y coordinates in xi_save and y_save, and returns the type
  // of wall hit.
  //    This routine finds the VERTICAL cells along the ray. (see TGPG)
  //

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
	 y_bound = CELL_Y_SIZE + y & 0xffc0;
	 // Set y_delta. This is added to y_bound each time through the loop.
	 y_delta = CELL_Y_SIZE;
	 // and this fixes the "1 off" error in getting the map cell location (array x&y).
	 next_y_cell = 0;
	}
  // Else, if we are looking DOWN...
  else
	{
	 // Calculate the lower cell boundary.
	 y_bound = y & 0xffc0;
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
    cell_yy = (y_bound+next_y_cell) >> 6;
	 cell_xy = ((int)(xi>>16)) >> 6;
    // And determine if there is a texture in that cell.
    if ((hit_type = *(world+(cell_yy<<6)+cell_xy))!=0)
	  {
		// If there is, let's save the location (fine coords),
      xi_save = (long)((xi+32768L)>>16);
		y_save = y_bound;
		// and indicate what type of wall we hit.
		return hit_type;
	  }
	 // If we didn't hit a wall, we increment the intercept variables, and check again.
	 xi += xstep;
	 y_bound += y_delta;
   }
 }

void Texture_FloorCeil(long x, long y, long view_angle)
 {
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
  //    The next step would be to cast for the edges in each row, and linearly interpolate
  // across the row. I am currently experimenting with that, but I'm having some trouble.
  //
  register int   col,         // The screen column currently being processed.
				ROW,         // The screen row currently being processed.
					size;        // The row on the screen where the floor ends and the wall begins.
  register long  distance,    // How far away the point is from the eye.
                 MapOff,      // Offset into the floor map of the current pixel. (see below)
					  TxtOff,      // Offset into the texture map of the current pixel. (see below)
		 inv_cost;    // Holds the inverse cosine. (22:10) It doesn't change by row, so we only get it once.
  long           xv,          // X coordinate of the screen pixel (ROW,COL) in texture space.
		 yv,          // Y coordinate of the screen pixel (ROW,COL) in texture space.
		 angle,       // the angle (absolute) the ray for the column is pointing.
		 sintab,      // The sine value for the current angle. (22:10 fixed)
		 costab;      // The cosine value for the current angle. (22:10 fixed)
  struct col_obj *line;       // This saves us a lot in array dereferencing! only have to do it once.

  // For each column in the viewport (ANGLE_0 to ANGLE_60)
  for (col = 0; col < 240; ++col)
   {
    // Calculate the absolute angle of the ray. Since col=[0,240), col-120 = [-ANGLE_30, ANGLE_30)
	 angle = view_angle+(col-120);
    // Check the bounds of the angle, and make it loop back. (mod with ANGLE_360).
    if (angle < ANGLE_0)
     angle += ANGLE_360;
	 if (angle >= ANGLE_360)
	  angle -= ANGLE_360;
	 // Get the line structure for the current column, farthest wall.
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
	 // Now, we cast from the bottom, up to the beginning of the farthest wall.
    for (ROW = 149; ROW >= size; --ROW)
	  {
		// Get the distance of the floor pixel for this row, dead ahead, and then rotate it out
		// to the proper angle. (the inv_cost).
		distance =((*(Floor_Row_Table+ROW)*inv_cost)>>10);
		// Figure out the x and y from trig, (hyp*sin=y, hyp*cos=x).
		yv = (long)((distance*sintab)>>20)+y;
      xv = (long)((distance*costab)>>20)+x;
      // We pre-calculate these to save time. Calculate once, use twice...
		MapOff = (((int)yv>>6)<<6)+((int)xv>>6);
		TxtOff = (((yv&63)<<6)+(xv&63));
      // Now we draw the ceiling and floor at the same time.
		*(dblbuf+*(row+ROW)+col) = *(WallTextures[*(FoorMap+MapOff)-1]+TxtOff);
		*(dblbuf+*(row+149-ROW)+col) = *(WallTextures[*(CeilMap+MapOff)-1]+TxtOff);
     }
	 // Now we find out where the ceiling really ends. It isn't always even with the floor.
	 size = line->top;
	 // And we do the above rendering again. This time, just on the ceiling.
    for (ROW = 148-ROW; ROW <= size; ++ROW)
	  {
		distance =((*(Floor_Row_Table+ROW)*inv_cost)>>10);
      yv = (long)((distance*sintab)>>20)+y;
		xv = (long)((distance*costab)>>20)+x;
		*(dblbuf+*(row+ROW)+col) = *(WallTextures[*(CeilMap+(((int)yv>>6)<<6)+((int)xv>>6))-1]+(((yv&63)<<6)+(xv&63)));
	  }
	}
 }

void process_sprites(long x, long y, long view_angle)
 {
  //
  //  This routine WILL handle the rendering calculations of the sprites. It is work
  // in progress.
  //
  int  Current_Sprite, // The current object we're rendering.
		 Sprite_Num;     // The sprite used for the current object.
  long ang,       // The angle the sprite makes with the view_angle.
		 ang_clipped, // The relative angle, clipped to +- ANGLE_30.
		 ang_saved, // Saved copy of the angle. Used in scaling and fisheye fix.
		 dist,      // The distance from the viewer.
		 size,      // The size of the sprite in pixels (horiz and vert).
		 left,      // The starting column of the sprite.
		 right,     // The ending column of the sprite.
		 sx,sy,     // The camera coordinates of the sprite.
		 tx,ty,     // Temporary holding for the coords. of the sprite during the transform.
		 VA_Neg,    // The negative of the player's view angle.
		 Ref_ang,   // The angle we are viewing the sprite from.
		 Scale_Factor; // Fixed point value used in scaling the slices.

  long  Frame,     // The frame of the sprite we are going to display.
		 sliver,    // The column of the texture to draw for the scan line.
		 col,       // The scan line (col) we are in.
		 Col_Depth, // The number of objects in the scan line buffer for this column.
		 Indx;      // Loop index used to insert sprite sliver in scan line buffer.

  Sprite_Num = 50; // We start adding sprites to the global WallTextures at 51.

  // Loop through all the world's objects.
  for (Current_Sprite = 0; Current_Sprite <= Num_Objects; ++Current_Sprite)
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
	 ang_clipped = ang_saved = ang = ((sy << 7) + (sy << 6) + (sy << 5) + (sy << 1))/sx;
	 if (ang < ANGLE_0) ang += ANGLE_360;
	 // Step 5 Cull the sprite if it's too far off the side of the screen
	 if ((ang_saved < -ANGLE_45) || (ang_saved > ANGLE_45))
	  continue;
	 // Step 6 Get the distance, and size of the sprite (22:10 Fixed)
	 dist = sx * inv_cos_table[ang] >> 13; //  Integer Result (Table = 19:13)
	 // Step 6a. If it's too close, we can't draw it.
	 if (dist < 5) continue;

	 // Step 6b. Clip the angle to +/- ANGLE_30. The cos_table only goes that far.
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
	 TextureFlags[Sprite_Num+1].IsTransparent = 1;
	 TextureFlags[Sprite_Num+1].IsDoor = 0;

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
			  scan_lines[col].num_objs++;
			  break;
			 }
		  }
	  }
	}
 }

void Render_View(long x,long y,long view_angle)
 {
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
  //   When all the setup is done, we bwgin drawing the viewport. We call a function to cast and
  // draw the floors and ceilings, and then we loop through the column buffer, sending each level's
  // texture and scaling info to the draw_sliver and draw_sliver_trans in reverse order (back to front).
  //
  extern int sliver_start;    // These are the Sliver renderer globals. They are global for
  extern int sliver_column;   // SPEED considerations. They have been described quite enough so far.
  extern int sliver_scale;
  extern int sliver_row;
  extern int sliver_map;

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
		 dist_x = 1e8;
      // Otherwise, let's cast some X rays...
		else
       {
		  // First, we cast the ray.
		  x_hit_type = Cast_X_Ray(x,y,view_angle,yi,xs);
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
      dist_y=1e8;
     // Else, let's cast some Y rays.
     else
      {
		 // First, we cast the ray.
		 y_hit_type = Cast_Y_Ray(x,y,view_angle,xi,ys);
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
       line->row = ((int)yi)&63;
       // and it's position in the textures array.
		 line->texnum = x_hit_type-1;
		 // Center the sliver about the middle of the viewport
       line->top = 75 - (line->scale>>1);
       // Store the column number
		 line->col = ray;
       // and it's distance
		 line->dist = (int)dist_x;
       // and then increment the count.
       scan_lines[ray].num_objs++;

       // If the texture was a transparent one, we have a little more to do...
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
       line->row = ((int)xi)&63;
       line->texnum = y_hit_type-1;
       line->top = 75 - (line->scale>>1);
       line->col = ray;
       line->dist = (int)dist_y;
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

  //
  //    We will call the sprite rendering routines here.
  // They will calculate the scaling for the visible sprites,
  // where they are located, and then sort them into the
  // scan_lines array. The renderer will then draw them with
  // the walls.
  //

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
	 for (indx = s_line->num_objs-1; indx >= 0; --indx)
	  {
		// Now put the numbers into the globals.
		line = &(s_line->line[indx]);
		sliver_start = line->top;
		sliver_column = line->col;
		sliver_scale = line->scale;
		sliver_row = line->row;
		sliver_map = line->texnum;
      // And call the rendering routines.
      if (TextureFlags[sliver_map+1].IsTransparent == 1)
       draw_sliver_trans();
      else
       draw_sliver();
     }
   }
 }


// The last few functions perform the game logic...

void start_door(int x, int y)
 {
  //
  //  This routine is called each time the player tries to open a door. It finds an empty
  // spot in the Opening_Doors struct to insert necessary information, Inserts the info,
  // allocates a new spot for the temporary texture info (which is to animated), and sets
  // it's flags.
  //
  int i;                                  // Generic loop index variable.
  register int TxtNum;                    // Texture number of the original door.
  register struct Open_Doors_Info * Door; // Pointer to the used entry of Opening_Doors[].

  // Check to see if we are ALREADY opening the current door.
  for (i=0; i<10; ++i)
	if ((Opening_Doors[i].USED != 0) && (Opening_Doors[i].x == x) && (Opening_Doors[i].y == y))
    return;
  // Now, find an unused entry in the array.
  for (i=0; i<10; ++i)
   if (Opening_Doors[i].USED == 0)
    break;
  // If the array is full, we can't open it. Return adn make the user wait...
  if (i >= 10)
	return;
  // Get the address of the empty spot. Again, this saves time later on.
  Door = &Opening_Doors[i];
  // Get the map texture number for the original door.
  TxtNum = *(world+y*64+x);

  // Now, we start to fill in the Door info.
  Door->USED = 1;                           // This one is now used.
  Door->x = x;                              // It is located at these map coords.
  Door->y = y;
  Door->Orig_Txt_Num = TxtNum-1;            // And uses this texture.
  Door->New_Txt_Num = 40+i;                 // We're putting teh animated texture here
  Door->Pcnt_opened = 0;                    // It's not open yet.
  Door->Num_Col_Opnd = 0;
  Door->Opening = 1;                        // But it's going to be.
  Door->delay = TextureFlags[TxtNum].delay; // Set the door's delay value.

  // OK. Now we make a new copy of the door.
  if (!(WallTextures[40+i] = (char far *)farmalloc(4096)))
	{
    // If there was an error allocating, I'd like to know it.
    printf("Error opening door. \n");
    Opening_Doors[i].USED = 0;
    return;
   }
  // Copy the texture over.
  _fmemcpy(WallTextures[40+i],WallTextures[TxtNum-1],4096);

 // Now, copy the textures parameters over...
 TextureFlags[i+41].IsTransparent = 1;               // It's definately transparent now.
 TextureFlags[i+41].CanWalkThru = 0;                 // Can't go through it yet.
 TextureFlags[i+41].IsDoor = 1;                      // It's a door for sure.
 TextureFlags[i+41].bc = TextureFlags[TxtNum].bc;    // These define the rectangle to animate.
 TextureFlags[i+41].br = TextureFlags[TxtNum].br;
 TextureFlags[i+41].ec = TextureFlags[TxtNum].ec;
 TextureFlags[i+41].er = TextureFlags[TxtNum].er;
 TextureFlags[i+41].speed = TextureFlags[TxtNum].speed; // This is the amt. to move the door each frame.

 // Now we place the new texture information into the map.
 *(world+y*64+x) = 41+i;
 }

void open_doors()
 {
  //
  //   This function loops through the Opening_Doors aray, and opens each active door a
  // little bit more, based on it's speed. When 90% opened, it sets the "CanWalkThru"
  // flag. When 100% opened, it clears the Opening flag.
  //
  int i,          // Outer loop index variable.
      x,          // Loop variable for the animation routine.
      bc,         // These define the rectangle that is to be animated in the texture.
      br,
      ec,
      er,
      spd;        // This is the amount to move the animated part of the texture.
  char far *base; // This points to the door's texture.
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
     // Get a pointer to the bitmap.
     base = WallTextures[Opening_Doors[i].New_Txt_Num];
     // and it's speed.
	  spd = TextureFlags[Opening_Doors[i].New_Txt_Num+1].speed;

     // Move bitmap of door area over by 1 column.
	  for (x=ec; x >= bc+spd; --x)
     _fmemcpy(base+(x*64l)+br,base+br+((x-spd)*64l),er-br+1);
     // Clear out the edge, so we can see through it.
     for ( ; x>= bc; --x)
      _fmemset(base+(x*64l)+br,0,er-br);

      // Update the percent opened.
      Opening_Doors[i].Pcnt_opened += ((100*spd)/(ec-bc));
      // If it's 90% opened, let people walk through it.
      if (Opening_Doors[i].Pcnt_opened >= 90)
       TextUsed->CanWalkThru = 1;
      // If it's 100% open, mark it as closing.
      if (Opening_Doors[i].Pcnt_opened >= 100)
       Opening_Doors[i].Opening = 0;
     }
 }

void close_doors()
 {
  //
  //    This function closes all Active doors that are marked with "Opening=0". It copies
  //  part of the original bitmap back into the animated texture, and updates it's state.
  //  When the Percent_Opened is less than 90% it clears the CanWalkThru flag, which keeps
  //  anyone from ealking through the texture anymore. When it reaches 0%, the original door
  //  is put back into the map, and the animated texture space is deleted.
  //
  int i,                   // Outer loop index variable.
      x,                   // Bitmap copy loop index variable.
      bc,                  // These define the rectangle to be animated on the texture.
		br,
      ec,
      er,
      spd;                 // This is how much to shift the door each time.
  char far *base,          // Pointer to the animated texture.
       far *oldbase;       // Pointer to the original texture.
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
       spd = TextUsed->speed;
       base = WallTextures[Opening_Doors[i].New_Txt_Num];
       oldbase = WallTextures[Opening_Doors[i].Orig_Txt_Num];

		 // Then we copy part of the bitmap of the old door to the new door.
       // The part is defined bu Num_Col_Opnd.
       for (x = Opening_Doors[i].Num_Col_Opnd; x >= 0; --x)
        memcpy(base+(ec-Opening_Doors[i].Num_Col_Opnd+x)*64l+br,oldbase+(x+bc)*64l+br,er-br);
       // We update the size of teh "copy wndow".
       Opening_Doors[i].Num_Col_Opnd += spd;
       // and clip it to a maximum amount (the size of the rectangle)
       if (Opening_Doors[i].Num_Col_Opnd > (ec-bc))
	Opening_Doors[i].Num_Col_Opnd = ec-bc;
       // And we update the percent opened.
       Opening_Doors[i].Pcnt_opened -= ((100 * spd)/(ec-bc));
       // If we've closed enough, no one can walk through...
       if (Opening_Doors[i].Pcnt_opened < 90)
        TextureFlags[Opening_Doors[i].New_Txt_Num+1].CanWalkThru = 0;
       // If we're completely closed, put the old door back.
		 if (Opening_Doors[i].Pcnt_opened <= 0)
	{
	 farfree(base);
	 Opening_Doors[i].USED = 0; // Mark this array entry as available...
	 *(world+Opening_Doors[i].y*64+Opening_Doors[i].x) = Opening_Doors[i].Orig_Txt_Num+1;
	}
      }
    }
 }

void main(void)
 {
  //
  //  THIS is the function that drives it all. It begins by setting up the tables, the screen
  // the new keybard handler, loading the map, displaying the opening screens, processing user
  // input, checking the doors, collisions, etc. When done, it displays the closing screen, and
  // reports on the frames per seecond.

  int    done=0,               // This flag, when set, causes the outer While loop to terminate.
			i;                    // Generic loop index. Used for clearing some tables.
  long   x,                    // The players X position, in fine coordinates.
			y,                    // The players Y position, in fine coordinates.
			view_angle,           // The angle which the player is facing.
			view_angle_90,        // The angle the player is facing + 90 degreess. (Used for collision & door processing)
			x_cell,               // The map cell COL the player is in. (Used for collision & door processing)
			y_cell,               // The map cell ROW the player is in.
			x_sub_cell,           // The fine coordinate inside the cell (0-63) of the player.
			y_sub_cell,           // The fine coordinate inside the cell (0-63) of the player.
			dx,                   // The amount to move the player in X this round.
			dy;                   // The amount to move the player in Y this round.
  float  fps;                  // Calculated frames per second.
  time_t initl;                // The time the game started. (Used in calculating FPS).
  unsigned long frames = 0,    // The cumulative number of frames displayed this game.
					 InitMem = 0,
					 MemLeft = 0;

	// OK, First thing we're going to do is clear out the palette table.
	InitMem = farcoreleft();
   for (i = 0; i < 256; ++i)
    {
     palette[i][0] = 0;
     palette[i][1] = 0;
     palette[i][2] = 0;
	 }
   // Then set up the WallTextures table.
   for (i=0; i < 30; ++i)
    WallTextures[i] = NULL;
   // And clean up the OpeningDoors table.
   for (i=0; i<10; ++i)
    Opening_Doors[i].USED = 0;

	// Now, initialize the screen, and set the proper graphics mode.
   initscreen(1);
   // Set up the new keyboard handler, saving the address of the old one for later.
   OldInt9 = _dos_getvect(0x09);
   _dos_setvect(0x09,NewInt9);
   // Fade the screen to black.
   fadeout();
   // And load in the first full screen graphic.
	load_bkgd("open0.PCX",0);
   // Copy it to the double buffer,
	copyscrn(BkgBuffer);
   // Move that to the screen,
   swapdblbuf();
	// And fade it in.
   fadein();
	// Now we wait 2 seconds. Esc and Space will cancel the delay.
   delay2 (2);
   // Do the same with the second picture,
   load_bkgd("open1.PCX",0);
   dissolveto(BkgBuffer);
   delay2 (7);
	// And the third.
   load_bkgd("open2.PCX",0);
   dissolveto(BkgBuffer);
   delay2 (7);
   fadeout();
   // Now we show the main title.
   load_bkgd("TITLE.PCX",0);
   copyscrn(BkgBuffer);
	swapdblbuf();
   fadein();
   delay2 (5);
   fadeout();
   // temporarilly restore the OLD keyboard handler. Why? because we can die in Build_Tables
   // and Allocate_World.
	_dos_setvect(0x09,OldInt9);
   // make all the trig and support tables.
   Build_Tables();
   // Load in the map, textures, etc. and set the player's initital position.
	Allocate_World(x,y,view_angle);
   // Move him to the middle of the map,
   x += 32;
	y += 32;
   // and calculate the cell and sub-cell values.
	x_cell = x>>6;
   y_cell = y>>6;
   x_sub_cell = x & 63;
   y_sub_cell = y & 63;
   // Load the play screen background,
   load_bkgd("BKGD.PCX",0);
	// Copy it to the double buffer,
   copyscrn(BkgBuffer);
   // Render the 1st view, on top of the background,
   Render_View(x,y,view_angle);
   // Dump it to the screen,
   swapdblbuf();
   // And fade in..........  WE START....
   fadein();
	// Give us back our good kwyboard handler,
   OldInt9 = _dos_getvect(0x09);
   _dos_setvect(0x09,NewInt9);
   // and start counting up the Frames per second...
	frames = 0;
   initl = clock();

   // This is the MAIN GAME LOOP. EVERYTHING IN THE GAME HAPPENS HERE.
   while(!done)
    {
	  // Step 1. Process the doors.
     open_doors(); // Open all opening doors a little more.
     close_doors(); // close all closing doors a little more.

     // Step 2. Check the keyboard for an ESC key (Quit).
	  if (KeyScan == 129)
      done = 1;

     // Step 3. Check to see if the player wants to open a door. If so, then process it.
     if (KeyState & 0x10) // Check for opening a door...
      {
		 // If the player is facing to the WEST
       if ((view_angle >  ANGLE_330) || (view_angle < ANGLE_30))
        {
         // If it really is a door right next to him (1 cell away),
	 if (TextureFlags[*(world+(y_cell<<6)+x_cell+1)].IsDoor)
          // Then start it opening.
	  start_door(x_cell+1,y_cell);
        }
		 // Else see if it's to to the north.
       else if ((view_angle >  ANGLE_60) && (view_angle < ANGLE_120))
		  {
	 if (TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].IsDoor)
	  start_door(x_cell,y_cell+1);
        }
       // Else, see if it's to the west.
       else if ((view_angle >  ANGLE_150) && (view_angle < ANGLE_210))
        {
         if (TextureFlags[*(world+(y_cell<<6)+(x_cell-1))].IsDoor)
	  start_door(x_cell-1,y_cell);
        }
       // Else, see if it's to the south.
		 else if ((view_angle >  ANGLE_240) && (view_angle < ANGLE_300))
        {
			 if (TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].IsDoor)
	  start_door(x_cell,y_cell-1);
        }
      }

     // Step 4. Check for movement. The lower nibble of KeyState is for movement ONLY.
	  if (KeyState & 0x0f)
      {
       // Reset dx and dy.
       dx=dy=0;
       // If the player is pressing the Right arrow,
       if (KeyState & 4)
        {
         // Decrement the view angle by 6 degrees, wrap at zero.
	 if ((view_angle-=ANGLE_6)<ANGLE_0)
	  view_angle+=ANGLE_360;
        }
       // Else, if the player is pressing the Left arrow.
       if (KeyState & 8)
        {
         // Increment the view angle by 6 degrees, Wrap at 360.
         if ((view_angle+=ANGLE_6)>=ANGLE_360)
	  view_angle-=ANGLE_360;
        }
		 // Else, if the player wants to go FORWARD...
       if (KeyState & 2)
        {
			// Set view_angle_90, and wrap it around at 360 deg.
	 view_angle_90 = view_angle + ANGLE_90;
	 if (view_angle_90 > ANGLE_360)
	  view_angle_90 = view_angle_90 - ANGLE_360;
         // Calculate the dx and dy to move forward, at the current angle. (Trig)
	 dx= (long)((sin_table[view_angle_90]*10)>>16);
	 dy= (long)((sin_table[view_angle]*10)>>16);
        }
		 // Else, if the player wants to move BACKWARDS
       if (KeyState & 1)
        {
         // Set view_angle_90, and wrap it around at 360 deg.
	 view_angle_90 = view_angle + ANGLE_90;
	 if (view_angle_90 > ANGLE_360)
	  view_angle_90 = view_angle_90 - ANGLE_360;
         // Calculate the dx and dy to move backward, at the current angle. (Trig)
	 dx= -(long)((sin_table[view_angle_90]*10)>>16);
 	 dy= -(long)((sin_table[view_angle]*10)>>16);
        }
       // Now , add the deltas to the current location,
       x+=dx;
       y+=dy;
       // Calculate the x and y cell and subcell values,
       x_cell = x>>6;
       y_cell = y>>6;
       x_sub_cell = x & 63;
		 y_sub_cell = y & 63;

      // Now, we do collision detection... (Bumping into walls..)
		// If we're moving WEST,
      if (dx>0)
		 {
        if ((*(world+(y_cell<<6)+x_cell+1) != 0) &&      // If there'a wall there,
            (!TextureFlags[*(world+(y_cell<<6)+x_cell+1)].CanWalkThru) && // And we can't walk through it,
            (x_sub_cell > (CELL_X_SIZE-OVERBOARD))) // And we're too close to it...
	 x-= (x_sub_cell-(CELL_X_SIZE-OVERBOARD )); // then MOVE US BACK A BIT.
       }
		// Same as above, but going EAST.
      else
       {
        if ((*(world+(y_cell<<6)+x_cell-1) != 0) &&        // If there'a wall there,
				(!TextureFlags[*(world+(y_cell<<6)+x_cell-1)].CanWalkThru) &&    // And we can't walk through it,
            (x_sub_cell < (OVERBOARD)))   // And we're too close to it...
 	 x+= (OVERBOARD-x_sub_cell) ;     // then MOVE US BACK A BIT.
       }
		// If we're going NORTH,
      if (dy>0 )
       {
        if ((*(world+((y_cell+1)<<6)+x_cell) != 0) &&     // If there'a wall there,
            (!TextureFlags[*(world+((y_cell+1)<<6)+x_cell)].CanWalkThru) &&   // And we can't walk through it,
            (y_sub_cell > (CELL_Y_SIZE-OVERBOARD)))           // And we're too close to it...
	 y-= (y_sub_cell-(CELL_Y_SIZE-OVERBOARD ));           // then MOVE US BACK A BIT.
       }
      // Else, if we're going SOUTH
      else
		 {
        if ((*(world+((y_cell-1)<<6)+x_cell) != 0) &&       // If there'a wall there,
            (!TextureFlags[*(world+((y_cell-1)<<6)+x_cell)].CanWalkThru) &&     // And we can't walk through it,
				(y_sub_cell < (OVERBOARD)))         // And we're too close to it
	  y+= (OVERBOARD-y_sub_cell);           // then MOVE US BACK A BIT.
		 }
     }

	 // Get ready to draw the screen again...
	 // Copy the background to the double buffer.
	 copyscrn(BkgBuffer);
	 // Render the view plane
	 Render_View(x,y,view_angle);
	 // Send it to the screen.
	 swapdblbuf();
	 // Increment our frame count.
	 ++frames;
	 // Go back an ddo it all again....
	}
	MemLeft = farcoreleft();
  // Now, the game is over,
  // Calculate the frames per second.
  fps = (float)frames/((clock()-initl)/(float)CLK_TCK);
  // Fade the screen out.
  fadeout();
  // Load in the ending screen.
  load_bkgd("ending.PCX",0);
  // Copy it over to the double buffer.
  copyscrn(BkgBuffer);
  // Move it to the screen,
  swapdblbuf();
  // Fade it in.
  fadein();
  // Wait for 8 seconds, or until ESC or SPACE is pressed.
  delay2(8);
  // Give us back the normal keyboard handler
  _dos_setvect(0x09,OldInt9);
  // Go back to 80 col. text mode.
  txtscreen();
  // Print out the FPS message.
  fprintf(stderr,"Frames per second: %f\nStarting Memory: %ld\n Memory left: %ld\nTotal Used: %ld",fps,InitMem,MemLeft,(unsigned long)(InitMem-MemLeft));
 }

// End Ray.cpp

