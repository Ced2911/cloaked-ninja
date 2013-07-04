#include <GL/glew.h>
#include <GL/gl.h>
#include "stdafx.h"
#include "externals.h"

GLuint loadtga(char* filename, bool mipmap) {

	unsigned char	header[12] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char	bpp = 32;
	unsigned char	id = 8;
	unsigned short	width;
	unsigned short	height;
	unsigned char	*pPixels = NULL;
	FILE	*fp = NULL;

	fp = fopen(filename,"r");

	fread(header,sizeof(unsigned char),12,fp);
	fread(&width,sizeof(unsigned short),1,fp);
	fread(&height,sizeof(unsigned short),1,fp);
	fread(&bpp,sizeof(unsigned char),1,fp);
	fread(&id,sizeof(unsigned char),1,fp);

	pPixels = new unsigned char[width * height * 4];
	fread(pPixels, 4, width * height, fp);
	fclose(fp);

	GLuint texName = -1;
	//glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1,&texName);
	glBindTexture(GL_TEXTURE_2D,texName);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,
		0,GL_BGRA,GL_UNSIGNED_BYTE,pPixels);

	delete pPixels;

	return texName;

}