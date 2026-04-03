#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "Key.h"
#include "MyCAN.h"
#include "LED.h"
#include "IRSensor.h"
#include "Serial.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/*=========== 类型与宏定义 ===========*/
typedef enum {
	RED_OFF,      						 
	RED_BLINK_SLOW,  					 
	RED_BLINK_FAST   					 
} RedLED_State;


/*=========== 全局句柄 ===========*/
QueueHandle_t CAN_RxQueue;  		// CAN 接收队列
SemaphoreHandle_t CAN_Sem;
RedLED_State red_state = RED_OFF;
static TickType_t red_timeout;


/*=========== 任务函数声明 ===========*/
void vTask_LED(void *pvParameters);
void vTask_Input(void *pvParameters);
void vTask_CAN_Handler(void *pvParameters);
void vTask_OLED(void *pvParameters);


/*=========== 主函数 ===========*/
int main(void)
{	/* 2. 创建内核对象 */
	CAN_RxQueue = xQueueCreate(5, sizeof(CAN_MsgPacket));	// CAN数据队列
	CAN_Sem = xSemaphoreCreateBinary();						// 同步信号量
	
	/* 1. 硬件底层初始化 */
	OLED_Init();
	Key_Init();
	MyCAN_Init();
	LED_Init();
	IRSensor_Init();
	Serial_Init();
	
	/* 初始化成功，打印日志到串口 */
	printf("STM32 FreeRTOS CAN Demo Start\r\n");
	
	/* 3. 创建任务 */
	xTaskCreate(vTask_Input,       "In_Task",  256, NULL, 2, NULL);
    xTaskCreate(vTask_LED,         "LED_Task", 128, NULL, 1, NULL);
    xTaskCreate(vTask_OLED,        "UI_Task",  256, NULL, 1, NULL);
	
	/* 4. 启动调度器 */
	vTaskStartScheduler();
}


/*=========== 任务 1: LED 视觉反馈 (呼吸灯 + 状态机) ===========*/
void vTask_LED(void *pvParameters)
{
	static uint16_t pwm_val = 0;
	static uint8_t dir = 0;			// 0:变亮, 1:变暗
	
	while(1)
	{
		// 红灯超时自动关闭
		if(red_state != RED_OFF && xTaskGetTickCount() >= red_timeout)
		{
			red_state = RED_OFF;
			RedLED_OFF();
		}
		
		if(red_state == RED_OFF)
		{
			/* 蓝色呼吸灯逻辑 */
			if(dir == 0)
			{
				pwm_val++;
				if(pwm_val >= 100) dir = 1;
			}
			else
			{
				pwm_val--;
				if(pwm_val <= 0) dir = 0;
			}
			PWM_SetCCR(pwm_val);  		 // 设置蓝色LED亮度
			vTaskDelay(pdMS_TO_TICKS(15));
		}
		// 模式 1：慢闪（发送中）
		else if(red_state == RED_BLINK_SLOW)
		{
			RedLED_Toggle();
			vTaskDelay(pdMS_TO_TICKS(200));
		}
		// 模式 2：快闪（紧急状态）
		else if(red_state == RED_BLINK_FAST)
		{
			RedLED_Toggle();
			vTaskDelay(pdMS_TO_TICKS(70));
		}
	}
}


/*=========== 任务 2: 输入监控 (按键发送 + 红外检测) ===========*/
void vTask_Input(void *pvParameters)
{
	CAN_MsgPacket TxPacket = {0x555, 4, {0x11,0x22,0x33,0x44}};
	
	while(1)
	{
		Key_Scan();
		
		if(Key_GetNum() == 1)
		{
			red_state = RED_BLINK_SLOW;
			red_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(600);
			
			TxPacket.Data[0]++;
			TxPacket.Data[1]++;
			TxPacket.Data[2]++;
			TxPacket.Data[3]++;
			MyCAN_Transmit(TxPacket.ID, TxPacket.DLC, TxPacket.Data);
			
			printf(">> CAN Sent: Success!\r\n");
		}
		
		if(IR_State == 1)
		{
			IR_State = 0;
			red_state = RED_BLINK_FAST;
			red_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(600);
			printf("!!! EMERGENCY: IR Sensor Triggered !!!\r\n");
		}
		
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}


/*=========== 任务 3: OLED 显示刷新 (消费者) ===========*/
void vTask_OLED(void *pvParameters)
{
	CAN_MsgPacket displayBuffer;
	
	/* OLED 固定显示内容初始化 */
	OLED_ShowString(1, 1, "TxID: 555");
	OLED_ShowString(2, 1, "RxID:");
	OLED_ShowString(3, 1, "Leng:");
	OLED_ShowString(4, 1, "Data:");
	
	while(1)
	{
		// 参数：队列句柄, 存放数据的地址, portMAX_DELAY(死等直到有消息)
		if(xQueueReceive(CAN_RxQueue, &displayBuffer, portMAX_DELAY) == pdPASS)
		{
			OLED_ShowHexNum(2, 6, displayBuffer.ID, 3);
			OLED_ShowHexNum(3, 6, displayBuffer.DLC, 1);
			OLED_ShowHexNum(4, 6,  displayBuffer.Data[0], 2);
			OLED_ShowHexNum(4, 9,  displayBuffer.Data[1], 2);
			OLED_ShowHexNum(4, 12, displayBuffer.Data[2], 2);
			OLED_ShowHexNum(4, 15, displayBuffer.Data[3], 2);
			
			printf("<< OLED Display Updated: Success!\r\n");
		}
	}
}
