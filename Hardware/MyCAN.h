#ifndef __MYCAN_H
#define __MYCAN_H
 
#include "FreeRTOS.h"
#include "semphr.h"

// 定义 CAN 数据传输结构体
typedef struct {
	uint32_t ID;
	uint8_t DLC;
	uint8_t Data[8];
} CAN_MsgPacket;
 
extern CanRxMsg RxMessage;
extern uint8_t RxFlagStatus;
 
void MyCAN_Init(void);
void MyCAN_Transmit(uint32_t ID, uint8_t Length, uint8_t *Data);

#endif
