////////////////////////////////////////////////////////////////////////
// Helper... select the good function
////////////////////////////////////////////////////////////////////////
typedef void (*t_getshadetrans_func)(uint32_t *, uint32_t);

t_getshadetrans_func GetShadeTransCol32Func();
t_getshadetrans_func GetTextureTransColG32Func();


#define __ClampRGB(r,g,b)	{			\
	if(r&0x7FE00000) r=0x1f0000|(r&0xFFFF);		\
	if(r&0x7FE0)     r=0x1f    |(r&0xFFFF0000); \
	if(b&0x7FE00000) b=0x1f0000|(b&0xFFFF);		\
	if(b&0x7FE0)     b=0x1f    |(b&0xFFFF0000);	\
	if(g&0x7FE00000) g=0x1f0000|(g&0xFFFF);		\
	if(g&0x7FE0)     g=0x1f    |(g&0xFFFF0000);	\
}	


// Old function
void GetShadeTransCol32(uint32_t * pdest,uint32_t color);
void GetTextureTransColG32(uint32_t * pdest,uint32_t color);