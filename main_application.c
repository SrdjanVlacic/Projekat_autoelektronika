/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

#define	TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY  )
#define TASK_SERIAL_REC_PRI			(3+ tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI		(1+ tskIDLE_PRIORITY )

/* TASKS: FORWARD DECLARATIONS */
static void prvSerialReceiveTask_0(void* pvParameters);
static void prvSerialReceiveTask_1(void* pvParameters);
static void SerialSend_Task(void* pvParameters);
static void serialsend1_tsk(void* pvParameters);
static void set_led_Task(void* pvParameters);

static QueueHandle_t sk_q1,sk_q2,sk_q3;
static SemaphoreHandle_t RXC_BS_0, RXC_BS_1,led_sem;

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
static const char trigger[20] = "POZDRAV\n";
static char trigger1[30] = "";

static uint16_t volatile t_point;
static uint16_t volatile t_point1;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
static uint16_t r_buffer[R_BUF_SIZE];

static uint16_t volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const uint16_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
	0x15, 0x6d };


static SemaphoreHandle_t  TBE_BinarySemaphore;
static SemaphoreHandle_t RXC_BinarySemaphore;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static TimerHandle_t tH1;
static TimerHandle_t checkIdleCounterTimer;

typedef struct {
	uint16_t nivo[3];
	float srvr;
	char rezim;
	uint16_t prom;
} poruka_s;

static char ispisr='A';
static uint16_t ispisn[3] = { 250,500,750 };

static uint32_t OnLED_ChangeInterrupt() {
	// Ovo se desi kad neko pritisne dugme na LED Bar tasterima
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

/* TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER */
static uint32_t prvProcessTBEInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

/* RXC - RECEPTION COMPLETE - INTERRUPT HANDLER */
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status((uint8_t)0) != 0) {
		xSemaphoreGiveFromISR(RXC_BS_0, &xHigherPTW);
	}

	if (get_RXC_status((uint8_t)1) != 0) {
		xSemaphoreGiveFromISR(RXC_BS_1, &xHigherPTW);
	}

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

static void prvSerialReceiveTask_0(void* pvParameters) {
	poruka_s poruka;
	uint8_t ss;
	float tmp = 0;
	uint16_t cnt = 0;

	for (;;) {
		xSemaphoreTake(RXC_BS_0, portMAX_DELAY);
		get_serial_character(COM_CH_0, &ss);
		if (ss == (uint16_t)0x00) {
			
		}
		else if (ss == (uint16_t)0xff) {
			
		}
		else{
			cnt++;
			tmp += (float)ss;
			if (cnt == (uint16_t)10) {
				tmp /= (float)10;
				tmp *= (float)100;
				poruka.srvr = tmp;
				cnt = 0;
				printf("UNICOM0: trenutno stanje senzora: %.2f\n", poruka.srvr);
				tmp = (float)0;
				xQueueSend(sk_q1, &poruka.srvr, 0);
			}
		}
	}
}

