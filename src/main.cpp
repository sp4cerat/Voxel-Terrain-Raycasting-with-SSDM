#include <windows.h>
#include "glee.h"
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <GL/glut.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <cassert>

#include <cutil_inline.h>
#include <cutil_gl_inline.h>
#include <cutil_gl_error.h>
#include <cuda_gl_interop.h>

//#include "Vector3.h"
#include <math.h>
#include "Core.h"
#include "VecMath.h"
#include "fbo.h"
#include "Bmp.h"

#define MAX_KEYS 256
//#define VOLUME_TEX_SIZE 512
#define VOLUME_TEX_SIZE_X 1024
#define VOLUME_TEX_SIZE_Y 256
#define VOLUME_TEX_SIZE_Z 1024
#define VOLUME_TEX_SIZE_COL 64

extern "C" bool pboRegister(int pbo);
extern "C" void* cuda_main( int pbo_out, 
						   int size_x, 
						   int size_y, 
						   int size_z, 
						   int level, 
						   float box_x,float box_y,float box_z,
						   float pos_x,float pos_y,float pos_z );

void MouseMotionStatic (int x,int y)
{
	mouse.mouseX = float(x)/float(SCREEN_SIZE_X);
	mouse.mouseY = float(y)/float(SCREEN_SIZE_Y);
}

using namespace std;

// Globals ------------------------------------------------------------------

bool gKeys[MAX_KEYS];
bool toggle_visuals = true;
CGcontext context; 
CGprofile vertexProfile, fragmentProfile; 
CGparameter param1,param2,param3,param4,param5;
CGprogram	vertex_main,
			vertex_ssdm_displace,
			fragment_ssao,
			fragment_ssdm,
			fragment_ssdm_displace,
			fragment_ssdm_mip,
			fragment_ssdm_final,
			fragment_main,
			fragment_main_lowres; // the raycasting shader programs
GLuint volume_texture; // the volume texture
GLuint volume_texture_col; // the volume texture
float stepsize = 1.0/50.0;

vec3f	box_ini(1000,1000,1000);
vec3f	box_pos[2]={box_ini,box_ini};
float	box_dist[2]={1000,1000};

/// Implementation ----------------------------------------

void cgErrorCallback()
{
	CGerror lastError = cgGetError(); 
	if(lastError)
	{
		cout << cgGetErrorString(lastError) << endl;
		if(context != NULL)
			cout << cgGetLastListing(context) << endl;
		while(1);;
		exit(0);
	}
}

// Sets a uniform texture parameter
void set_tex_param(char* par, GLuint tex,const CGprogram &program,CGparameter param) 
{
	param = cgGetNamedParameter(program, par); 
	cgGLSetTextureParameter(param, tex); 
	cgGLEnableTextureParameter(param);
}


// load_vertex_program: loading a vertex program
void load_vertex_program(CGprogram &v_program,char *shader_path, char *program_name)
{
	assert(cgIsContext(context));
//	v_program = cgCreateProgram(context, CG_SOURCE,cgshader,
	v_program = cgCreateProgramFromFile(context, CG_SOURCE,shader_path,
		vertexProfile,program_name, NULL);	
	if (!cgIsProgramCompiled(v_program))
		cgCompileProgram(v_program);

	cgGLEnableProfile(vertexProfile);
	cgGLLoadProgram(v_program);
	cgGLDisableProfile(vertexProfile);
}

// load_fragment_program: loading a fragment program
void load_fragment_program(CGprogram &f_program,char *shader_path, char *program_name)
{
	char *param[3]={"-unroll","none","-fastmath"}	 ;

	assert(cgIsContext(context));
//	f_program = cgCreateProgram(context, CG_SOURCE,cgshader,
//	v_program = cgCreateProgramFromFile(context, CG_SOURCE,shader_path,
	f_program = cgCreateProgramFromFile(context, CG_SOURCE, shader_path,
		fragmentProfile,program_name, NULL);	
	if (!cgIsProgramCompiled(f_program))
		cgCompileProgram(f_program);

	cgGLEnableProfile(fragmentProfile);
	cgGLLoadProgram(f_program);
	cgGLDisableProfile(fragmentProfile);
}

void vertex(float x, float y, float z)
{
	glColor3f(x,y,z);
	glMultiTexCoord3fARB(GL_TEXTURE1_ARB, x, y, z);
	glVertex3f(x,y,z);
}

