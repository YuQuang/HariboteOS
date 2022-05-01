#include "bootpack.h"

#define FLAGS_OVERRUN 0x0001    // 溢出旗標

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char* buf)
{
    fifo->buf = buf;
    fifo->flags = 0;
    fifo->free = size;
    fifo->p = 0;
    fifo->q = 0;
    fifo->size = size;
    return;
}

// 往 fifo 放數據
int fifo8_put(struct FIFO8 *fifo, unsigned char data)
{
    // 沒有空間了
    if(fifo->free == 0){
        // 溢出旗標
        fifo->flags = FLAGS_OVERRUN;
        return -1;
    }
    fifo->buf[fifo->p] = data;
    fifo->p++;
    if(fifo->p == fifo->size){ fifo->p = 0; }
    fifo->free--;
    return 0;
}

// POP一筆資料
char fifo8_get(struct FIFO8 *fifo)
{
    int data;
    // 空閒空間 = 總空間 (空的)
    if(fifo->free == fifo->size){ return -1; }
    data = fifo->buf[fifo->q];
    fifo->q++;
    if(fifo->q == fifo->size){ fifo->q = 0; }
    fifo->free++;
    return data;
}

// 剩多少空間
int fifo8_status(struct FIFO8 *fifo)
{
    return fifo->size - fifo->free;
}