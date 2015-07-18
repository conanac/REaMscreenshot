
#include "global.h"

#define REG(x)   		(*(volatile u32*)(x))
#define SET32(x,y,o)	x=(u32) (y+o)
#define VAL32(x,y) 		*(u32* )(x)=y
#define SET16(x,y,o) 	x=(u16) (y+o)
#define VAL16(x,y) 		*(u16* )(x)=y

typedef enum
{
	GSP_RGBA8_OES=0, 	//pixel_size = 4-bytes
	GSP_BGR8_OES=1, 	//pixel_size = 3-bytes
	GSP_RGB565_OES=2, 	//pixel_size = 2-bytes
	GSP_RGB5_A1_OES=3, 	//pixel_size = 2-bytes
	GSP_RGBA4_OES=4 	//pixel_size = 2-bytes
} GSP_FramebufferFormats;

typedef struct
{
	u32 addr[2];
	GSP_FramebufferFormats format;
	u32 size;
	u32 leftright;
} Screen_CaptureInfo;

Handle fsUserHandle;
FS_archive sdmcArchive = {0x9, (FS_path){PATH_EMPTY, 1, (u8*)""}};

u32 IoBaseLcd;
u32 IoBasePad;
u32 IoBasePdc;
u32 GSPOffset;
int isNewNtr;

u32 PtoV;
u32 bmp_idx = 0;
u8* buf;
u32 g_arm11_param1, g_arm11_param2, g_arm11_param3, g_arm11_cmd;

void lcd_solid_fill(u32 fillcount, u32 fillcolor) {
	u32 i;
	
	for (i=0; i < fillcount; ++i ) {
		*(u32*) (IoBaseLcd+0x204) = fillcolor;
		svc_sleepThread(5000000); // 0.005 second
	}
	*(u32*) (IoBaseLcd+0x204) = 0;
}	

Result open_file(u8* filename, u32 mode) {
	Handle fhandle;
	FS_path fpath;
	u32 retc;
	
	fpath.type = (FS_pathType) PATH_CHAR;
	fpath.size = (u32) (strlen(filename)+1);
	fpath.data = filename;
	Result retf = FSUSER_OpenFileDirectly(fsUserHandle, &fhandle, sdmcArchive, fpath, mode, FS_ATTRIBUTE_NONE); 
	if (retf >= 0) {
		retc = fhandle;
	} else {
		retc = retf;
	}
	return retc;
}

void close_file(Handle fhandle) {
	
	FSFILE_Close(fhandle);
}

Result memdump_to_file(Handle fhandle, u64 offset, u8* data, u32 filesize) {
	Result retc, retv;
	u32 byteswritten;
		
	retc = FSFILE_Write(fhandle, &byteswritten, offset, (u32* ) data, filesize, 0);
	if (retc >= 0) {
		retv = byteswritten;
	}
	else {
		retv = retc;
	}
	return retv;
}

void converttorgb(u8 *target, u32 width, u32 height, u8 *src, u32 rowpixelsize, u32 format) {  
  u32 byteperpixel,i,j,merge;
  GSP_FramebufferFormats type;
  u32 d;
  u32 s;
  
  if ( format & 0xF )
  {
    if ( (format & 0xF) == GSP_BGR8_OES )
      byteperpixel = 3; 
    else
      byteperpixel = 2;
  }
  else
  {
    byteperpixel = 4;
  }
  type = format & 0xF; 
  for ( i = 0; i < height; ++i )
  {
    d = (u32) (target + 3 * width * i); 
    s = (u32) (src + byteperpixel * i); 
    for ( j = 0; j < width; ++j )
    {
		switch ( type ) {
			case GSP_BGR8_OES:
				*(u8* )d = *(u8* )s;
				*(u8* )(d + 1) = *(u8* )(s + 1);
				*(u8* )(d + 2) = *(u8* )(s + 2);
				break;
			case GSP_RGB565_OES:	  
				merge = (*(u8* )(s + 1) << 8) | *(u8* )s;
				*(u8* )d = 8 * *(u8* )s & 0xF8;
				*(u8* )(d + 1) = (merge >> 3) & 0xFC;
				*(u8* )(d + 2) = *(u8* )(s + 1) & 0xF8;
				break;
			case GSP_RGB5_A1_OES:
				merge = (*(u8* )(s + 1) << 8) | *(u8* )s;
				*(u8* )d = 4 * *(u8* )s & 0xF8;
				*(u8* )(d + 1) = (merge >> 3) & 0xF8;
				*(u8* )(d + 2) = *(u8* )(s + 1) & 0xF8;
				break;
			case GSP_RGBA4_OES:
				merge = (*(u8* )(s + 1) << 8) | *(u8* )s;
				*(u8* )d = *(u8* )s & 0xF0;
				*(u8* )(d + 1) = (merge >> 4) & 0xF0;
				*(u8* )(d + 2) = *(u8* )(s + 1) & 0xF0;
				break;
			default:
				*(u8* )d = *(u8* )s;
				*(u8* )(d + 1) = *(u8* )(s + 1);
				*(u8* )(d + 2) = *(u8* )(s + 2);
				break;
		}
		d += 3;
		s += rowpixelsize;
    }
  }
}

