#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"

typedef struct event {
  clock_time_t time;
  linkaddr_t addr;
} event_h;

#define MAX_NUMBER_OF_EVENTS 3
#define EVENT_TIMEOUT (30 * CLOCK_SECOND)
#define ALARM_CHECK_INTERVAL (CLOCK_SECOND)

static struct event event_history[MAX_NUMBER_OF_EVENTS];

static void handle_event(const linkaddr_t *src);
static void check_alarm_status();
static void print_event_history();

static void handle_event(const linkaddr_t *src) {
  clock_time_t current_time = clock_time();
  int i;
  int found = 0;
  
  // Check if this node is already in our history
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time > 0 && 
        linkaddr_cmp(&event_history[i].addr, src)) {
      // Update the time
      event_history[i].time = current_time;
      found = 1;
      break;
    }
  }
  
  // If not found, add to history
  if (!found) {
    // Find an empty slot or replace the oldest
    int oldest_idx = 0;
    clock_time_t oldest_time = event_history[0].time;
    
    for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
      if (event_history[i].time == 0) {
        oldest_idx = i;
        break;
      }
      else if (event_history[i].time < oldest_time) {
        oldest_idx = i;
        oldest_time = event_history[i].time;
      }
    }
    
    event_history[oldest_idx].time = current_time;
    linkaddr_copy(&event_history[oldest_idx].addr, src);
  }
  
  print_event_history();
  check_alarm_status();
}

static void check_alarm_status() {
  clock_time_t current_time = clock_time();
  int distinct_nodes = 0;
  linkaddr_t recent_nodes[MAX_NUMBER_OF_EVENTS];
  
  // Count distinct nodes with recent events
  int i;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time == 0) {
      continue;  // Skip empty slots
    }
    
    // Check if event is recent
    if (current_time - event_history[i].time <= EVENT_TIMEOUT) {
      int recent_event = 1;
      
      // Check if this node is already counted
      int j;
      for (j = 0; j < distinct_nodes; j++) {
        if (linkaddr_cmp(&event_history[i].addr, &recent_nodes[j])) {
          recent_event = 0;
          break;
        }
      }
      
      if (recent_event) {
        linkaddr_copy(&recent_nodes[distinct_nodes], &event_history[i].addr);
		printf("Button clicked on node %d\n", event_history[i].addr.u8[0]);
		printf("Number of distinct nodes with button events: %d\n", distinct_nodes);
        distinct_nodes++;
      }
    } else {
      // Clear timeout events
      printf("Button event from node %d timedout\n", 
             event_history[i].addr.u8[0]);
      event_history[i].time = 0;
    }
  }
  
  // Trigger alarm if 3 distinct nodes have recent events
  if (distinct_nodes == 3) {
    leds_on(LEDS_YELLOW);
    printf("Alarm ON!\n");
  } else {
    leds_off(LEDS_YELLOW);
    printf("No alarm\n");
  }
	
}

static void print_event_history() {
  printf("Button event history:\n");

  int i;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time > 0) {
      printf("[Button event ID %d] Time: %lu, Node: %d\n", 
             i, 
             (unsigned long)event_history[i].time,
             event_history[i].addr.u8[0]);
    }
  }
}

/*---------------------------------------------------------------------------*/
PROCESS(clicker_ng_process, "Clicker NG Process");
AUTOSTART_PROCESSES(&clicker_ng_process);

/*---------------------------------------------------------------------------*/

static void recv(
  const void *data,
  uint16_t len,
  const linkaddr_t *src, 
  const linkaddr_t *dest) 
{
  printf("Received: %s - from %d\n", (char*) data, src->u8[0]);
  handle_event(src);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(clicker_ng_process, ev, data)
{
  static char payload[] = "hej";

  PROCESS_BEGIN();

  /* Initialize NullNet */
   nullnet_buf = (uint8_t *)&payload;
   nullnet_len = sizeof(payload);
   nullnet_set_input_callback(recv);
  
  /* Activate the button sensor. */
  SENSORS_ACTIVATE(button_sensor);

  // Initialize event history
  int i;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    event_history[i].time = 0;
  }
  
  static struct etimer alarm_timer;
  etimer_set(&alarm_timer, ALARM_CHECK_INTERVAL);

  while(1) {
  
    PROCESS_WAIT_EVENT();

   if (ev == sensors_event && data == &button_sensor){
    memcpy(nullnet_buf, &payload, sizeof(payload));
    nullnet_len = sizeof(payload);

    /* Send the content of the packet buffer using the
     * broadcast handle. */
    NETSTACK_NETWORK.output(NULL);
	}

     if (etimer_expired(&alarm_timer)) {
      // Periodically check alarm status to turn off when events expire
      check_alarm_status();
      etimer_reset(&alarm_timer);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
