
#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"

/*---------------------------------------------------------------------------*/
PROCESS(clicker_ng_process, "Clicker NG Process");
AUTOSTART_PROCESSES(&clicker_ng_process);
/*---------------------------------------------------------------------------*/

typedef struct event {
  clock_time_t	time;
  linkaddr_t	addr;
} event_t;

#define MAX_EV_NUM  3
static event_t history[MAX_EV_NUM];
static uint8_t history_index = 0;

void print_events(void)
{
	clock_time_t now = clock_time();
	printf("Event History:\n");
	uint8_t i = 0;
	for (; i < history_index; i++) {
		// Skip expired events (older than 30 seconds)
		if (now - history[i].time > 30 * CLOCK_SECOND) {
			continue;
		}
		
		// Calculate time difference in seconds
		uint32_t time_diff = (now - history[i].time) / CLOCK_SECOND;
		
		// Print event details: index, time ago in seconds, and source address
		printf("[%u] %lu seconds ago from node %u\n", 
			i,
			time_diff,
			history[i].addr.u8[0]);
	}
	
	if (history_index == 0) {
		printf("No events recorded\n");
	}
	printf("-----------------\n");
}

void handle_event(const linkaddr_t* src)
{
	clock_time_t now = clock_time();
	uint8_t ix = 0;
	for (;ix < MAX_EV_NUM; ++ix)
	{
		if (now + 30 * CLOCK_SECOND < now) 
		{
			if (ix == MAX_EV_NUM - 1) break;
			history[ix] = history[ix + 1];
		}
	}

	if (history_index < 3) {
		history[history_index++] = (event_t) {
			.time	= now,
			.addr	= *src,
		};
	} else {
		history[0] = history[1];
		history[1] = history[2];
		history[2] = (event_t) {
			.time	= now,
			.addr	= *src,
		};
	}

	ix = 0;
	for (;ix < MAX_EV_NUM; ++ix)
	{
		if (now + 30 * CLOCK_SECOND < now) 
		{
			return;
		}
	}

	leds_toggle(LEDS_YELLOW);
}

static void recv(
  const void *data, 
  uint16_t len,
  const linkaddr_t *src, 
  const linkaddr_t *dest
) {
	printf("Received: %s - from %d\n", (char*)data, src->u8[0]);
	leds_toggle(LEDS_GREEN);
	handle_event(src);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(clicker_ng_process, ev, data)
{
	static char payload[] = "hej";

	PROCESS_BEGIN();

	/* Initialize NullNet */
	nullnet_buf = (uint8_t*)&payload;
	nullnet_len = sizeof(payload);
	nullnet_set_input_callback(recv);
  
	/* Activate the button sensor. */
	SENSORS_ACTIVATE(button_sensor);

	while(1) 
	{
		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&data == &button_sensor);

		leds_toggle(LEDS_RED);

		memcpy(nullnet_buf, &payload, sizeof(payload));
		nullnet_len = sizeof(payload);

		/* Send the content of the packet buffer using the
		* broadcast handle. */
		NETSTACK_NETWORK.output(NULL);
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
