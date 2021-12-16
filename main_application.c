/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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
void prvSerialReceiveTask_0(void* pvParameters);
void prvSerialReceiveTask_1(void* pvParameters);


QueueHandle_t sk_q1,sk_q2,sk_q3;
SemaphoreHandle_t RXC_BS_0, RXC_BS_1,led_sem;

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TASKS: FORWARD DECLARATIONS */
void led_bar_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);


/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[20] = "POZDRAV\n";
char trigger1[30] = "\0";

unsigned volatile t_point;
unsigned volatile t_point1;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];

unsigned volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const unsigned char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
	0x15, 0x6d };


SemaphoreHandle_t TBE_BinarySemaphore;
SemaphoreHandle_t RXC_BinarySemaphore;
SemaphoreHandle_t LED_INT_BinarySemaphore;
TimerHandle_t tH1;
TimerHandle_t checkIdleCounterTimer;


typedef struct {
	uint16_t nivo[3];
	float srvr;
	char rezim;
	uint8_t prom;
} poruka_s;

char ispisr='A';
uint16_t ispisn[3] = { 250,500,750 };

uint32_t OnLED_ChangeInterrupt() {
	// Ovo se desi kad neko pritisne dugme na LED Bar tasterima
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);

	portYIELD_FROM_ISR(xHigherPTW);
}

/* TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER */
static uint32_t prvProcessTBEInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}

/* RXC - RECEPTION COMPLETE - INTERRUPT HANDLER */
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0)
		xSemaphoreGiveFromISR(RXC_BS_0, &xHigherPTW);

	if (get_RXC_status(1) != 0)
		xSemaphoreGiveFromISR(RXC_BS_1, &xHigherPTW);

	portYIELD_FROM_ISR(xHigherPTW);
}

void prvSerialReceiveTask_0(void* pvParameters) {
	poruka_s poruka;
	uint8_t ss;
	float tmp = 0;
	uint8_t cnt = 0;

	for (;;) {
		xSemaphoreTake(RXC_BS_0, portMAX_DELAY);
		get_serial_character(COM_CH_0, &ss);
		if (ss == 0x00) {
			
		}
		else if (ss == 0xff) {
			
		}
		else{
			cnt++;
			tmp += ss;
			if (cnt == 10) {
				tmp /= 10;
				tmp *= 100;
				poruka.srvr = tmp;
				cnt = 0;
				printf("UNICOM0: trenutno stanje senzora: %.2f\n", poruka.srvr);
				tmp = 0;
				xQueueSend(sk_q1, &poruka.srvr, 0);
			}
		}
	}
}

void prvSerialReceiveTask_1(void* pvParameters) {
	poruka_s poruka;
	uint8_t ss;
	uint8_t  cnt = 0, broj[3] = { 0,0,0 },niz[3] = { 100,10,1 };
	uint16_t broj1 = 0;
	uint8_t uslov=0,prom=0;
	uint8_t i, j;
	char tmp = '\0', tmp1 = '\0';

	for (;;) {
		xSemaphoreTake(RXC_BS_1, portMAX_DELAY);
		get_serial_character(COM_CH_1, &ss);

		if (ss == 'C') {
			tmp1 = 'C';
		}

		else if (ss == 'R' && tmp1 == 'C') {
			if (uslov == 1) {
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
				tmp1 = 0;

				j = cnt - 1;
				for (i = 0; i <= cnt - 1; i++) {
					broj1 = broj1 + broj[i] * pow(10, j);
					j--;
					broj[i] = 0;
				}
				poruka.nivo[prom] = broj1;
				ispisn[prom] = broj1;
				printf("%d\n", broj1);
				cnt = 0;
				broj1 = 0;
			}
		}

		else if (ss == 'N') {
			tmp = 'N';
		}

		else if (ss == 'K' && tmp == 'N') {
			printf("uneo si\n");
			tmp = 'K';
		}

		else if (tmp == 'K') {
			if (ss == 49) {
				prom = 0;
				tmp = 0;
			}
			else if(ss==50){
				prom = 1;
				tmp = 0;
			}
			else if (ss == 51) {
				prom = 2;
				tmp = 0;
			}
		}

		else if (ss == 'A') {
			poruka.rezim = 'A';
			uslov = 1;
		}
		else if (ss == 'M') {
			poruka.rezim = 'M';
			uslov = 1;
		}

		else {
			broj[cnt] = atoi(&ss);
			cnt++;
		}

	}
}