// this method is used to draw the front and backside of the volume
void drawQuads(float x, float y, float z)
{
	
	glBegin(GL_QUADS);
	/* Back side */
	glNormal3f(0.0, 0.0, -1.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(x, y, 0.0);
	vertex(x, 0.0, 0.0);

	/* Front side */
	glNormal3f(0.0, 0.0, 1.0);
	vertex(0.0, 0.0, z);
	vertex(x, 0.0, z);
	vertex(x, y, z);
	vertex(0.0, y, z);

	/* Top side */
	glNormal3f(0.0, 1.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(0.0, y, z);
    vertex(x, y, z);
	vertex(x, y, 0.0);

	/* Bottom side */
	glNormal3f(0.0, -1.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, 0.0, z);
	vertex(0.0, 0.0, z);

	/* Left side */
	glNormal3f(-1.0, 0.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, 0.0, z);
	vertex(0.0, y, z);
	vertex(0.0, y, 0.0);

	/* Right side */
	glNormal3f(1.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, y, 0.0);
	vertex(x, y, z);
	vertex(x, 0.0, z);
	glEnd();
	
}

// create a test volume texture, here you could load your own volume
void create_volumetexture()
{
	int size = 
		VOLUME_TEX_SIZE_X *
		VOLUME_TEX_SIZE_Y * 
		VOLUME_TEX_SIZE_Z ;
	GLubyte *data = new GLubyte[size];

	cout << "creating terrain volume texture " << (int)(size/1000000) << "MB" << endl;
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume_texture);
	glBindTexture(GL_TEXTURE_3D, volume_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexImage3D(GL_TEXTURE_3D, 0,GL_LUMINANCE, 
		VOLUME_TEX_SIZE_X, 
		VOLUME_TEX_SIZE_Y,
		VOLUME_TEX_SIZE_Z,
		0, GL_LUMINANCE, GL_UNSIGNED_BYTE,data);

	delete []data;
	cout << "terrain volume texture creation finished" << endl;

	int size2 = VOLUME_TEX_SIZE_COL*VOLUME_TEX_SIZE_COL*VOLUME_TEX_SIZE_COL* 4;
	GLubyte *data2 = new GLubyte[size2];

	for (int x = 0; x < VOLUME_TEX_SIZE_COL; x++)
	{for(int y = 0; y < VOLUME_TEX_SIZE_COL; y++)
	{for(int z = 0; z < VOLUME_TEX_SIZE_COL; z++)
	{
		int 
		rn=rand()&255;data2[(x*4)+0+ (y * VOLUME_TEX_SIZE_COL * 4) + (z * VOLUME_TEX_SIZE_COL * VOLUME_TEX_SIZE_COL * 4)] = rn;
		rn=rand()&255;data2[(x*4)+1+ (y * VOLUME_TEX_SIZE_COL * 4) + (z * VOLUME_TEX_SIZE_COL * VOLUME_TEX_SIZE_COL * 4)] = rn;
		rn=rand()&255;data2[(x*4)+2+ (y * VOLUME_TEX_SIZE_COL * 4) + (z * VOLUME_TEX_SIZE_COL * VOLUME_TEX_SIZE_COL * 4)] = rn;
		rn=rand()&255;data2[(x*4)+3+ (y * VOLUME_TEX_SIZE_COL * 4) + (z * VOLUME_TEX_SIZE_COL * VOLUME_TEX_SIZE_COL * 4)] = rn;
	}}}

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume_texture_col);
	glBindTexture(GL_TEXTURE_3D, volume_texture_col);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexImage3D(GL_TEXTURE_3D, 0,GL_LUMINANCE, 
		VOLUME_TEX_SIZE_COL, 
		VOLUME_TEX_SIZE_COL,
		VOLUME_TEX_SIZE_COL,
		0, GL_LUMINANCE, GL_UNSIGNED_BYTE,data2);
/*	glTexImage3D(GL_TEXTURE_3D, 0,GL_RGBA, 
		VOLUME_TEX_SIZE_COL, 
		VOLUME_TEX_SIZE_COL,
		VOLUME_TEX_SIZE_COL,0, GL_RGBA, GL_UNSIGNED_BYTE,data2);*/

	delete []data2;

	cout << "color volume texture created" << endl;
}

// ok let's start things up 

void init()
{
	/*
	cout << "Glew init " << endl;
	GLenum err = glewInit();

	// initialize all the OpenGL extensions
	glewGetExtension("glMultiTexCoord2fvARB");  

	if (glewGetExtension("GL_ARB_fragment_shader")      != GL_TRUE ||
		glewGetExtension("GL_ARB_vertex_shader")        != GL_TRUE ||
		glewGetExtension("GL_ARB_shader_objects")       != GL_TRUE ||
		glewGetExtension("GL_ARB_shading_language_100") != GL_TRUE)
	{
		cout << "Driver does not support OpenGL Shading Language" << endl;
		exit(1);
	}*/

	glEnable(GL_CULL_FACE);
	glClearColor(0.0, 0.0, 0.0, 0);
	create_volumetexture();

	// CG init
	cgSetErrorCallback(cgErrorCallback);
	context = cgCreateContext();
	if (cgGLIsProfileSupported(CG_PROFILE_VP40))
	{
		vertexProfile = CG_PROFILE_VP40;
		cout << "CG_PROFILE_VP40 supported." << endl; 
	}
	else 
	{
		if (cgGLIsProfileSupported(CG_PROFILE_ARBVP1))
			vertexProfile = CG_PROFILE_ARBVP1;
		else
		{
			cout << "Neither arbvp1 or vp40 vertex profiles supported on this system." << endl;
			exit(1);
		}
	}

	if (cgGLIsProfileSupported(CG_PROFILE_FP40))
	{
		fragmentProfile = CG_PROFILE_FP40;
		cout << "CG_PROFILE_FP40 supported." << endl; 
	}
	else 
	{
		if (cgGLIsProfileSupported(CG_PROFILE_ARBFP1))
			fragmentProfile = CG_PROFILE_ARBFP1;
		else
		{
			cout << "Neither arbfp1 or fp40 fragment profiles supported on this system." << endl;
			exit(1);
		}
	}

	// load the vertex and fragment raycasting programs
	load_vertex_program(vertex_main,"raycasting_shader.cg","vertex_main");
	cgErrorCallback();
	load_vertex_program(vertex_ssdm_displace,"raycasting_shader.cg","vertex_ssdm_displace");
	cgErrorCallback();
	load_fragment_program(fragment_main,"raycasting_shader.cg","fragment_main");
	cgErrorCallback();
	load_fragment_program(fragment_main_lowres,"raycasting_shader.cg","fragment_main_lowres");
	cgErrorCallback();
	load_fragment_program(fragment_ssdm,"raycasting_shader.cg","fragment_ssdm");
	cgErrorCallback();
	load_fragment_program(fragment_ssdm_mip,"raycasting_shader.cg","fragment_ssdm_mip");
	cgErrorCallback();
	load_fragment_program(fragment_ssdm_final,"raycasting_shader.cg","fragment_ssdm_final");
	cgErrorCallback();
	load_fragment_program(fragment_ssdm_displace,"raycasting_shader.cg","fragment_ssdm_displace");	
	cgErrorCallback();
	load_fragment_program(fragment_ssao,"raycasting_shader.cg","fragment_ssao");
	cgErrorCallback();
}

// for contiunes keypresses
void ProcessKeys()
{	
	static int t1 = timeGetTime();
	static int t2 = timeGetTime();

	t2=t1;
	t1=timeGetTime();
	float dt = t1-t2;

	float step = 0.0000031 * dt;


	screen.rotx = screen.rotx*0.9+0.1*((mouse.mouseY)*15);
	screen.roty = screen.roty*0.9+0.1*((mouse.mouseX)*15+M_PI/2);
	screen.rotx = 1*((mouse.mouseY)*15);
	screen.roty = 1*((mouse.mouseX)*15+M_PI/2);

	matrix44 m;
	m.ident();
	//m.rotate_z(screen.rotz );
	m.rotate_x(screen.rotx );
	m.rotate_y(screen.roty );

	vec3f pos( screen.posx,screen.posy,screen.posz );
	vec3f forward = m * vec3f(0,0,-step).v3(); forward.x*=-1;forward.y*=-1;
	vec3f side	  = m * vec3f(-step,0,0).v3(); side.x*=-1;side.y*=-1;
	vec3f updown  = m * vec3f(0,-step,0).v3(); updown.x*=-1;updown.y*=-1;

	// Process keys
	for (int i = 0; i < 256; i++)
	{
		if (!gKeys[i])  { continue; }
		switch (i)
		{
		case ' ':
			break;
		case 'w':		pos = pos + forward;	break;
		case 's':		pos = pos - forward;	break;
		case 'a':	    pos = pos - side;		break;
		case 'd':		pos = pos + side;		break;
		case 'q':	    pos = pos - updown;		break;
		case 'e':		pos = pos + updown;		break;
		case 'r':
			stepsize += 1.0/2048.0;
			if(stepsize > 0.25) stepsize = 0.25;
			break;
		case 'f':
			stepsize -= 1.0/2048.0;
			if(stepsize <= 1.0/200.0) stepsize = 1.0/200.0;
			break;
		}
	}
   screen.posx=pos.x;
   screen.posy=pos.y;
   screen.posz=pos.z;
}

void key(unsigned char k, int x, int y)
{
	gKeys[k] = true;
}

void KeyboardUpCallback(unsigned char key, int x, int y)
{
	gKeys[key] = false;

	switch (key)
	{
	case 27 :
		{
			exit(0); break; 
		}
	case ' ':
		toggle_visuals = !toggle_visuals;
		break;
	}
}

// glut idle function
void idle_func()
{
	ProcessKeys();
	glutPostRedisplay();
}

void reshape_ortho(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0,w,h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 1, 0, 1);
	glMatrixMode(GL_MODELVIEW);
}


