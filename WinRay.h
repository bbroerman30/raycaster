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
#include <windows.h>
#include <time.h> 
#include <io.h> 
#include <conio.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <fcntl.h> 
#include <memory.h> 
#include <malloc.h> 
#include <math.h> 
#include <string.h>

// Windows defined stuff (from resource.h)
#define IDM_EXIT                        100
#define IDB_BITMAP1                     101
#define IDC_WINRAY2                     104
#define IDM_TEST                        200
#define IDM_ABOUT                       301
#define IDM_RUN                         40001
#define IDM_DISPLAY_320                 40002
#define IDM_DISPLAY_640                 40003
#define IDM_DISPLAY_960                 40004
#define IDM_SHOWKEYS                    40005
  
// If defined, then floor and ceiling are rendered.  
#define CAST_FLOORCIEL  
 
// How close you can get to a wall.  
#define TOOCLOSE 60  
  
// Size of the viewport. 
#define VIEWPORT_WIDTH	240 
#define VIEWPORT_HEIGHT	200 
 
// For keyboard handling (in moving player), these query the keyboard in real-time
#define KEY_DOWN(vk_code)	((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)
#define KEY_UP(vk_code)		((GetAsyncKeyState(vk_code) & 0x8000) ? 0 : 1)

// Our definitions for the angles.  ANGLE_60 = width of viewport. (60 deg. FOV)  
// All the other angles are based on this.  
#define ANGLE_0     0  
#define ANGLE_1     4 
#define ANGLE_6     24  
#define ANGLE_30    120 
#define ANGLE_45    180  
#define ANGLE_60    240 
#define ANGLE_90    360  
#define ANGLE_120   480  
#define ANGLE_150   600  
#define ANGLE_180   720  
#define ANGLE_210   840  
#define ANGLE_225   900  
#define ANGLE_240   960  
#define ANGLE_270   1080  
#define ANGLE_300   1200  
#define ANGLE_330   1320
#define ANGLE_360   1440  
  
// The size of each cell. 128 makes the math easier. (also textures are 128 pixels wide)  
#define CELL_X_SIZE   128  
#define CELL_Y_SIZE   128  
  
// The maximum size of our world 64x64 cells.  
#define MAX_WORLD_ROWS 64  
#define MAX_WORLD_COLS 64  
  
// These defines are used for lighting.  
#define MAX_LIGHT_LEVELS  128  
#define PALETTE_SIZE      256ul  
#define MAX_DISTANCE      (MAX_WORLD_ROWS * MAX_WORLD_COLS)  
  
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
    short x,y;             // We ignore these.  
    short width,height;    // Will be either 64x64 or 320x200  
    short horiz_rez;       // Forget about these...  
    short vert_rez;  
    char  EGA_palette[48]; // We won't even touch this.  
    char  reserved;  
    char  Num_Planes;      // We want 1  
    short bytes_per_line;  // Either 64 or 320  
    short palette_type;    // We can forget about these too..  
    char  padding[58];  
} PCX_HEADER;  
  
// Stores slice information for rendering. This is used so that we can  
// add sprites, transparency, etc. and have the rendering engine draw them too.  
struct col_obj  
{  
    int  top,         // Wihch row to begin drawing this slice.  
         col,         // Which screen column this slice is in.  
         scale,       // How much do I scale this slice  
         row,         // Which texture column to render for this slice.  
         texnum,      // The number of the wall/sprite texture to use.  
         lightnum,    // The number of the lighting tile to use. If -1, then use sptlight as the light for the whole sliver.  
         sptlight,    // Flat light value (if used).  
         zoneambient, // Ambient light for the zone the sliver is in.  
         usefog;      // Use the fog table or the darkness table for the sliver.  
    long dist;        // How far away is it (Used for sorting in sprites and light sourcing.)  
  
};  
  
// This implements the multiple levels of transparency we can use. The  
// objecs are ordered from nearest to farthest.  
struct scan_line_t  
{  
    col_obj line[10];  // We can go to 10 levels of transparency, sprites, etc.  
    int num_objs;  
};  
  
