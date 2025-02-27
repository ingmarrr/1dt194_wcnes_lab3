#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "dev/leds.h"
#include "net/routing/rpl-classic/rpl.h"
#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

#define CONNECTIVITY_TIMEOUT 30
static clock_time_t last_parent_response = 0;

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
	int is_coordinator = 0;

	PROCESS_BEGIN();

	is_coordinator = (node_id == 1);

	if (is_coordinator) 
	{
		NETSTACK_ROUTING.root_start();
    	leds_on(LEDS_GREEN);
	}
	NETSTACK_MAC.on();

	{
		static struct etimer et;
		/* Print out routing tables every minute */
		etimer_set(&et, CLOCK_SECOND * 10);
		while(1) {
			LOG_INFO("Routing entries: %u\n", uip_ds6_route_num_routes());
			uip_ds6_route_t *route = uip_ds6_route_head();
			LOG_INFO("[MODE] has joined: %u\n", NETSTACK_ROUTING.node_is_reachable());
			LOG_INFO("[HEAD] is null: %u\n", route == NULL);

			//if (!is_coordinator) {
			//	rpl_instance_t *instance = rpl_get_default_instance();
                
			//	if (instance != NULL) {
			//	    // Check if we actually have a stable parent connection
			//	    if (instance->current_dag != NULL && 
			//		instance->current_dag->preferred_parent != NULL) {
			//		
			//		// Check if parent is responding (based on your app logic)
			//		// This code attempts to detect actual connectivity
			//		if (clock_seconds() - last_parent_response > CONNECTIVITY_TIMEOUT) {
			//		    LOG_INFO("Parent hasn't responded for %u seconds, leaving DAG\n", 
			//			CONNECTIVITY_TIMEOUT);
			//		    
			//		    // Force leaving the DAG - this is the key to reset joined status
			//		    rpl_remove_parent(instance->current_dag, instance->current_dag->preferred_parent);
			//		    rpl_set_default_instance(NULL);
			//		    
			//		    // Clear the routing table
			//		    uip_ds6_route_t *r;
			//		    while((r = uip_ds6_route_head()) != NULL) {
			//			uip_ds6_route_rm(r);
			//		    }
			//		    
			//		    leds_off(LEDS_YELLOW);
			//		    leds_off(LEDS_RED);
			//		}
			//	    }
			//	}
			//}

			if (!is_coordinator) {
				if (NETSTACK_ROUTING.node_is_reachable()) {
					if (route != NULL) {
						leds_off(LEDS_RED);
						leds_on(LEDS_YELLOW);
						last_parent_response = clock_seconds();
					} else {
						leds_off(LEDS_YELLOW);
						leds_on(LEDS_RED);
					} 			
				} else {
					NETSTACK_ROUTING.local_repair("the fuck");
					leds_off(LEDS_YELLOW);
					leds_off(LEDS_RED);
				}
			}


			while(route) {
				LOG_INFO("Route ");
				LOG_INFO_6ADDR(&route->ipaddr);
				LOG_INFO_(" via ");
				LOG_INFO_6ADDR(uip_ds6_route_nexthop(route));
				LOG_INFO_("\n");
				route = uip_ds6_route_next(route);
			}
			NETSTACK_ROUTING.global_repair("the fuck");
			PROCESS_YIELD_UNTIL(etimer_expired(&et));
			etimer_reset(&et);
		}
	}

	PROCESS_END();
}