void set_led_Task(void* pvParameters) {
	poruka_s poruka;
	uint8_t tmp=0,tmp2=0, tmp1,prom1;
	uint16_t prom;
	for (;;)
	{

		xQueueReceive(sk_q1, &poruka.srvr, portMAX_DELAY);
		prom = poruka.srvr;
		sprintf(trigger1, "%.2f ", poruka.srvr);
		select_7seg_digit(4);
		set_7seg_digit(hexnum[prom / 1000]);
		prom %= 1000;
		select_7seg_digit(5);
		set_7seg_digit(hexnum[prom / 100]);
		prom %= 100;
		select_7seg_digit(6);
		set_7seg_digit(hexnum[prom / 10]);
		prom %= 10;
		select_7seg_digit(7);
		set_7seg_digit(hexnum[prom]);
		prom = 0;

		if (poruka.srvr < ispisn[0]) {
			vTaskDelay(200);
			set_LED_BAR(2, 0x00);
			set_LED_BAR(1, 0x00);
		}
		else {
			set_LED_BAR(1, 128);
		}

		if (ispisr == 'M') {
			strcat(trigger1, "Manuelno\n");
			if (get_LED_BAR(0, &tmp2) != 0) {
				printf("greska");
			}
			else
			{
				select_7seg_digit(0);
				set_7seg_digit(hexnum[0]);
				set_LED_BAR(2, tmp2);
				if (tmp2==0 ) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[0]);
					set_LED_BAR(1, 0);
					strcat(trigger1, "Ne Rade ");
				}
				else if (tmp2 == 1) {
					select_7seg_digit(2);
					set_LED_BAR(1, 128);
					set_7seg_digit(hexnum[1]);
					strcat(trigger1, "Rade ");
				}
				else if (tmp2 == 3) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[2]);
					set_LED_BAR(1, 128);
					strcat(trigger1, "Rade ");
				}
				else if (tmp2 == 7) {
					select_7seg_digit(2);
					set_LED_BAR(1, 128);
					set_7seg_digit(hexnum[3]);
					strcat(trigger1, "Rade ");
				}
			}
		}

		else {
			strcat(trigger1, "Automatski\n");
			select_7seg_digit(0);
			set_7seg_digit(hexnum[1]);
			if (poruka.srvr < ispisn[0]) {
				strcat(trigger1, "Ne Rade ");
			}
			else if (poruka.srvr >= ispisn[0] && poruka.srvr <= ispisn[1]) {
				set_LED_BAR(2, 1);
				strcat(trigger1, "Rade ");
			}
			else if (poruka.srvr >= ispisn[1] && poruka.srvr <= ispisn[2]) {
				set_LED_BAR(2, 3);
				strcat(trigger1, "Rade ");
			}
			else if (poruka.srvr >= ispisn[2] && poruka.srvr <= 1000) {
				set_LED_BAR(2, 7);
				strcat(trigger1, "Rade ");
			}
			if (get_LED_BAR(2, &tmp1) != 0) {
				printf("greska");
			}
			else
			{
				if (tmp1 == 0) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[0]);
				}
				else if (tmp1 == 1) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[1]);
				}
				else if (tmp1 == 3) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[2]);
				}
				else if (tmp1 == 7) {
					select_7seg_digit(2);
					set_7seg_digit(hexnum[3]);
				}
			}
		}
	}

}

void serialsend1_tsk(void* pvParameters) {
	t_point1 = 0;
	for (;;) {
		if (t_point1 > (sizeof(trigger1) - 1))
			t_point1 = 0;
		send_serial_character(1, trigger1[t_point1]);
		send_serial_character(1, trigger1[t_point1++] + 1);
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

void SerialSend_Task(void* pvParameters)
{
	t_point = 0;
	for (;;)
	{
		if (t_point > (sizeof(trigger) - 1))
			t_point = 0;
		send_serial_character(0, trigger[t_point]);
		send_serial_character(0, trigger[t_point++] + 1);
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

	 xTaskCreate(serialsend1_tsk, "Stld", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);
	vTaskStartScheduler();

	while (1);
}