// This structure is kept for every texture map loaded into the game. It is  
// used (mainly) for doors and transparent items.  
struct TextureFlagsTYPE  
{  
    char IsTransparent, // 1=transparent, 0=not.  
         IsTranslucent, // 1=Translucent, 0=not.  
         IsDoor,        // 1= door, 0= regular wall.  
         DoorType,      // 1=normal, 2=elevator, 3=garage  
         IsSecretDoor,  // 1=Secret door, 0=Normal door.  
         IsRecessed,    // 1=Recessed texture(walls), 0=Not recessed  
         DoorSideTxt,   // This holds the texture number for the door sides. (Not used yet.)  
         CanWalkThru,   // 1 = you can walk trough it, 0 = Can not walk through it.  
         speed,         // How fast the door opens and closes. (cols per frame)  
         delay,         // How many frames to wait before closing the door.  
         bc,            // Upper left corner of the door  
         br,  
         ec,            // Lower right corner of the door  
         er;  
    int  AnimNbr;  
};  
  
// This structure keeps the parameters for the lights, including light type,  
// and blink parameters.  
struct LightFlagsTYPE  
{  
    char LightType;           // Either a 1, or a 2. 1: Regular blink, 2: Custom blink.  
    int  PulseWidth,          // For a regular blink, defines the pulse.  
         Period,  
         CurrPatternIndx;     // counter... see ProcessBlinkLights() for usage.  
    char *PulsePattern;       // For a custom blinking light, this is the pattern: 1 - on, 0= off.  
    char *OriginalBitmap; // Used to track the original bitmap. see ProcessBlinkLights() for usage.  
    int  AnimNbr;  
};  
  
// This structure keeps track of all the doors that are opening and closing  
// at any particular time.  
struct Open_Doors_Info  
{  
    char USED;          // Used in finding an empty spot to use. See the code.  
    int  x,             // Map location of the door.  
         y,  
         delay,         // The door's delay from above.  
         Orig_Txt_Num,  // The texture number of the original (un-opened) door.  
         New_Txt_Num,   // Points to the working copy of the door tex.  
         Pcnt_opened,   // Percent opened.  
         Num_Col_Opnd,  // The number of columns the door is opening.  
         Opening;       // 1=opening, 0=closing.  
};  
  
// This structure keeps track of information associated with all doors on the map.  
// Work in progress.  
struct Door_Info  
{  
    int x,            // Map location of the door.  
        y,  
        locked,       // Can I open it or not? 1 = Locked , 0 = Un-Locked.  
        opening;      // lets up know if the texture is not the original one. (!= 0) --> Don't touch!  
};  
  
// This is the basic structure for the sprites.  
struct Sprite_Type  
  
{  
    char *Frames[8];  // Stores the views (angles object can be viewed from)  
    int  translucent; // Specifies wether or not this sprites texture is translucent. (Sprites are always transparent)  
};  
  
// Sprite instance variables.  
// Work in progrss,  
struct object  
{  
    int sprite_num,  // The sprite array object we're using.  
        x,           // The sprites position in the map (fine coordiates)  
        y,  
        angle,       // The angle the sprite is facing.  
        state;       // The sprites state (To be used later)  
};  

// The struct that holds the player information. (I am keeping this separate from object since I
// plan on changing the two considerably.
struct player_t
{
	long x,                    // The players X position, in fine coordinates. 
         y,                    // The players Y position, in fine coordinates. 
         view_angle;           // The angle which the player is facing. 
};

  
//    This structure holds information about anumated textures. This includes the  
//  frames, and counter / timing information.  
struct Animations_Typ  
{  
    char **Frames;      // Array of pointers to the frames.  
    int    Flagged,       // 1=Animate this texture, 0=Show only 1st frame.  
           Txt_Nbr,       // Which texture in the map this applies to.  
           Delay,         // Delay time between frames.  
           Timer,         // Counts the delay for this texture.  
           Curr_Frame,    // The frame we are currently showing.  
           Nbr_Frames;    // The number of frames.  
};  
  
//   This structure holds information about the various defined zones. These zones  
// could be rooms, the outside, etc.  
struct zone_info  
{  
    char ambient,       // Ambient light level 0-64 for this zone.  
         inside,        // 1=inside, 0=outside  (turns on/off ceiling lighting)  
         fog;           // 1=Use fog, 0=use normal depth cueing.  
};  
  
//   This structure is used to record trigger information.  
// Work in progrss...  
struct trigger_info  
{  
    int  Trigger_Nbr,  
         Trigger_Type,  
         Trigger_Area_Type,  
         X1, Y1,  
         X2, Y2,  
         Radius,  
         MapX,  
         Mapy,  
         ZoneNbr,  
         NewLightLvl,  
         NewAngle,  
         TxtNbr;  
    char NewFileName[13];  
};  