static void prvSerialReceiveTask_1(void* pvParameters) {
	poruka_s poruka;
	uint8_t ss;
	uint16_t  cnt = 0, broj[3] = { 0,0,0 },niz[3] = { 100,10,1 };
	uint16_t broj1 = 0;
	uint16_t uslov=0,prom=0;
	uint16_t i, j;
	char tmp = '\0', tmp1 = '\0';

	for (;;) {
		xSemaphoreTake(RXC_BS_1, portMAX_DELAY);
		get_serial_character(COM_CH_1, &ss);

		if (ss == (uint8_t)'C') {
			tmp1 = 'C';
		}

		else if (ss == (uint8_t)'R' && tmp1 == 'C') {
			if (uslov == (uint16_t)1) {
				if (poruka.rezim == 'A') {
					printf("UNICOM1: OK\nUNICOM1: AUTOMATSKI\n");
				}
				if (poruka.rezim == 'M') {
					printf("UNICOM1: OK\nUNICOM1: MANUELNO\n");
				}		
				ispisr = poruka.rezim;
				uslov = 0;
			}
			else {
				printf("kraj poruke\n");
				tmp1 = (char)0;

				j = cnt - (uint16_t)1;
				for (i = 0; i <= cnt - (uint16_t)1; i++) {
					broj1 = broj1 + broj[i] * (uint16_t)pow((uint16_t)10, j);
					j--;
					broj[i] = (uint16_t)0;
				}
				poruka.nivo[prom] = broj1;
				ispisn[prom] = broj1;
				printf("%d\n", broj1);
				cnt = (uint16_t)0;
				broj1 = (uint16_t)0;
			}
		}

		else if (ss == (uint8_t)'N') {
			tmp = 'N';
		}

		else if (ss == (uint8_t)'K' && tmp == 'N') {
			printf("uneo si\n");
			tmp = 'K';
		}

		else if (tmp == 'K') {
			if (ss == (uint8_t)49) {
				prom = 0;
				tmp = (char)0;
			}
			else if(ss== (uint8_t)50){
				prom = 1;
				tmp = (char)0;
			}
			else if (ss == (uint8_t)51) {
				prom = 2;
				tmp = (char)0;
			}
			else { // asd
			}
		}

		else if (ss == (uint8_t)'A') {
			poruka.rezim = 'A';
			uslov = 1;
		}
		else if (ss == (uint8_t)'M') {
			poruka.rezim = 'M';
			uslov = 1;
		}

		else {
			broj[cnt] = (uint16_t)(atoi(&ss));
			cnt++;
		}

	}
}

static void set_led_Task(void* pvParameters) {
	poruka_s poruka;
	uint8_t tmp=0,tmp2=0, tmp1,prom1;
	uint16_t prom;
	for (;;)
	{

		xQueueReceive(sk_q1, &poruka.srvr, portMAX_DELAY);
		prom = (uint16_t)poruka.srvr;
		sprintf(trigger1, "%.2f ", poruka.srvr);
		select_7seg_digit((uint8_t)4);
		set_7seg_digit(hexnum[(uint8_t)(prom / (uint16_t)1000)]);
		prom %= (uint16_t)1000;
		select_7seg_digit((uint8_t)5);
		set_7seg_digit(hexnum[(uint8_t)(prom / (uint16_t)100)]);
		prom %= (uint16_t)100;
		select_7seg_digit((uint8_t)6);
		set_7seg_digit(hexnum[(uint8_t)(prom / (uint16_t)10)]);
		prom %= (uint16_t)10;
		select_7seg_digit((uint8_t)7);
		set_7seg_digit(hexnum[(uint8_t)prom]);
		prom = (uint16_t)0;

		if (poruka.srvr < (float)ispisn[0]) {
			vTaskDelay(200);
			set_LED_BAR((uint8_t)2, (uint8_t)0x00);
			set_LED_BAR((uint8_t)1, (uint8_t)0x00);
		}
		else {
			set_LED_BAR((uint8_t)1, (uint8_t)128);
		}

		if (ispisr == 'M') {
			strcat(trigger1, "Manuelno\n");
			if (get_LED_BAR((uint8_t)0, &tmp2) != (uint8_t)0) {
				printf("greska");
			}
			else
			{
				select_7seg_digit((uint8_t)0);
				set_7seg_digit((uint8_t)hexnum[0]);
				set_LED_BAR((uint8_t)2, (uint8_t)tmp2);
				if (tmp2 == (uint8_t)0 ) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[0]);
					set_LED_BAR((uint8_t)1, (uint8_t)0);
					strcat(trigger1, "Ne Rade ");
				}
				else if (tmp2 == (uint8_t)1) {
					select_7seg_digit((uint8_t)2);
					set_LED_BAR((uint8_t)1, (uint8_t)128);
					set_7seg_digit((uint8_t)hexnum[1]);
					strcat(trigger1, "Rade ");
				}
				else if (tmp2 == (uint8_t)3) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[2]);
					set_LED_BAR((uint8_t)1, (uint8_t)128);
					strcat(trigger1, "Rade ");
				}
				else if (tmp2 == (uint8_t)7) {
					select_7seg_digit((uint8_t)2);
					set_LED_BAR((uint8_t)1, (uint8_t)128);
					set_7seg_digit((uint8_t)hexnum[3]);
					strcat(trigger1, "Rade ");
				}
				else {
					//
				}
			}
		}

		else {
			strcat(trigger1, "Automatski\n");
			select_7seg_digit((uint8_t)0);
			set_7seg_digit((uint8_t)hexnum[1]);
			if (poruka.srvr < (float)ispisn[0]) {
				strcat(trigger1, "Ne Rade ");
			}
			else if (poruka.srvr >= (float)ispisn[0] && poruka.srvr <= (float)ispisn[1]) {
				set_LED_BAR((uint8_t)2, (uint8_t)1);
				strcat(trigger1, "Rade ");
			}
			else if (poruka.srvr >= (float)ispisn[1] && poruka.srvr <= (float)ispisn[2]) {
				set_LED_BAR((uint8_t)2, (uint8_t)3);
				strcat(trigger1, "Rade ");
			}
			else if (poruka.srvr >= (float)ispisn[2] && (float)poruka.srvr <= (float)1000) {
				set_LED_BAR((uint8_t)2, (uint8_t)7);
				strcat(trigger1, "Rade ");
			}
			if (get_LED_BAR((uint8_t)2, &tmp1) != (uint8_t)0) {
				printf("greska");
			}
			else
			{
				if (tmp1 == (uint8_t)0) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[0]);
				}
				else if (tmp1 == (uint8_t)1) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[1]);
				}
				else if (tmp1 == (uint8_t)3) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[2]);
				}
				else if (tmp1 == (uint8_t)7) {
					select_7seg_digit((uint8_t)2);
					set_7seg_digit((uint8_t)hexnum[3]);
				}
				else {
					//
				}
			}
		}
	}

}

