#include "stm32f10x.h"                  // Device header

static uint8_t Key_State = 0;      // 按键状态机
static uint32_t Key_Time = 0;      // 消抖计时
uint8_t Key_Flag = 0;              // 按键按下标志


void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

// 无阻塞按键扫描
void Key_Scan(void)
{
	// 读取引脚状态 (假设 0 为按下)
	uint8_t GPIO_Val = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);
	
	switch(Key_State)
	{
		case 0:  // 等待按下
			if(GPIO_Val == 0)
			{
				Key_State = 1;	
			}
			break;
		
		case 1:	// 确认按下 (消抖)
			// 因为 vTask_Input 的 delay 是 20ms，
            // 意味着下一次进入这个函数已经是 20ms 以后了
			if(GPIO_Val == 0)
			{
				Key_State = 2;  // 20ms 后依然是低电平，确认是真的按下了
			}
			else
			{
				Key_State = 0;  // 抖动，回退
			}
			break;
		
		case 2:	// 等待松开
			if(GPIO_Val == 1)
			{
				Key_Flag = 1;      // 确认松手，触发有效按键
				Key_State = 0;     // 回到初始
			}
			break;
	}
}


uint8_t Key_GetNum(void)
{
	if(Key_Flag == 1)
	{
		Key_Flag = 0;
		return 1;
	}
	return 0;
}