void resize(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(90.0, (GLfloat)w/(GLfloat)h, 0.01, 400.0);
	glMatrixMode(GL_MODELVIEW);
}

void draw_fullscreen_quad()
{
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_QUADS);
   
	glTexCoord2f(0,0); 
	glVertex2f(0,0);	 
	glTexCoord2f(1,0); 
	glVertex2f(1,0);	 
	glTexCoord2f(1, 1);	 
	glVertex2f(1, 1);
	glTexCoord2f(0, 1); 
	glVertex2f(0, 1);

	glEnd();
	glEnable(GL_DEPTH_TEST);
}

void draw_fullscreen_quad_ssao(int color_tex,int depth_tex,int shadow_tex)
{
	glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
								   	
	CGprogram fragment_prg = fragment_ssao;
	CGprogram vertex_prg   = vertex_main;

	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(vertex_prg);
	cgGLBindProgram(fragment_prg);
	set_tex_param("depth_tex",depth_tex,fragment_prg,param1);
	set_tex_param("color_tex",color_tex,fragment_prg,param2);
	set_tex_param("shadow_tex",shadow_tex,fragment_prg,param3);
		

	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), 
		float(screen.window_width),float(screen.window_height));

	glBegin(GL_QUADS);
   
	glTexCoord2f(0,0); 
	glVertex2f(0,0);	 
	glTexCoord2f(1,0); 
	glVertex2f(1,0);	 
	glTexCoord2f(1, 1);	 
	glVertex2f(1, 1);
	glTexCoord2f(0, 1); 
	glVertex2f(0, 1);

	glEnd();
	glEnable(GL_DEPTH_TEST);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