void write_to_file(u8* data, u32 width, u32 height, u8* filename) {
	u32 filesize, offset32;
	Result retc, retm;
	Handle fhandle;

	filesize = 3*height*width + 0x36;
	memset(data, 0, 0x36);
	*(data) = 0x42;
	*(data+1) = 0x4D;

	SET32(offset32,data,2);
	VAL32(offset32,filesize);
	SET32(offset32,data,10);
	VAL32(offset32,0x36);
	SET32(offset32,data,14);
	VAL32(offset32,0x28);
	SET32(offset32,data,18);
	VAL32(offset32,width);
	SET32(offset32,data,22);
	VAL32(offset32,height);
	SET32(offset32,data,26);
	VAL32(offset32,0x180001);
	SET32(offset32,data,34);
	VAL32(offset32,3*height*width);
	SET32(offset32,data,38);
	VAL32(offset32,0xB12);
	SET32(offset32,data,42);
	VAL32(offset32,0xB12);
	
	retc = open_file(filename, FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE);
	if ( retc > 0) {
		fhandle = retc;
		retm = memdump_to_file(fhandle, 0, data, filesize);
		close_file(fhandle); 
	}	
}

void svc_backDoor(void* codeaddress);

void dispatch_arm11_kernel_cmd() {
  u32 cmd, src, dst, count, i;
  
  cmd = g_arm11_cmd;
  if(cmd == 1) { // set only for kernel memory copy command
	i = 0;
	src = g_arm11_param2;
    dst = g_arm11_param1;
    count = g_arm11_param3;
    while(i < count) {
		*(u8*) (dst+i) = *(u8*) (src+i);
		i = i+1; 
	} 
  }
}

void cb_arm11_dispatch()
{
  __asm__ (
    "MRS		R0, CPSR;"
    "ORR		R0, R0, #0xC0;"
    "MSR     	CPSR_cf, R0;"
  );
  dispatch_arm11_kernel_cmd();
}

void arm11k_memcpy(void* dst, void* src, u32 count) {
  
  g_arm11_param1 = (u32) dst;
  g_arm11_param2 = (u32) src;
  g_arm11_param3 = count;
  g_arm11_cmd = 1;
  svc_backDoor(&cb_arm11_dispatch);
}
 
