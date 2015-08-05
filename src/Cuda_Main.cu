#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/glut.h>

#define M_PI 3.141592653589793238462643

int timeGetTime(){return 0;};

#define IN_CUDA_ENV
//#ifdef _WIN32
//#  define WINDOWS_LEAN_AND_MEAN
//#  include <windows.h>
//#endif
#include <cutil.h>
#include <cuda_gl_interop.h>
#include "cutil_math.h"
////////////////////////////////////////////////////////////////////////////////
#define C_CHECK_GL_ERROR() ChkGLError(__FILE__, __LINE__)
////////////////////////////////////////////////////////////////////////////////
extern "C" bool pboRegister(int pbo);
extern "C" void pboUnregister(int pbo);
extern "C" void* cuda_main( int pbo_out, 
						   int size_x, 
						   int size_y, 
						   int size_z, 
						   int level, 
						   float box_x,float box_y,float box_z,
						   float pos_x,float pos_y,float pos_z );
////////////////////////////////////////////////////////////////////////////////
#define THREAD_COUNT_X 16
#define THREAD_COUNT_Y 16
#define THREAD_COUNT_Z 1
#define uchar unsigned char  
////////////////////////////////////////////////////////////////////////////////
void gpu_memcpy(void* dst, void* src, int size)
{
	CUDA_SAFE_CALL( cudaMemcpy( dst, src, size, cudaMemcpyHostToDevice) );
	CUT_CHECK_ERROR("cudaMemcpy cudaMemcpyHostToDevice failed");
}
////////////////////////////////////////////////////////////////////////////////
void cpu_memcpy(void* dst, void* src, int size)
{
	CUDA_SAFE_CALL( cudaMemcpy( dst, src, size, cudaMemcpyDeviceToHost) );
	CUT_CHECK_ERROR("cudaMemcpy cudaMemcpyDeviceToHost failed");
}
////////////////////////////////////////////////////////////////////////////////
void* gpu_malloc(int size)
{
	void* ptr=0;	
	CUDA_SAFE_CALL( cudaMalloc( (void**) &ptr, size ) );
	CUT_CHECK_ERROR("cudaMalloc failed");
	if(ptr==0){printf("\ncudaMalloc %d MB: out of memory error\n",(size>>20));while(1);;}
	return ptr;
}
////////////////////////////////////////////////////////////////////////////////
texture<uchar, 3, cudaReadModeNormalizedFloat> texRnd;  // 3D texture
texture<uchar, 3, cudaReadModeNormalizedFloat> texBrush;  // 3D texture
cudaArray *d_rnd_volumeArray = 0;
cudaArray *d_brush_volumeArray = 0;
////////////////////////////////////////////////////////////////////////////////
bool init_rnd_texture(int size)
{
	uchar *h_volume = new uchar [size*size*size];

	for(int x = 0; x < size; x++)
	{
	 printf("slice:%d               \r"	,x);
	 for(int y = 0; y < size; y++)
	{for(int z = 0; z < size; z++)
	{
		h_volume[(x)+ (y * size ) + (z * size * size )] = rand()&255;
	}}}

	cudaExtent volumeSize = make_cudaExtent(size, size, size);

    // create 3D array
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar>();
    cudaMalloc3DArray(&d_rnd_volumeArray, &channelDesc, volumeSize) ;

    // copy data to 3D array
    cudaMemcpy3DParms copyParams = {0};
    copyParams.srcPtr   = make_cudaPitchedPtr((void*)h_volume, volumeSize.width*sizeof(uchar), volumeSize.width, volumeSize.height);
    copyParams.dstArray = d_rnd_volumeArray;
    copyParams.extent   = volumeSize;
    copyParams.kind     = cudaMemcpyHostToDevice;
    cudaMemcpy3D(&copyParams) ;

    // set texture parameters
    texRnd.normalized = true;                      // access with normalized texture coordinates
    texRnd.filterMode = cudaFilterModeLinear;      // linear interpolation
    texRnd.addressMode[0] = cudaAddressModeWrap;   // wrap texture coordinates
    texRnd.addressMode[1] = cudaAddressModeWrap;
    texRnd.addressMode[2] = cudaAddressModeWrap;

    // bind array to 3D texture
    cudaBindTextureToArray(texRnd, d_rnd_volumeArray, channelDesc);

	delete []h_volume;

	return true;
}
////////////////////////////////////////////////////////////////////////////////
__device__ int getTerrainVal( 
				  int size_x, int size_y, int size_z, 
				  int x, int y , int z, float scale,
				  float box_x,float box_y,float box_z,
				  float p_x,float p_y,float p_z  )
{
	//p_x = p_y = p_z = 0;
	p_x /= scale;
	p_y /= scale;
	p_z /= scale;
	float add_x = int(p_x+1000);
	float add_y = int(p_y+1000);
	float add_z = int(p_z+1000);
	float frac_x= p_x + 1000 - add_x;
	float frac_y= p_y + 1000 - add_y;
	float frac_z= p_z + 1000 - add_z;
	add_x -= 1000;
	add_y -= 1000;
	add_z -= 1000;

	float x01   = float(x) /  size_x ;
	float y01   = float(y) /  size_y ;
	float z01   = float(z) / (size_z-1) ;

	if (frac_x!=0) if (x01<frac_x) x01+= 1;
	if (frac_y!=0) if (y01<frac_y) y01+= 1;
	if (frac_z!=0) if (z01<frac_z) z01+= 1;

	x01+=add_x;
	y01+=add_y;
	z01+=add_z;

	float xf_in = x01-0.5; xf_in = xf_in*scale;
	float yf_in = y01-0.5; yf_in = yf_in*scale * 0.25;
	float zf_in = z01-0.5; zf_in = zf_in*scale;

	/*
	if ( xf_in > -box_x - 0.5 )
	if ( xf_in < -box_x + 0.5 ) 
	if ( yf_in > -box_y - 0.5 )
	if ( yf_in < -box_y + 0.5 ) 
	if ( zf_in > -box_z - 0.5 )
	if ( zf_in < -box_z + 0.5 ) return -1;
	*/
							   		
	float v=0.0;

	for (float a=0.3,b=0.6;b>0.01;a*=2,b*=0.5)
	{
		v += tex3D(texRnd,0.5+ xf_in*a+a*999, yf_in*a, zf_in*a) * b;
	}
	v += -yf_in*8; 
//	v=0;
	

	if(0)
	for (float objx = -0.1 ; objx <= 0.1 ; objx+=0.02 )
	for (float objz = -0.1 ; objz <= 0.1 ; objz+=0.02 )
	{
		float sinx=sin(objx*327.5);
		float cosx=cos(objx*437.5);
		float sinz=sin(objz*455.9);
		float cosz=cos(objz*655.9);

		float obj_size = 16.0 / 1.0;//tex3D(texRnd, objx * 3.4 , objz * 2.6, 0.35 )*1.0+1.0;

		float xfa = xf_in	+ objx;
		float yfa = yf_in	;//+ 0.25;
		float zfa = zf_in 	+ objz;

		if ( xfa*xfa+yfa*yfa+zfa*zfa > 0.5 ) continue;

		xfa *= obj_size;
		yfa *= obj_size;
		zfa *= obj_size;

		float xfb = xfa * cosx - zfa * sinx;
		float yfb = yfa;
		float zfb = zfa * cosx + xfa * sinx;

		float xf = xfb * sinz - yfb * cosz	;
		float yf = yfb * sinz + xfb * cosz	;
		float zf = zfb						;

//		v = max( v, 1.0-sqrt(xf*xf+yf*yf+zf*zf)*2.0);
		v = max( v, tex3D(texBrush, 2.0*xf+0.5, yf+0.5, zf+0.5) );
//		v +=  tex3D(texBrush, xf+0.5, yf+0.5, zf+0.5) ;
	}					   		
		
	return float( min ( max ( v * 255.0f , 0.0f ), 255.0f )); 
}
////////////////////////////////////////////////////////////////////////////////
__global__ void
cudaTerrainKernel( unsigned int* data, 
				  int size_x , 
				  int size_y , 
				  int size_z , 
				  float anim , float scale,
				  float box_x,float box_y,float box_z,
				  float p_x,float p_y,float p_z  )
{
    extern __shared__ int sdata[];
   
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

	int ofs = x+y*(size_x/4);
	int add = size_x*size_y/4;

	for (int z=0;z<size_z-1;z++)
	{
		unsigned int d0 = getTerrainVal( size_x,size_y,size_z, x*4+0, y, z, scale, box_x,box_y,box_z, p_x,p_y,p_z ); //sin(anim*x0)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		unsigned int d1 = getTerrainVal( size_x,size_y,size_z, x*4+1, y, z, scale, box_x,box_y,box_z, p_x,p_y,p_z ); //tex3D(tex, x1, y0, z0); //( sin(anim*x1)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		unsigned int d2 = getTerrainVal( size_x,size_y,size_z, x*4+2, y, z, scale, box_x,box_y,box_z, p_x,p_y,p_z ); //tex3D(tex, x2, y0, z0); //( sin(anim*x2)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		unsigned int d3 = getTerrainVal( size_x,size_y,size_z, x*4+3, y, z, scale, box_x,box_y,box_z, p_x,p_y,p_z ); //tex3D(tex, x3, y0, z0); //( sin(anim*x3)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;

		data[ ofs ] = d0 + (d1<<8) + (d2<<16) + (d3 << 24);

		ofs += add;
	}
	data[ ofs ] = data[ x+y*(size_x/4) ];
   
	return;
}
////////////////////////////////////////////////////////////////////////////////
__device__ int getSmoothVal( int size, unsigned int* in_data, int ofs)
{	
	uchar *data = (uchar*) in_data;

	int v = 0;

	for (int x = -1 ; x <= 1 ; x++ )
	for (int y = -1 ; y <= 1 ; y++ )
	for (int z = -1 ; z <= 1 ; z++ )
		v+= data[ofs+x+y*size+z*size*size];

	return v/27;
}
////////////////////////////////////////////////////////////////////////////////
__global__ void
cudaSmoothKernel( unsigned int* out_data, unsigned int* in_data, int size)
{
    extern __shared__ int sdata[];
   
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

	int ofs = x+y*(size/4);
	int add = size*size/4;

	if ( x==0 || y==0 || x==size-1 || y==size-1 )
	{
		for (int z=0;z<size;z++)
		{
			out_data[ ofs ] = in_data[ ofs ] ;
			ofs += add;
		}
		return ;
	}

	for (int z=0;z<size;z++)
	{
		if (z>0 && z<size-1 )
		{
			int d0 = getSmoothVal( size, in_data , ofs*4+0);
			int d1 = getSmoothVal( size, in_data , ofs*4+1);
			int d2 = getSmoothVal( size, in_data , ofs*4+2);
			int d3 = getSmoothVal( size, in_data , ofs*4+3);
			out_data[ ofs ] = d0 + (d1<<8) + (d2<<16) + (d3 << 24);
		}
		else
			out_data[ ofs ] = in_data[ ofs ] ;

		ofs += add;
	}
   
	return;
}
////////////////////////////////////////////////////////////////////////////////
__device__ int getBrushVal( int ofs, int size, int x, int y , int z , float anim, unsigned char* data)
{
	float xf_in = 2*float(x) / size - 1.0 ; 
	float yf_in = 2*float(y) / size - 1.0 ; 
	float zf_in = 2*float(z) / size - 1.0 ; 
	float v = 0.0;//max ( 0.0 , 0.05-yf_in );

	//for (float objx = -0.2 ; objx <= 1.2 ; objx+=0.3 )
	//for (float objz = -0.2 ; objz <= 1.2 ; objz+=0.3 )

	float objx = 0.0;
	float objz = 0.0;
	{
		float sinx=sin(objx*7.5);
		float cosx=cos(objx*7.5);
		float sinz=sin(objz*5.9);
		float cosz=cos(objz*5.9);

		float obj_size = 0.7;//tex3D(texRnd, objx * 3.4 , objz * 2.6, 0.35 )*1.0+1.0;

		float xfa = xf_in + objx	+ tex3D(texRnd, objx*4.4 , objz*24.0 , 0.5 )*0.4;
		float yfa = yf_in			- tex3D(texRnd, objx*2.4 , objz*54.0 , 0.5 )*0.9 + 0.8;
		float zfa = zf_in + objz	+ tex3D(texRnd, objx*7.4 , objz*74.0 , 0.5 )*0.4;

		float xfb = xfa * cosx - zfa * sinx;
		float yfb = yfa;
		float zfb = zfa * cosx + xfa * sinx;

		float xf = xfb * sinz - yfb * cosz;
		float yf = yfb * sinz + xfb * cosz;
		float zf = zfb;
						   
		float len = sqrt ( xf*xf + yf*yf + zf*zf );

		//if (len > 0.2/(obj_size*0.3)) continue;

		float nx  = abs(xf) / len;
		float ny  = abs(yf) / len;
		float nz  = abs(zf) / len;

		float az = abs(atan2( xf,yf ));
		float ay = abs(atan2( xf,zf ));
		float ax = abs(atan2( yf,zf ));

		float axz = 0.2;
		float axy = 0.2;
		float ayz = 0.2;
		
		for (float a=0.003,b=0.75;a<1.4;a*=2.0,b*=0.5)
		{
			axz += tex3D(texRnd, objx*3+ax*a,objz*3+az*a		,1.0-objx*3.23	)*b;
			axy += tex3D(texRnd, objx*3+ax*a,objz*3+ay*a		,objz*1.234		)*b;
			ayz += tex3D(texRnd, objx*3+ay*a,objz*3+az*a		,objx+objz*5.22 )*b;
		}
															  
		axz*=tex3D(texRnd, objx*14.4 , objz*15.0 , 0.4 )*2+0.5;
		axy*=tex3D(texRnd, objx*44.4 , objz*74.0 , 0.5 )*2+0.5;
		ayz*=tex3D(texRnd, objx*34.4 , objz*11.0 , 0.7 )*2+0.5;
		

		float displace = 
			max( ayz*(nx*1.0-0.0) , 0.0 ) +
			max( axz*(ny*1.0-0.0) , 0.0 ) +
			max( axy*(nz*1.0-0.0) , 0.0 ) ;
			
	//	v = max( v , 1.0-(1.0-displace*0.8) * obj_size * len );
		float len2 = sqrt ( xf*xf*ayz*ayz + yf*yf*axz*axz + zf*zf*axy*axy );
		v = max( v , 1.0-obj_size * len2 );
		//v = min( v , max(1.0-obj_size * len2,0.0) );
	}

	int vi = float( min ( max ( v * 255.0f , 0.0f ), 255.0f )); 

	return vi;
}
////////////////////////////////////////////////////////////////////////////////
__global__ void
cudaGenBrushKernel( int* data, int size , float anim )
{
    extern __shared__ int sdata[];
   
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

	int ofs = x+y*(size/4);
	int add = size*size/4;

	for (int z=0;z<size;z++)
	{
		int d0 = getBrushVal(ofs, size, x*4+0, y, z, anim, (unsigned char *)data); //sin(anim*x0)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		int d1 = getBrushVal(ofs, size, x*4+1, y, z, anim, (unsigned char *)data); //tex3D(tex, x1, y0, z0); //( sin(anim*x1)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		int d2 = getBrushVal(ofs, size, x*4+2, y, z, anim, (unsigned char *)data); //tex3D(tex, x2, y0, z0); //( sin(anim*x2)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;
		int d3 = getBrushVal(ofs, size, x*4+3, y, z, anim, (unsigned char *)data); //tex3D(tex, x3, y0, z0); //( sin(anim*x3)+sin(anim+y0)+sin(anim+z0) ) * (1.0f/6.0f) + 0.5;

		((unsigned int *)data)[ ofs ] = d0 + (d1<<8) + (d2<<16) + (d3 << 24);

		ofs += add;
	}
   
	return;
}

