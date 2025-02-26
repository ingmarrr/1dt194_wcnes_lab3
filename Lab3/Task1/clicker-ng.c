#include "contiki.h"
#include "net/nullnet/nullnet.h"
#include "net/netstack.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include <stdio.h>

struct event {
  clock_time_t time;
  linkaddr_t addr;
};

#define MAX_NUMBER_OF_EVENTS 10
#define EVENT_TIMEOUT (30 * CLOCK_SECOND)
#define ALARM_CHECK_INTERVAL (CLOCK_SECOND)

// Structure for the message payload
struct click_msg {
  linkaddr_t source;
};

static struct event event_history[MAX_NUMBER_OF_EVENTS];
// static struct click_msg msg_buf;

// Function prototypes
static void input_callback(const void *data, uint16_t len,
                          const linkaddr_t *src, const linkaddr_t *dest);
static void handle_event(const linkaddr_t *src);
static void check_alarm_status();
static void print_event_history();

PROCESS(clicker_process, "Clicker");
AUTOSTART_PROCESSES(&clicker_process);

static void input_callback(const void *data, uint16_t len,
                          const linkaddr_t *src, const linkaddr_t *dest) {
  if (len != sizeof(struct click_msg)) {
    printf("Received malformed packet\n");
    return;
  }

  const struct click_msg *msg = data;
  printf("Message received from %d.%d (source: %d.%d)\n", 
         src->u8[0], src->u8[1],
         msg->source.u8[0], msg->source.u8[1]);
  
  // Toggle the green LED (preserving original behavior)
  leds_toggle(LEDS_GREEN);
  
  // Handle the event from the source indicated in the message
  handle_event(&msg->source);
}

static void handle_event(const linkaddr_t *src) {
  clock_time_t current_time = clock_time();
  int i;
  int updated = 0;
  
  // First check if this node is already in our history
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time > 0 && 
        linkaddr_cmp(&event_history[i].addr, src)) {
      // Update the time
      event_history[i].time = current_time;
      updated = 1;
      break;
    }
  }
  
  // If not found, add to history
  if (!updated) {
    // Find an empty slot or replace the oldest
    int oldest_idx = 0;
    clock_time_t oldest_time = event_history[0].time;
    
    for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
      if (event_history[i].time == 0) {
        oldest_idx = i;
        break;
      }
      if (event_history[i].time < oldest_time) {
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
  int recent_distinct_nodes = 0;
  linkaddr_t recent_nodes[MAX_NUMBER_OF_EVENTS];
  
  // Count distinct nodes with recent events
  int i;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time == 0) {
      continue;  // Skip empty slots
    }
    
    // Check if event is recent
    if (current_time - event_history[i].time <= EVENT_TIMEOUT) {
      int is_new = 1;
      
      // Check if we've already counted this node
      int j;
      for (j = 0; j < recent_distinct_nodes; j++) {
        if (linkaddr_cmp(&event_history[i].addr, &recent_nodes[j])) {
          is_new = 0;
          break;
        }
      }
      
      if (is_new) {
        linkaddr_copy(&recent_nodes[recent_distinct_nodes], &event_history[i].addr);
        recent_distinct_nodes++;
      }
    } else {
      // Clear expired events
      printf("Event from node %d.%d expired\n", 
             event_history[i].addr.u8[0], 
             event_history[i].addr.u8[1]);
      event_history[i].time = 0;
    }
  }
  
  printf("Number of distinct nodes with recent events: %d\n", recent_distinct_nodes);
  
  // Trigger alarm if 3 or more distinct nodes have recent events
  if (recent_distinct_nodes >= 3) {
    leds_on(LEDS_YELLOW);
    printf("ALARM TRIGGERED!\n");
  } else {
    leds_off(LEDS_YELLOW);
    printf("No alarm\n");
  }
}

static void print_event_history() {
  printf("Event history:\n");
  int i;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
    if (event_history[i].time > 0) {
      printf("[%d] Time: %lu, Node: %d.%d\n", 
             i, 
             (unsigned long)event_history[i].time,
             event_history[i].addr.u8[0], 
             event_history[i].addr.u8[1]);
    }
  }
}

PROCESS_THREAD(clicker_process, ev, data) {
  PROCESS_BEGIN();

  // Initialize nullnet
  nullnet_set_input_callback(input_callback);
  
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
    
    if (ev == sensors_event && data == &button_sensor) {
      printf("Button clicked on node %d.%d\n", 
             linkaddr_node_addr.u8[0], 
             linkaddr_node_addr.u8[1]);
      
      // Prepare and send a packet with source information
      struct click_msg msg;
      linkaddr_copy(&msg.source, &linkaddr_node_addr);
      
      // Set the nullnet buffer and send the message
      nullnet_buf = (uint8_t *)&msg;
      nullnet_len = sizeof(msg);
      NETSTACK_NETWORK.output(NULL);
      
      // Handle the local event
      handle_event(&linkaddr_node_addr);
    }
    
    if (etimer_expired(&alarm_timer)) {
      // Periodically check alarm status to turn off when events expire
      check_alarm_status();
      etimer_reset(&alarm_timer);
    }
  }
  
  PROCESS_END();
}