void draw_fullscreen_quad_2mio(int rgba_tex,int ssdm_tex)
{
	// create one display list
	static int gl_list = -1;

	glDisable(GL_DEPTH_TEST);

	if(gl_list == -1)
	{
		gl_list=glGenLists(1);
		glNewList(gl_list, GL_COMPILE);

		int gridx=screen.window_width;
		int gridy=screen.window_height;

		for (int b=0;b<gridy;b++)
		{
			glBegin(GL_TRIANGLE_STRIP);

			for (int a=0;a<=gridx;a++)
			{
				float x0 = float(a  )/float(gridx);
				float y0 = float(b  )/float(gridy);
				float x1 = float(a+1)/float(gridx);
				float y1 = float(b+1)/float(gridy);
				//glTexCoord2f(x0, y0);	 
				glVertex2f  (x0, y0);
				//glTexCoord2f(x0, y1);	 
				glVertex2f  (x0, y1);
			}
			glEnd();
		}
		glEndList();
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	CGprogram fragment_prg = fragment_ssdm_displace;
	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(fragment_prg);
	set_tex_param("ssdm_tex",ssdm_tex,fragment_prg,param1);
	set_tex_param("rgba_tex",rgba_tex,fragment_prg,param2);
		
	cgGLBindProgram(vertex_ssdm_displace);
	set_tex_param("ssdm_tex",ssdm_tex,vertex_ssdm_displace,param3);
	set_tex_param("rgba_tex",rgba_tex,vertex_ssdm_displace,param4);

	//set_tex_param("rgba_tex",rgba_tex,vertex_ssdm_displace,param1);
	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), 
		float(screen.window_width),float(screen.window_height));

	glCallList(gl_list);
	glEnable(GL_DEPTH_TEST);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

