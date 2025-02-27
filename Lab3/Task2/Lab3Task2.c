#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "dev/leds.h"
#include "net/routing/rpl-classic/rpl.h"
#include "net/ipv6/uip-debug.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG
#define DEBUG DEBUG_PRINT
#define ROOT_NODE 1

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
	int is_coordinator = 0;

	PROCESS_BEGIN();

	is_coordinator = (node_id == ROOT_NODE);

	if (is_coordinator) 
	{
		NETSTACK_ROUTING.root_start();
    	leds_on(LEDS_GREEN);
	}
	NETSTACK_MAC.on();

	static struct etimer et;
	/* Print out routing tables every 10 seconds */
	etimer_set(&et, CLOCK_SECOND * 10);

	while(1) {
		is_coordinator = (node_id == ROOT_NODE);
		if (is_coordinator == ROOT_NODE) {
			LOG_INFO("I am the root node!\n");
		} 

		LOG_INFO("Routing entries: %u\n", uip_ds6_route_num_routes());
		uip_ds6_route_t *route = uip_ds6_route_head();

		if (!is_coordinator) {
			if (NETSTACK_ROUTING.node_has_joined()) {
				if (route != NULL && 
					(NETSTACK_ROUTING.node_is_reachable() || !NETSTACK_ROUTING.node_is_reachable())) {
					NETSTACK_ROUTING.local_repair("Intermediate Node");
					leds_off(LEDS_RED);
					leds_on(LEDS_YELLOW);
					leds_off(LEDS_GREEN);
				} else if (route == NULL && 
						  (NETSTACK_ROUTING.node_is_reachable() || !NETSTACK_ROUTING.node_is_reachable())) {
							NETSTACK_ROUTING.local_repair("Leaf Node");
							leds_off(LEDS_YELLOW);
							leds_on(LEDS_RED);
							leds_off(LEDS_GREEN);
				} 			
			} else {
				leds_off(LEDS_YELLOW);
				leds_off(LEDS_RED);
				leds_off(LEDS_GREEN);
			}
		}else {
			leds_on(LEDS_GREEN);
		}

		while(route) {
			LOG_INFO("Route ");
			LOG_INFO_6ADDR(&route->ipaddr);
			LOG_INFO_(" via ");
			LOG_INFO_6ADDR(uip_ds6_route_nexthop(route));
			LOG_INFO_("\n");
			route = uip_ds6_route_next(route);
		}

		PROCESS_YIELD_UNTIL(etimer_expired(&et));
		etimer_reset(&et);
	}

	PROCESS_END();
}