//
//    This enum is used in the window procedure to change the procedures functionality
//  based on game state (displaying the title, displaying the menu, or running the game ).
//
enum {	SEQ_TITLESCREEN = 1,
		SEQ_FADETOGAME, 
		SEQ_PLAYGAME,
		SEQ_FADETOLEVEL, 
		SEQ_MENUDISPLAY
} GameState_Typ;
  
// These globals are used for speeding up the tracing and rendering parts.  
// These are copied from the col_obj structure above.  
long sliver_dist;  
int  sliver_start,  // Where (y) to begin drawing the slice.  
     sliver_column, // Screen column to draw in.  
     sliver_scale,  // Height of the sliver (pixels).  
     sliver_row,    // Column of the texture to draw.  
     sliver_map,    // Which texture to use.  
     sliver_light,  // Which lighting tile to use.  
     sliver_sptlt,  // Lighting level for sprites.  
     sliver_zoneamb,// Ambient light delta for the current zone.  
     sliver_usefog, // If 1, use fog table, else use darkness table.  
  
// These are used in the tracing process. They are global for the same reason as above.  
    cell_xx,       // These store the coordinates for the horiz. trace.  
    cell_yx,       //  
    cell_xy,       // These store the coordinates for the vert. trace.  
    cell_yy;       //  
  
// These are global because they're used all over, and I wanted to generate them ONLY  
// at the beginning of the program. It also saves us on stack usage.  

// Information needed by the application itself.
HWND ghWnd;			// Handle to the main window.
BOOL bIsActive;		// Tells us when we should be rendering, and when we should stop.
BOOL bRunGame;		// Tells us wether we are running the game or showing the title screen.
HINSTANCE hInst;	// current application instance
LPCTSTR lpszAppName  = "WinRay2"; // The application and window class name.
LPCTSTR lpszTitle    = "WinRay"; // The main window title.

// Pointers to double buffer info and address. 
BITMAPINFO *BitMapInfo ;
HBITMAP DblBuff;
unsigned char *double_buffer;
    
// Vaarious pointers and constants used while rendering the images.
int    WORLD_ROWS = 64;             // Current size of the map.  
int    WORLD_COLUMNS = 64;  
long   WORLD_X_SIZE = 8192;         // Total size of the world (64cells x 128 per cell)  
long   WORLD_Y_SIZE = 8192;  
char   *AutoMapBkgd;  
char   *world = NULL;               // pointer to matrix of cells that make up world  
char   *FloorMap = NULL;            // pointer to the matrix of cells that define the floor textures.  
char   *CeilMap = NULL;             // pointer to the matrix of cells that define the ceiling textures.  
char   *ZoneMap = NULL;             // pointer to the matrix of calls that define the 'zones' for various effects.  
char   TriggerMap[64][64];          // Assists in quick detection of simple triggers. Maps location to trigger number.  
char   *CeilLightMap = NULL;        // pointer to the ceiling light map.  
char   *FloorLightMap = NULL;       // pointer to the floor light map.  
int    Num_Textures = 0;            // The number of textures we've loaded.  
char   *WallTextures[90];           // The 64x64 wall textures, door textures, and visible sprites.  
int    Num_Light_Tiles = 0;         // The number of lighting tiles for the floor/ceiling light maps.  
char   *LightMapTiles[30];          // The 64x64 lighting map tiles.  
char   *LightLevel = NULL;          // Gives the lighting level as a function of distance.  
char   *LightMap = NULL;            // Maps a color/light pair into a pallete value for depth cueing. 32*256 in size.  
char   *FogMap = NULL;              // Maps a color/light pair into a pallete value for fog effects. 32*256 in size.  
char   *Translucency = NULL;        // Trans. lookup. Blends a color/color pair with 10% color 1, 90% color 2. Gives palette value.  
char   *BkgBuffer = NULL;           // The background picture.  
long   *tan_table = NULL;           // tangent tables used to compute initial  
long   *inv_tan_table = NULL;       // intersections with ray  
long   *y_step = NULL;              // x and y steps, used to find intersections  
long   *x_step = NULL;              // after initial one is found  
long   *cos_table = NULL;           // used to cacell out fishbowl effect  
long   *sin_table = NULL;           // Used for movement in different directions.  
long   *inv_cos_table = NULL;       // used to compute distances by calculating  
long   *inv_sin_table = NULL;       // the hypontenuse  
long   *sliver_factor = NULL;       // Scale factor for texture slivers.  
long   Floor_Row_Table[151];        // Pre-calculated row start values.  
long   *Floor_inv_cos_table = NULL; // The following tables are used in the math for the floor/ceiling.  
long   *Floor_cos_table = NULL;  
long   *Floor_sin_table = NULL;  
long   *Floor_Dx_table = NULL;  
long   row[200];                    // Pre-calculates 320*row.  
int    oldmode;                     // Original grahics/text mode when the game starts.  
char   palette[256][3];             // The current palette.  
int    Num_Sprites = 0;             // The number of sprites defined.  
int    Num_Objects = 0;             // The number of objects defined.  
int    Sprite_Frame[361];           // Determines frame to display for sprites.  
int    Num_Animations = 0;          // The number of animated textures we are using.  
int    Num_LightAnimations = 0;     // The number of animated lighting tiles we are using.  
int    Num_Triggers = 0;            // The number of defined triggers.  
short  ambient_level = 3;           // The global ambient light level.  
struct scan_line_t       scan_lines[240];          // Stores the scan-line info.  
struct TextureFlagsTYPE  TextureFlags[81];         // Stores the texture flags for each loaded texture.  
struct LightFlagsTYPE    LightFlags[30];           // Stores the flags for the lights.  
struct Open_Doors_Info   Opening_Doors[10];        // Moving door info.  
struct zone_info         ZoneAttr[20];             // Attributes for the zones. Currently: ambient light, fog, and inside/outside  
struct Sprite_Type       Sprites[10];              // The sprites. These are the bitmaps.  
struct object            Objects[30];              // The actual objects that use the sprites.  
struct Animations_Typ    Animations[10];           // Up to 10 animated textures.  
struct Animations_Typ    Light_Animations[10];     // Up to 10 animated lighting tiles.  
struct trigger_info      Triggers[10];             // The loaded triggers.  

