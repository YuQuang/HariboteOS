#include "bootpack.h"

extern struct FIFO8 keyfifo, mousefifo;

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct MOUSE_DEC mdec;
	char s[40], keyboardstr[10], mousestr[20], mcursor[256], keybuf[32], mousebuf[128], i;
	int mx, my;
	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

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

	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000);
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	sprintf(s, "Memory %dMB  free %dKB"
			, memtotal / (1024 * 1024), memman_total(memman) / 1024);
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