void raycasting_pass(bool lowres,int lowres_tex=0)
{
	if ( lowres ) 
	{
//		glDisable(GL_DEPTH_BUFFER );
//		glDisable(GL_DEPTH_TEST);
		glEnable(GL_DEPTH_BUFFER );
		glEnable(GL_DEPTH_TEST);
	}
	else
	{
		glEnable(GL_DEPTH_BUFFER );
		glEnable(GL_DEPTH_TEST);
	}

	glDepthFunc(GL_ALWAYS);
	CGprogram fragment_prg = fragment_main;
	if ( lowres ) fragment_prg = fragment_main_lowres;

	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(vertex_main);
	cgGLBindProgram(fragment_prg);
	cgGLSetParameter4f (cgGetNamedParameter( fragment_prg, "viewpoint"), screen.posx,screen.posy,screen.posz,0);
	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), float(screen.window_width),float(screen.window_height));
	set_tex_param("lowres_tex",lowres_tex,fragment_prg,param1);
	set_tex_param("volume_tex",volume_texture,fragment_prg,param2);
	set_tex_param("volume_tex_col",volume_texture_col,fragment_prg,param3);

	drawQuads(1.0, 1.0, 1.0);
	glDisable(GL_CULL_FACE);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

void ssdm_pass(int rgba_tex,int zbuf_tex, matrix44 &m )
{
	resize(screen.window_width,screen.window_height);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	reshape_ortho(screen.window_width,screen.window_height);
	glDisable(GL_DEPTH_BUFFER );
	glDisable(GL_DEPTH_TEST);

	CGprogram fragment_prg = fragment_ssdm;
	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(vertex_main);
	cgGLBindProgram(fragment_prg);
	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), float(screen.window_width),float(screen.window_height));
	set_tex_param("zbuf_tex",zbuf_tex,fragment_prg,param1);
	set_tex_param("rgba_tex",rgba_tex,fragment_prg,param2);

	vector3 xcomp = m.x_component();
	vector3 ycomp = m.y_component();
	vector3 zcomp = m.z_component();
	cgGLSetParameter4f (cgGetNamedParameter( fragment_prg, "matrix1"), xcomp.x ,xcomp.y ,xcomp.z ,	0);
	cgGLSetParameter4f (cgGetNamedParameter( fragment_prg, "matrix2"), ycomp.x ,ycomp.y ,ycomp.z ,	0);
	cgGLSetParameter4f (cgGetNamedParameter( fragment_prg, "matrix3"), zcomp.x ,zcomp.y ,zcomp.z ,	0);

	draw_fullscreen_quad();
	glDisable(GL_CULL_FACE);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

void ssdm_mip_pass(int rgba_tex,int width,int height)
{
	resize(width,height);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	reshape_ortho(width,height);
	glDisable(GL_DEPTH_BUFFER );

	CGprogram fragment_prg = fragment_ssdm_mip;
	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(vertex_main);
	cgGLBindProgram(fragment_prg);
	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), 
		float(width),
		float(height));
	set_tex_param("rgba_tex",rgba_tex,fragment_prg,param2);

	draw_fullscreen_quad();
	glDisable(GL_CULL_FACE);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

void ssdm_final_pass(int width,int height, int t1,int t2,int t3,int t4,int rgba_tex)
{
	resize(width,height);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	reshape_ortho(width,height);
	glDisable(GL_DEPTH_BUFFER );

	CGprogram fragment_prg = fragment_ssdm_final;
	cgGLEnableProfile(vertexProfile);
	cgGLEnableProfile(fragmentProfile);
	cgGLBindProgram(vertex_main);
	cgGLBindProgram(fragment_prg);
	cgGLSetParameter2f (cgGetNamedParameter( fragment_prg, "screen"), 
		float(width),
		float(height));

	set_tex_param("ssdm_m0",t1,fragment_prg,param1);
	set_tex_param("ssdm_m1",t2,fragment_prg,param2);
	set_tex_param("ssdm_m2",t3,fragment_prg,param3);
	set_tex_param("ssdm_m3",t4,fragment_prg,param4);
	set_tex_param("rgba_tex",rgba_tex,fragment_prg,param5);

	draw_fullscreen_quad();
	glDisable(GL_CULL_FACE);
	cgGLDisableProfile(vertexProfile);
	cgGLDisableProfile(fragmentProfile);
}

