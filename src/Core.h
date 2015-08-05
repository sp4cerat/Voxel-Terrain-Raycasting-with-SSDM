#pragma once
////////////////////////////////////////////////////////////////////////////////
#define SCREEN_SIZE_X 1024
#define SCREEN_SIZE_Y 768
//#define SCREEN_SIZE_Y 624
#define RENDER_SIZE 2048
#define VIEW_DIST_MAX 300000

#define THREAD_COUNT_X 8 //16
#define THREAD_COUNT_Y 32

#define PERSISTENT_LG_X 5
#define PERSISTENT_LG_Y 1

#define PERSISTENT_X (1<<PERSISTENT_LG_X)
#define PERSISTENT_Y (1<<PERSISTENT_LG_Y)

#define OCTREE_DEPTH 16
#define OCTREE_DEPTH_AND ((1<<OCTREE_DEPTH)-1)

#define COARSE_OFFSET (RENDER_SIZE*SCREEN_SIZE_Y)

#define COARSE_SCALE 4

#define MAX_LOD 7
//#define LOD_ADJUST 2
//#define LOD_ADJUST_COARSE 2
#define LOD_ADJUST_COARSE 15/8
#define LOD_ADJUST 15/8
#define SCENE_LOOP 0
#define XOR_BREAK xory  //xory
////////////////////////////////////////////////////////////////////////////////
#define _USE_MATH_DEFINES
#define loop(a_l,start_l,end_l) for ( a_l = start_l;a_l<end_l;++a_l )
#define loops(a_l,start_l,end_l,step_l) for ( a_l = start_l;a_l<end_l;a_l+=step_l )

#ifndef byte
#define byte unsigned char
#endif

#ifndef ushort
#define ushort unsigned short
#endif

#ifndef uint
#define uint unsigned int
#endif

#ifndef uchar
#define uchar unsigned char
#endif
/*
#ifndef IN_CUDA_ENV
struct uchar4 
{
	unsigned char x,y,z,w;
	
	unsigned int to_uint()
	{
		return *((unsigned int*)this);
	};
};
#endif

  */
////////////////////////////////////////////////////////////////////////////////
class Keyboard
{
	public:

	bool  key [256]; // actual
	bool  key2[256]; // before

	Keyboard(){ int a; loop(a,0,256) key[a] = key2[a]=0; }

	bool KeyDn(char a)//key down
	{
		return key[a];
	}
	bool KeyPr(char a)//pressed
	{
		return ((!key2[a]) && key[a] );
	}
	bool KeyUp(char a)//released
	{
		return ((!key[a]) && key2[a] );
	}
	void update()
	{
		int a;loop( a,0,256 ) key2[a] = key[a];
	}
};
////////////////////////////////////////////////////////////////////////////////
class Mouse
{
	public:

	bool  button[256];
	bool  button2[256];
	float mouseX,mouseY;
	float mouseDX,mouseDY;

	Mouse()
	{ 
		int a; loop(a,0,256) button[a] = button2[a]=0; 
		mouseX=mouseY=mouseDX=mouseDY= 0;
	}
	void update()
	{
		int a;loop( a,0,256 ) button2[a] = button[a];
	}
};
////////////////////////////////////////////////////////////////////////////////
class Screen
{
	public:

	int	 window_width;
	int	 window_height;
	bool fullscreen;

	float posx,posy,posz;
	float rotx,roty,rotz;
};
////////////////////////////////////////////////////////////////////////////////
extern Keyboard		keyboard;
extern Mouse		mouse;
extern Screen		screen;
////////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif 
////////////////////////////////////////////////////////////////////////////////
extern void	cpu_memcpy(void* dst, void* src, int count);
extern void	gpu_memcpy(void* dst, void* src, int count);
extern void*	gpu_malloc(int size);
extern int		cpu_to_gpu_delta;
////////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
}
#endif 
////////////////////////////////////////////////////////////////////////////////