static void serialsend1_tsk(void* pvParameters) {
	t_point1 = 0;
	for (;;) {
		if (t_point1 > (uint16_t)(sizeof(trigger1) - 1)) {
			t_point1 = (uint16_t)0;
		}
		
		send_serial_character((uint8_t)1, trigger1[t_point1]);
		send_serial_character((uint8_t)1, trigger1[t_point1++] + 1);
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

static void SerialSend_Task(void* pvParameters)
{
	t_point = 0;
	for (;;)
	{
		if (t_point > (uint16_t)(sizeof(trigger) - 1)) {
			t_point = (uint16_t)0;
		}

		send_serial_character((uint8_t)0, trigger[t_point]);
		send_serial_character((uint8_t)0, trigger[t_point++] + 1);
		//xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		vTaskDelay(pdMS_TO_TICKS(200)); // kada se koristi vremenski delay }
	}
}

void main_demo(void)
{
	init_7seg_comm();
	init_LED_comm();
	init_serial_uplink(COM_CH_0);
	init_serial_downlink(COM_CH_0);
	init_serial_uplink(COM_CH_1);
	init_serial_downlink(COM_CH_1);

	//interrupts
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);

	RXC_BS_0 = xSemaphoreCreateBinary();
	RXC_BS_1 = xSemaphoreCreateBinary();
	led_sem = xSemaphoreCreateBinary();

	/* Create LED interrapt semaphore */
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();
	RXC_BinarySemaphore = xSemaphoreCreateBinary();
	TBE_BinarySemaphore = xSemaphoreCreateBinary();

	sk_q1 = xQueueCreate(10, sizeof(sk_q1));
	sk_q2 = xQueueCreate(10, sizeof(sk_q2));
	sk_q3 = xQueueCreate(10, sizeof(sk_q3));

	/* SERIAL RECEIVER TASK */
	 xTaskCreate(prvSerialReceiveTask_0, "SR0", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_REC_PRI, NULL);

	/* SERIAL RECEIVER TASK */
	 xTaskCreate(prvSerialReceiveTask_1, "SR1", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_REC_PRI, NULL);
	
	/* SERIAL TRANSMITTER TASK */
	 xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);
	 /*set led*/
	 xTaskCreate(set_led_Task, "Stld", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);

	 xTaskCreate(serialsend1_tsk, "stx1", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);
	vTaskStartScheduler();

	while (1);
}