GLuint createPBO(int buffer_size) {

    // set up vertex data parameter
	GLuint pbo;
    // create buffer object
    glGenBuffers( 1, &pbo);
    glBindBuffer( GL_ARRAY_BUFFER, pbo);
    // buffer data
	cout << "glBufferData" <<  endl;
    glBufferData( GL_ARRAY_BUFFER, buffer_size, 0, GL_DYNAMIC_COPY);
    glBindBuffer( GL_ARRAY_BUFFER, 0);
	cout << "pboRegister" <<  endl;
	pboRegister(pbo);

    return pbo;
}


// This display function is called once pr frame 
void display()
{
	int width =screen.window_width;
	int height=screen.window_height;

	int lowres_width =width /4;
	int lowres_height=height/4;

	//resize(width,height);

	static FBO fbo_lowres(lowres_width,lowres_height,	FBO::ALPHA32F);
	static FBO fbo_hires (width,height,					FBO::RGBA8);
	static FBO fbo_ssdm  (width,height,					FBO::RGBA16F);
	static FBO fbo_rayout(width,height,					FBO::RGBA8);
	/*
	static FBO fbo_ssdm_final  (width,height,			FBO::RGBA8);
	static FBO fbo_ssdm_mip1(width/2,height/2,			FBO::RGBA8);
	static FBO fbo_ssdm_mip2(width/4,height/4,			FBO::RGBA8);
	static FBO fbo_ssdm_mip3(width/8,height/8,			FBO::RGBA8);
	*/

	// Lo-res
	fbo_lowres.enable();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	resize(lowres_width,lowres_height);
	glLoadIdentity();
	glRotatef(screen.rotx*180/M_PI,1,0,0);
	glRotatef(screen.roty*180/M_PI,0,1,0);
	glTranslatef(-0.5,-0.5,-0.5);
	raycasting_pass(true);
	fbo_lowres.disable();
	
	// Hi-res
	fbo_hires.enable();
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	resize(width,height);
	glLoadIdentity();
	glRotatef(screen.rotx*180/M_PI,1,0,0);
	glRotatef(screen.roty*180/M_PI,0,1,0);
	glTranslatef(-0.5,-0.5,-0.5);
	//GLfloat glmm[16];
	//glGetFloatv(GL_MODELVIEW, glmm);
	raycasting_pass(false,fbo_lowres.depth_tex);
	fbo_hires.disable();

	
	
	// SSDM
	fbo_ssdm.enable();
	matrix44 m;
	m.ident();
	m.rotate_x(-screen.rotx);
	m.rotate_y(-screen.roty);
	ssdm_pass(fbo_hires.color_tex,fbo_hires.depth_tex,m);
	fbo_ssdm.disable();
	
	fbo_rayout.enable();
	resize(width,height);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	reshape_ortho(screen.window_width,screen.window_height);
	draw_fullscreen_quad_2mio(fbo_hires.color_tex,fbo_ssdm.color_tex);
	glDisable(GL_TEXTURE_2D);
	fbo_rayout.disable();
	  
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	resize(width,height);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,fbo_lowres.depth_tex);
	reshape_ortho(screen.window_width,screen.window_height);
	//draw_fullscreen_quad();
	draw_fullscreen_quad_ssao(
		fbo_rayout.color_tex,
		fbo_rayout.depth_tex,
		fbo_lowres.color_tex);
	glDisable(GL_TEXTURE_2D);
	

