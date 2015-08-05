class FBO {

	public:

	enum Type { RGBA8=1 ,ALPHA32F=2 , RGBA32F=3 ,ALPHA16F=4 , RGBA16F=5 };

	int color_tex;
	int color_bpp;
	int depth_tex;
	int depth_bpp;
	Type type;

	int width;
	int height;

	int tmp_viewport[4];

	FBO (int texWidth,int texHeight,Type type_in=RGBA8)//,Type type = Type(COLOR | DEPTH),int color_bpp=32,int depth_bpp=24)
	{
		color_tex = -1;
		depth_tex = -1;
		fbo = -1;
		dbo = -1;
		init (texWidth, texHeight, type_in);
	}

	void clear ()
	{		
		if(color_tex!=-1)
		{
			// destroy objects
			//glDeleteRenderbuffersEXT(1, &dbo);
			glDeleteFramebuffersEXT(1, &fbo);
			glDeleteTextures(1, (GLuint*)&color_tex);
			glDeleteTextures(1, (GLuint*)&depth_tex);
		}
	}

	void enable()
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
		//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
		//glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, dbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, 
			GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color_tex, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, 
			GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth_tex, 0);

		glGetIntegerv(GL_VIEWPORT, tmp_viewport);
		glViewport(0, 0, width, height);		// Reset The Current Viewport And Perspective Transformation
	}

	void disable()
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		//glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		glViewport(
			tmp_viewport[0],
			tmp_viewport[1],
			tmp_viewport[2],
			tmp_viewport[3]);
	}

	void init (int texWidth,int texHeight,Type type_in)// = Type(COLOR | DEPTH),int color_bpp=32,int depth_bpp=24)
	{
	//	clear ();
		this->width = texWidth;
		this->height= texHeight;
		this->type	= type_in;

		// init texture
		glGenTextures(1, (GLuint*)&color_tex);
		glBindTexture(GL_TEXTURE_2D, color_tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//GL_NEAREST);//GL_LINEAR);//
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);//GL_NEAREST);//GL_LINEAR);//
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// rgba8
		if(type==RGBA8)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 
				texWidth, texHeight, 0, 
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);//GL_RGBA
		// r float
		if(type==ALPHA32F)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY32F_ARB,//GL_ALPHA32F_ARB,//GL_RGBA8, 
				texWidth, texHeight, 0, 
				GL_LUMINANCE, GL_FLOAT, NULL);//GL_RGBA
		if(type==RGBA32F)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB,//GL_ALPHA32F_ARB,//GL_RGBA8, 
				texWidth, texHeight, 0, 
				GL_RGBA, GL_FLOAT, NULL);//GL_RGBA
		if(type==ALPHA16F)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY16F_ARB,//GL_ALPHA32F_ARB,//GL_RGBA8, 
				texWidth, texHeight, 0, 
				GL_LUMINANCE, GL_FLOAT, NULL);//GL_RGBA
		if(type==RGBA16F)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB,//GL_ALPHA32F_ARB,//GL_RGBA8, 
				texWidth, texHeight, 0, 
				GL_RGBA, GL_FLOAT, NULL);//GL_RGBA
		glBindTexture(GL_TEXTURE_2D, 0);
		get_error();

		glGenTextures(1, (GLuint*)&depth_tex);
		glBindTexture(GL_TEXTURE_2D, depth_tex);
		get_error();
		/*
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);*/
		glTexParameteri (GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 
			texWidth, texHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		// create a framebuffer object
		glGenFramebuffersEXT(1, &fbo);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

		// attach two textures to FBO color0 and depth attachement points
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth_tex, 0);	
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color_tex, 0);

		get_error();

		check_framebuffer_status();
	    
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	}

	private:

	GLuint fbo; // frame buffer object ref
	GLuint dbo; // depth buffer object ref

	void get_error()
	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) 
		{
			printf("GL FBO Error: %s\n",gluErrorString(err));
			printf("Programm Stopped!\n");
			while(1);;
		}
	}

	void check_framebuffer_status()
	{
		GLenum status;
		status = (GLenum) glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		switch(status) {
			case GL_FRAMEBUFFER_COMPLETE_EXT:
				return;
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
				printf("Unsupported framebuffer format\n");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
				printf("Framebuffer incomplete, missing attachment\n");
				break;
//			case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
//				printf("Framebuffer incomplete, duplicate attachment\n");
//				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
				printf("Framebuffer incomplete, attached images must have same dimensions\n");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
				printf("Framebuffer incomplete, attached images must have same format\n");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
				printf("Framebuffer incomplete, missing draw buffer\n");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
				printf("Framebuffer incomplete, missing read buffer\n");
				break;
			case 0:
				printf("Not ok but trying...\n");
				return;
				break;
			default:;
				printf("Framebuffer error code %d\n",status);
				break;
		};
		printf("Programm Stopped!\n");
		while(1)Sleep(100);;
	}
};
			/*
			//Use generated mipmaps if supported
			if(GLEE_SGIS_generate_mipmap)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, true);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
			}

			//Use maximum anisotropy if supported
			if(GLEE_EXT_texture_filter_anisotropic)
			{
				GLint maxAnisotropy=1;
				glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
			}
			*/
/*
	// create a texture object for depth
	glGenTextures(1, &depthTex);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ImageW, ImageH, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
	

	// create a texture object for color
	glGenTextures(1, &colorTex);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ImageW, ImageH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);


	// create a framebuffer object
	glGenFramebuffersEXT(1, &fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
	
	// attach two textures to FBO color0 and depth attachement points
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depthTex, 0);	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, colorTex, 0);

	// check FBO status
	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
		cerr &lt;&lt; "FBO incomplete!!  Code:" &lt;&lt; status &lt;&lt; endl;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

*/

		/*
		glGenRenderbuffersEXT(1, &dbo);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, dbo);


			glGenTextures(1, (GLuint*)&depth_tex);
			glBindTexture(GL_TEXTURE_2D, depth_tex);
			//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 
			//	texWidth, texHeight, 0, 
			//	GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			get_error();
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTexParameteri (GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


			glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 
				texWidth, texHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, 
								GL_TEXTURE_2D, depth_tex, 0);

			get_error();
			glBindTexture(GL_TEXTURE_2D, 0);// don't leave this texture bound or fbo (zero) will use it as src, want to use it just as dest GL_DEPTH_ATTACHMENT_EXT

			*/
		//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