////////////////////////////////////////////////////////////////////////////////
bool init_brush_texture(int size)
{
	int* h_volume = (int*)gpu_malloc(size*size*size);

	dim3 threads(THREAD_COUNT_X,THREAD_COUNT_Y,1 );
    dim3 grid(	size/(4*threads.x) , size/threads.y ,1 	);

	cudaGenBrushKernel<<< grid, threads, 0>>>	( h_volume , size , 0 );

	cudaExtent volumeSize = make_cudaExtent(size, size, size);

    // create 3D array
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar>();
    cudaMalloc3DArray(&d_brush_volumeArray, &channelDesc, volumeSize) ;

    // copy data to 3D array
    cudaMemcpy3DParms copyParams = {0};
    copyParams.srcPtr   = make_cudaPitchedPtr((void*)h_volume, volumeSize.width*sizeof(uchar), volumeSize.width, volumeSize.height);
    copyParams.dstArray = d_brush_volumeArray;
    copyParams.extent   = volumeSize;
    copyParams.kind     = cudaMemcpyDeviceToDevice;
    cudaMemcpy3D(&copyParams) ;

    // set texture parameters
    texBrush.normalized = true;                      // access with normalized texture coordinates
    texBrush.filterMode = cudaFilterModeLinear;      // linear interpolation
    texBrush.addressMode[0] = cudaAddressModeClamp;   // wrap texture coordinates
    texBrush.addressMode[1] = cudaAddressModeClamp;
    texBrush.addressMode[2] = cudaAddressModeClamp;

    // bind array to 3D texture
    cudaBindTextureToArray(texBrush, d_brush_volumeArray, channelDesc);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
extern "C" void* cuda_main( int pbo_out, 
						   int size_x, 
						   int size_y, 
						   int size_z, 
						   int level, 
						   float box_x,float box_y,float box_z,
						   float pos_x,float pos_y,float pos_z )
{
	static bool init1		= init_rnd_texture( 128 );
	static bool init2		= init_brush_texture( 128 );
	if(pbo_out==0) return NULL;
	
	int t0 = timeGetTime();

    unsigned int* out_data;   
    CUDA_SAFE_CALL(cudaGLMapBufferObject( (void**)&out_data, pbo_out));   
	if(out_data==0) return NULL;

	dim3 threads(THREAD_COUNT_X,THREAD_COUNT_Y,1 );
    dim3 grid	(size_x/(4*threads.x) , size_y/threads.y ,1 	);

	float anim = float((timeGetTime()>>4)&1023)*2*M_PI / 1024;

	cudaTerrainKernel<<< grid, threads, 0>>> ( 
		out_data, 
		size_x , 
		size_y, 
		size_z , 
		anim, float(int(1<<level)),
		box_x,box_y,box_z,
		pos_x,pos_y,pos_z );
	
	CUT_CHECK_ERROR("cudaRender failed");

	CUDA_SAFE_CALL( cudaThreadSynchronize() );
	int t2 = timeGetTime();

	printf ("cuda terrain time=%d , scale=%d  \n",t2-t0,int(1<<level));
    
   CUDA_SAFE_CALL(cudaGLUnmapBufferObject( pbo_out));

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
bool pboRegister(int pbo)
{
    // register this buffer object with CUDA
    CUDA_SAFE_CALL(cudaGLRegisterBufferObject(pbo));
	CUT_CHECK_ERROR("cudaGLRegisterBufferObject failed");
//	C_CHECK_GL_ERROR();
	return true;
}
////////////////////////////////////////////////////////////////////////////////
void pboUnregister(int pbo)
{
    // unregister this buffer object with CUDA
    CUDA_SAFE_CALL(cudaGLUnregisterBufferObject(pbo));	
	CUT_CHECK_ERROR("cudaGLUnregisterBufferObject failed");
//	C_CHECK_GL_ERROR();
}
////////////////////////////////////////////////////////////////////////////////


	/*
	float v=0;
	for (float a=0.04,b=0.5;a<0.8;a*=2.0,b*=0.5)
	{
		v = v + tex3D(tex, xf*a+a*sin(yf*14)*0.03, yf*a, zf*a+a*cos(yf*14)*0.03) * b * 0.8;
		//float t=tex3D(tex, xf*a+a*sin(yf*14)*0.03, yf*a, zf*a+a*cos(yf*14)*0.03);
		
		v = v + (cos(xf*a*1600)*0.5+cos(yf*a*600)+cos(zf*a*1400)*0.5+3)*b*0.3333*0.5*0.2;

//		v = v + tex3D(tex, xf*a, yf*a, zf*a) * b;
	}
	*/