/*	
	// SSDM mip1
	fbo_ssdm_mip1.enable();
	ssdm_mip_pass(fbo_ssdm.color_tex		,width/2,height/2);
	fbo_ssdm_mip1.disable();
	
	// SSDM mip2
	fbo_ssdm_mip2.enable();
	ssdm_mip_pass(fbo_ssdm_mip1.color_tex	,width/4,height/4);
	fbo_ssdm_mip2.disable();
	
	// SSDM mip3
	fbo_ssdm_mip3.enable();
	ssdm_mip_pass(fbo_ssdm_mip1.color_tex	,width/8,height/8);
	fbo_ssdm_mip3.disable();
	
	// SSDM final
	fbo_ssdm_final.enable();
	ssdm_final_pass(
		width,height,
		fbo_ssdm.color_tex,
		fbo_ssdm_mip1.color_tex,
		fbo_ssdm_mip2.color_tex,
		fbo_ssdm_mip3.color_tex,
		fbo_hires.color_tex
	);
	fbo_ssdm_final.disable();
*/	
	//render_buffer_to_screen();

	
	//glFlush();
	static int	pbo_handle=0;
	static bool	init = true;
	if (init)
	{
		cout << "createPBO" <<  endl;
		init=false;
		pbo_handle = 
			createPBO( 
				VOLUME_TEX_SIZE_X *
				VOLUME_TEX_SIZE_Y *
				VOLUME_TEX_SIZE_Z );
		cout << "createPBO finished" <<  endl;
	//}

	box_dist[0]=vec3f(
			screen.posx-box_pos[0].x,
			screen.posy-box_pos[0].y,
			screen.posz-box_pos[0].z).length();

	//{
		float scale = 1.0;
		int i = 0;
		if (vec3f(
			screen.posx-box_pos[i].x,
			screen.posy-box_pos[i].y,
			screen.posz-box_pos[i].z).length() > float(1<<i)*0.02 )
		{		
			cout << "cuda_main start" <<  endl;
			cuda_main( pbo_handle, 
				VOLUME_TEX_SIZE_X,
				VOLUME_TEX_SIZE_Y,
				VOLUME_TEX_SIZE_Z, i, 
				box_pos[i].x, box_pos[i].y, box_pos[i].z,
				screen.posx, screen.posy, screen.posz );
			cout << "cuda_main finished" <<  endl;

			glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, pbo_handle);
			glBindTexture( GL_TEXTURE_3D, volume_texture);
				
			glTexSubImage3D(
				  GL_TEXTURE_3D,
				  0,
				  0,
				  0,
				  0,
				  VOLUME_TEX_SIZE_X,
				  VOLUME_TEX_SIZE_Y,
				  VOLUME_TEX_SIZE_Z,
				  GL_LUMINANCE, 
				  GL_UNSIGNED_BYTE,
				  NULL );

			glBindTexture( GL_TEXTURE_3D, 0);

			box_pos[i].x = screen.posx;
			box_pos[i].y = screen.posy;
			box_pos[i].z = screen.posz;
			printf("box: %3.3f %3.3f %3.3f\n",
				screen.posx,screen.posy,screen.posz);
		}
	}
	//printf("scr: %3.3f %3.3f %3.3f\n",
	//	screen.posx,screen.posy,screen.posz);
	glutSwapBuffers();
}

void draw_line(float x1,float y1,float x2,float y2,uchar r,uchar g,uchar b,uchar z,Bmp& bmp,Bmp& bmp_z)
{
	if(x1>bmp.width) return;	if(x1<0) return;
	if(x2>bmp.width) return;	if(x2<0) return;
	if(y1>bmp.height) return;	if(y1<0) return;
	if(y2>bmp.height) return;	if(y2<0) return;
	
	int dx=x2-x1;
	int dy=y2-y1;
	int steps= ( abs(dx) > abs(dy) ) ? abs(dx) : abs(dy);

	if(steps==0){
		int x=x1;
		int y=y1;
		int o=3*(x+y*bmp.width);

		if(bmp_z.data[o+0]>z)
		{
			bmp.data[o+0]=b;
			bmp.data[o+1]=g;
			bmp.data[o+2]=r;
			bmp_z.data[o+0]=z;
		}
		return;
	}

	for (int a=0;a<steps;a++)
	{
		int x=x1+a*dx/steps;
		int y=y1+a*dy/steps;
		int o=3*(x+y*bmp.width);

		if(bmp_z.data[o+0]>z)
		{
			bmp.data[o+0]=b;
			bmp.data[o+1]=g;
			bmp.data[o+2]=r;
			bmp_z.data[o+0]=z;
		}
	}  
}

