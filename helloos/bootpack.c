#include "bootpack.h"

extern struct FIFO8 keyfifo, mousefifo;

struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int btn, x, y;
};

void init_keyboard(void);
void enable_mouse(struct MOUSE_DEC *mdec);
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char i);
unsigned int memtest(unsigned int start, unsigned int end);

/* 主要函數 */
void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct MOUSE_DEC mdec;
	char s[40], keyboardstr[10], mousestr[20], mcursor[256], keybuf[32], mousebuf[128], i;
	int mx, my;
	unsigned int memorySize;

	// 初始化 GDT IDT PIC
	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */

	fifo8_init(&keyfifo, 32, keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);
	io_out8(PIC0_IMR, 0xf9); /* 开放PIC1和键盘中断(11111001) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */
	
	// 初始化鍵盤滑鼠控制器
	init_keyboard();

	// 初始化調色盤、螢幕顯示畫面、滑鼠樣式以及顯示滑鼠
	init_palette();
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny);
	mx = (binfo->scrnx - 16) >> 1; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) >> 1;
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);

	// 初始化一開始顯示的字符
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(binfo->vram, binfo->scrnx,  0, 0, COL8_000000, s);

	memorySize = memtest(0x00400000, 0xbfffffff) / (1024 * 1024);
	sprintf(s, "Memory %dMB", memorySize);
	putfonts8_asc(binfo->vram, binfo->scrnx,  0, 64, COL8_000000, s);
	
	// 啟用滑鼠
	enable_mouse(&mdec);


	for(;;){
		io_cli();
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_stihlt();
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti();
				sprintf(keyboardstr, "%02X", (i & 0x0000ff));
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484,  0, 16, 31 + (8 << 3), 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, keyboardstr);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti();
				if(mouse_decode(&mdec, i) != 0){
					/* 印出滑鼠傳來的 Byte */
					// sprintf(mousestr, "%02X, %02X, %02X", mdec.buf[0], mdec.buf[1], mdec.buf[2]);
					// boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 32, 31 + (8 << 3), 47);
					// putfonts8_asc(binfo->vram, binfo->scrnx, 0, 32, COL8_FFFFFF, mousestr);

					/* 印出滑鼠按件以及移動狀態 */
					// sprintf(mousestr, "[lcr, %3d, %3d]", mdec.x, mdec.y);
					// if((mdec.btn & 0x01) != 0) mousestr[1] = 'L';
					// if((mdec.btn & 0x02) != 0) mousestr[3] = 'R';
					// if((mdec.btn & 0x04) != 0) mousestr[2] = 'C';
					// boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 48, 40 + (9 << 3), 63);
					// putfonts8_asc(binfo->vram, binfo->scrnx, 0, 48, COL8_FFFFFF, mousestr);

					
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx+15, my+15);
					mx+=mdec.x;
					my+=mdec.y;
					if(mx < 0) mx = 0;
					else if(mx > (binfo->scrnx-16)) mx = binfo->scrnx-16;
					if(my < 0) my = 0;
					else if(my > (binfo->scrny-16)) my = binfo->scrny-16;
					sprintf(s, "(%3d, %3d)", mx, my);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 0, 79, 15);
					putfonts8_asc(binfo->vram, binfo->scrnx,  0, 0, COL8_000000, s);
					putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
				}
			}
		}
	}
}

#define PORT_KEYDAT				0x0060
#define PORT_KEYSTA				0x0064
#define PORT_KEYCMD				0x0064
#define KEYSTA_SEND_NOTREADY	0x02
#define KEYCMD_WRITE_MODE		0x60
#define KBC_MODE				0x47

// 等待鍵盤控制器處理
void wait_KBC_sendready(void)
{
	/* 等待键盘控制电路准备完毕 */
	for (;;) {
		if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
			break;
		}
	}
	return;
}

// 初始化鍵盤控制器
void init_keyboard(void)
{
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, KBC_MODE);
	return;
}


#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

void enable_mouse(struct MOUSE_DEC *mdec)
{
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	mdec->phase = 0;
	return;	// 順利的話 鍵盤控制器會返回 ACK (0xfa)
}

int mouse_decode(struct MOUSE_DEC *mdec, unsigned char i)
{
	switch (mdec->phase)
	{
		case 0:
			if(i == 0xfa) mdec->phase = 1;
			return 0;
		case 1:
			if((i & 0xc8) == 0x08){
				mdec->buf[0] = i;
				mdec->phase = 2;
			}
			return 0;
		case 2:
			mdec->buf[1] = i;
			mdec->phase = 3;
			return 0;
		case 3:
			mdec->buf[2] = i;
			mdec->phase = 1;
			mdec->btn = mdec->buf[0] & 0x07;
			mdec->x = mdec->buf[1];
			mdec->y = mdec->buf[2];

			if((mdec->buf[0] & 0x10) != 0){
				mdec->x |= 0xffffff00;
			}
			if((mdec->buf[0] & 0x20) != 0){
				mdec->y |= 0xffffff00;
			}

			mdec->y = -mdec->y;

			return 1;
		default: break;
	}
	return -1;
}


#define EFLAGS_AC_BIT 		0x00040000
#define CR0_CACHE_DISABLE 	0x60000000

unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	/* 確認是 386 還是 486 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT;
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0){
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT;
	io_store_eflags(eflg);

	if(flg486 != 0){
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE;
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if(flg486 != 0){
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE;
		store_cr0(cr0);
	}

	return i;
}

// unsigned int memtest_sub(unsigned int start, unsigned int end)
// {
// 	unsigned int i, *p, old, pat0 = 0xaa55aa55, pat1 = 0x55aa55aa;
// 	for(i = start; i < end; i+=0x1000)
// 	{
// 		p = (unsigned int *) (i + 0xffc);
// 		old = *p;
// 		*p = pat0;
// 		*p ^= 0xffffffff;
// 		if( *p != pat1){
// 			not_memory:
// 				*p = old;
// 				break;
// 		}
// 		*p ^= 0xffffffff;
// 		if(*p != pat0){
// 			goto not_memory;
// 		}
// 		*p = old;
// 	}
// 	return i;
// }