s32 create_screenshot() {
	Screen_CaptureInfo top, bottom;
	u32 topsrc, bottomsrc;
	u8* target = NULL;
	u8 bufx[200];
	u32 toptarget, topshift, toprowpixelsize, topformat;
	u32 bottomtarget, bottomshift, bottomrowpixelsize, bottomformat;
	
	target = buf;	
	if (target) {
		nsDbgPrint("target address: %08x\n", (u32) target);
		// bottom first to avoid ACNL problem with acquire and release video of bottom screen in ntr.bin
		bottom.addr[0] = REG(IoBasePdc+0x568) + PtoV;
		bottom.addr[1] = REG(IoBasePdc+0x56C) + PtoV;
		bottom.format = GSP_BGR8_OES;
		bottom.size = 0x38400;
		bottom.leftright = REG(IoBasePdc+0x578) & 0x1;
		bottomsrc = *(bottom.addr + bottom.leftright);
		nsDbgPrint("bottom screen address: %08x\n", (u32) bottomsrc);
		bottomtarget = (u32) (target+0x50000);
		if (isNewNtr) {
			arm11k_memcpy((u8*) (bottomtarget), (u8*) (bottomsrc), bottom.size*(sizeof(u8)));
		} else {
			memcpy((u8*) (bottomtarget), (u8*) (bottomsrc), bottom.size*(sizeof(u8)));	
		};	
		bottomshift = (u32) (target+0x36);
		bottomrowpixelsize = REG(IoBasePdc+0x590);
		bottomformat = REG(IoBasePdc+0x570);
		nsDbgPrint("bottom row pixel size: %08x\n", bottomrowpixelsize);
		nsDbgPrint("bottom format: %08x\n", bottomformat);
		converttorgb((u8*) (bottomshift), 320, 240, (u8*) (bottomtarget), bottomrowpixelsize, bottomformat);
		xsprintf(bufx, "/bottom_%04d.bmp", bmp_idx);
		write_to_file(target, 320, 240, bufx);	
		
		// top
		top.addr[0] = REG(IoBasePdc+0x468) + PtoV;
		top.addr[1] = REG(IoBasePdc+0x46C) + PtoV;
		top.format = GSP_BGR8_OES;
		top.size = 0x46500;
		top.leftright = REG(IoBasePdc+0x478) & 0x1;
		topsrc = *(top.addr + top.leftright);
		nsDbgPrint("top screen address: %08x\n", (u32) topsrc);
		toptarget = (u32) (target+0x50000);
		if (isNewNtr) {
			arm11k_memcpy((u8*) (toptarget), (u8*) (topsrc), top.size*(sizeof(u8)));
		} else {
			memcpy((u8*) (toptarget), (u8*) (topsrc), top.size*(sizeof(u8)));	
		};
		topshift = (u32) (target+0x36);
		toprowpixelsize = REG(IoBasePdc+0x490);
		topformat = REG(IoBasePdc+0x470);
		nsDbgPrint("top row pixel size: %08x\n", toprowpixelsize);
		nsDbgPrint("top format: %08x\n", topformat);
		converttorgb((u8*) (topshift), 400, 240, (u8*) (toptarget), toprowpixelsize, topformat);
		xsprintf(bufx, "/top_%04d.bmp", bmp_idx);
		write_to_file(target, 400, 240, bufx);
		
		lcd_solid_fill(0x64, 0x1FF00FF);		
		++bmp_idx;
		nsDbgPrint("bmp index is: %d\n", bmp_idx);
	}
	return 0;
}

u32 create_screenshot_callback() {	

	nsDbgPrint("start screenshot\n");
	if (!buf) {	
		u32 retm = plgRequestMemory(0x100000);
		buf = (u8*) retm;
		nsDbgPrint("buffer address: %08x\n", (u32) buf);
	}	
	controlVideo(CONTROLVIDEO_RELEASEVIDEO,0,0,0);
	create_screenshot();			
	svc_sleepThread(100000000); // 0.1 second
	controlVideo(CONTROLVIDEO_ACQUIREVIDEO,0,0,0);	
	nsDbgPrint("end screenshot\n");
	return 0;
}

u32 get_screenshot_index() {
	u8 bufx[200];
	Result retc, i;
	
	for (i=0; ; ++i) {
		xsprintf(bufx, "/top_%04d.bmp", i);
		retc = open_file(bufx, FS_OPEN_READ | FS_OPEN_WRITE);
		if (retc <= 0)
			break;
		close_file(retc);
	}	
	return i;
}

int main() {
	u32 retv;

	initSharedFunc();
	if (((NS_CONFIG*)(NS_CONFIGURE_ADDR))->sharedFunc[8]) {
		isNewNtr = 1;
	} else {
		isNewNtr = 0;
	}
	if (isNewNtr) {
		IoBasePad = plgGetIoBase(IO_BASE_PAD);
		IoBaseLcd = plgGetIoBase(IO_BASE_LCD);
		IoBasePdc = plgGetIoBase(IO_BASE_PDC);
		GSPOffset = plgGetIoBase(IO_BASE_GSPHEAP) - 0x14000000;
		PtoV = 0xC0000000;
	}
	else {
		IoBasePad = 0xFFFD4000;
		IoBaseLcd = 0xFFFD6000;
		IoBasePdc = 0xFFFCE000;
		GSPOffset = 0x0;
		PtoV = 0xD0000000;
	}

	nsDbgPrint("initializing REaM screenshot plugin\n");
	nsDbgPrint("isNewNtr:  %08x\n", isNewNtr);
	nsDbgPrint("IoBasePad: %08x\n", IoBasePad);
	nsDbgPrint("IoBaseLcd: %08x\n", IoBaseLcd);
	nsDbgPrint("IoBasePdc: %08x\n", IoBasePdc);
	nsDbgPrint("GSPOffset: %08x\n", GSPOffset);
	nsDbgPrint("PtoV:      %08x\n", PtoV);
	plgRegisterMenuEntry(CATALOG_MAIN_MENU, "REaM screenshot", &create_screenshot_callback);
	plgGetSharedServiceHandle("fs:USER", &fsUserHandle);
	nsDbgPrint("fsUserHandle: %08x\n", fsUserHandle);
	bmp_idx = get_screenshot_index();
	nsDbgPrint("bmp index is: %d\n", bmp_idx);
}