int main(int argc, char* argv[])
{
	if(0)
	{
		Bmp bmp( "test.bmp" );
		Bmp bmp_out( bmp.width,bmp.height,24,0 );
		Bmp bmp_z  ( bmp.width,bmp.height,24,0 );
		memset(bmp_z.data,255,bmp.width*bmp.height*3);
		//memcpy(bmp_out.data,bmp.data,bmp.width*bmp.height*3);

		for ( float x=0;x<1.0;x+=1.0f/float(bmp.width))
		for ( float y=0;y<1.0;y+=1.0f/float(bmp.height))
		{	
			int xi =int(float(x*float(bmp.width)));
			int yi =int(float(y*float(bmp.height)));
			int o  =3*(xi+yi*bmp.width);
			int b = bmp.data[o+0];
			int g = bmp.data[o+1];
			int r = bmp.data[o+2];

			int dx     = (r-128)/6;
			int dy     = (g-128)/6;
			int depth  =  b;
			/*
			int yii=bmp.height-1-yi;
			if(yii>620 && yii<630)
				if(xi>968)
					printf("xi:%d\tyi%d\tdx:%d\tdy:%d\tr:%d\tg:%d\n",xi,yi,dx,dy,r,g);
					*/

			
			draw_line(xi,yi,xi+dx,yi+dy,r,g,b,depth,bmp_out,bmp_z);
		}
		if(0)
		for ( float x=0;x<1.0;x+=1.0f/float(bmp.width))
		for ( float y=0;y<1.0;y+=1.0f/float(bmp.height))
		{	
			int xi =int(float(x*float(bmp.width)));
			int yi =int(float(y*float(bmp.height)));
			int o  =3*(xi+yi*bmp.width);
			int b = bmp.data[o+0];
			int g = bmp.data[o+1];
			int r = bmp.data[o+2];

			int dx     = (r-128)/6;
			int dy     = (g-128)/6;
			int depth  =  b;
			/*
			int yii=bmp.height-1-yi;
			if(yii>620 && yii<630)
				if(xi>968)
					printf("xi:%d\tyi%d\tdx:%d\tdy:%d\tr:%d\tg:%d\n",xi,yi,dx,dy,r,g);
					*/

			if(xi+dx<bmp.width)
			if(yi+dy<bmp.height)
			if(xi+dx>0)
			if(yi+dy>0)
			{
				o+=3*(dx+dy*bmp.width);
				bmp_out.data[o+0]=b;
				bmp_out.data[o+1]=g;
				bmp_out.data[o+2]=r;
			}  

		}
		if(0)
		for ( float a=0;a<6.2;a+=6.2/380.0f)
		{
			int xi=bmp.width/2;
			int yi=bmp.height/2;
			int dxi=float(bmp.width/2*sin(a-1)*a/16.2);
			int dyi=float(bmp.height/2*cos(a-1)*a/16.2);
			draw_line(xi,yi,xi+dxi,yi+dyi,255,255,0,0,bmp,bmp_z);
		}
		bmp_out.save("test_out.bmp");
		ShellExecute(
		  0,					//__in_opt  HWND hwnd,
		  "open",	//__in_opt  LPCTSTR lpOperation,
		  "C:/Program Files/IrfanView/i_view32.exe", //__in      LPCTSTR lpFile,
		  "test_out.bmp", //__in_opt  LPCTSTR lpParameters,
		  ".",//__in_opt  LPCTSTR lpDirectory,
		  SW_SHOW//__in      INT nShowCmd
		);   
		//while(1)Sleep(100);
		return 0;
	}

	//+float3(0.5,0.125,0.5)
	screen.posx=0;
	screen.posy=0;
	screen.posz=0;
	screen.rotx =screen.roty = screen.rotz = 0;
	screen.window_width  = SCREEN_SIZE_X;
	screen.window_height = SCREEN_SIZE_Y;

	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutCreateWindow("Perlin Noise Raycasting");
	glutReshapeWindow(SCREEN_SIZE_X,SCREEN_SIZE_Y);
	glutKeyboardFunc(key);
	glutKeyboardUpFunc(KeyboardUpCallback);
	glutMotionFunc(&MouseMotionStatic);
	glutPassiveMotionFunc(&MouseMotionStatic);

	wglSwapIntervalEXT(0);
	
	glutDisplayFunc(display);
	glutIdleFunc(idle_func);
	glutReshapeFunc(resize);
	resize(SCREEN_SIZE_X,SCREEN_SIZE_Y);
	init();
	glutMainLoop();
	return 0;
}