// The following items are game state information:
char   ShowAutomap = 0;                            // This flag tells if the automap is on or off.
char   TorchLevel = 8;                             // This is the light level the players torch is putting out.
short  ClientWidth = 320;                          // The current screen dimentions.
short  ClientHeight = 200;                         //
short  ClientxPos;                                 // The current screen position.
short  ClientyPos;                                 //
struct player_t			Player_1;				   // The player.
enum   GameState_Typ	GameState;                 // This is a state variable that guides us in the WM_TIMER message.

// This is used for the title animation sequence.
char   *FireSrc; 

// 
// Function Prototypes: 
// 
// Thses functions are the primitive screen handling functions: 
void clearscrn(char c);  
void copyscrn (char far *Buffer);  
void dissolveto(char far *Buffer);  
void blit(void);
 
// These functions build and maintain the data tables. 
void free_tables(void);  
int  Die(char *string); 
int  Build_Tables(void);  
void CalcTorchLevel(int level);  
 
// These functions handle loading and saving game graohics. 
int  Save_Pcx(char *filename, unsigned char far *Buffer, unsigned long width, unsigned long height);  
char *Load_PCX(char *filename, unsigned long size, int load_pal);  
int  transpose(char far *Bitmap);  
int  load_bkgd(char *filename);  
int  Load_Wall(char *filename,int offset);  
int  Load_Light_Tile(char *filename,int offset);  
char far *Load_Sprite (char *filename);  
 
// This function loads a map file. 
unsigned char translateChar(unsigned char ch); 
void Clear_Map(void); 
int  Load_Map(long &initx, long &inity, long &initang, char *MapFileName);  
 
// The following functions render the world...  
void draw_sliver_transparent(void);  
void draw_sliver_trans(void);  
void draw_sliver(void);  
int  Cast_X_Ray(long x, long y, long view_angle, long &yi_save, long &x_save, int &lighttile, int &zone);  
int  Cast_Y_Ray(long x, long y, long view_angle, long &xi_save, long &y_save, int &lighttile, int &zone);  
void Texture_FloorCeil(long x, long y, long view_angle);  
void process_sprites(long x, long y, long view_angle);  
void DrawAutomap(long Xpos, long Ypos);  
void Render_View(long x,long y,long view_angle);  
 
// The last few functions perform the game logic...  
void delay2(long time); 
int  start_door(int x, int y);  
void open_doors();  
void close_doors();  
void ProcessTrigger(long &x, long &y, long &view_angle, int TriggerNbr);  
void CheckTriggers(long &x, long &y, long &view_angle);  
int  Rotate_Animations(void);  
void processblinklights(void);  
void ProccessUserMovement( void );
void MainLoop(void); 
int  InitTitleAnimation (void);
void AnimateFireScreen (void);
int  EndTitleAnimation (void);

// And finally, the main process. 
int InitEngine(void);  
void init_double_buffer(void);
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);


