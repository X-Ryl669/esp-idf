#include <stdint.h>
#include <stdbool.h>
#include "ulp_lp_core.h"
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_gpio.h"
#include "ulp_lp_core_interrupts.h"
#include "ulp_lp_core_lp_timer_shared.h"
#include "riscv/csr.h"

#include "soc/lp_timer_struct.h"
#include "soc/rtc.h"
#include "hal/lp_timer_ll.h"
#include "hal/clk_tree_ll.h"
#include "hal/lp_core_ll.h"

volatile uint32_t logArray[256];
volatile uint32_t logIndex;

#if 0
void LP_CORE_ISR_ATTR ulp_lp_core_lp_io_intr_handler(void)
{
	ulp_lp_core_intr_disable();
    ulp_lp_core_gpio_clear_intr_status();
}

void LP_CORE_ISR_ATTR ulp_lp_core_lp_timer_intr_handler(void)
{
    lp_timer_ll_clear_lp_alarm_intr_status(&LP_TIMER);
    ulp_lp_core_intr_disable();

	
}
#endif

int main (void)
{
    // Remember the wake cause for this run
    ulp_lp_core_update_wakeup_cause();
    logArray[logIndex++] = ulp_lp_core_get_wakeup_cause();
#if 0
    if (logArray[logIndex - 1] & LP_CORE_LL_WAKEUP_SOURCE_LP_IO)
    {
    	// Clear IO interrupt now
    	ulp_lp_core_gpio_clear_intr_status();
    }
    else if (logArray[logIndex - 1] & LP_CORE_LL_WAKEUP_SOURCE_LP_TIMER)
    {
    	// Clear Timer interrupt now
	    lp_timer_ll_clear_lp_alarm_intr_status(&LP_TIMER);	
    }
#endif

    // Burn CPU time for a given amount of time so it's visible in the power profiler
    uint64_t now = ulp_lp_core_lp_timer_get_cycle_count();
    uint64_t then = now + ulp_lp_core_lp_timer_calculate_sleep_ticks(2000000); // 2s

    logArray[logIndex++] = (uint32_t)(now >> 32ULL);
    logArray[logIndex++] = (uint32_t)(now & 0xFFFFFFFF);
	if (logIndex > 100) 
	{
		ulp_lp_core_wakeup_main_processor();
	    ulp_lp_core_halt();
	}

    while (ulp_lp_core_lp_timer_get_cycle_count() < then)
    {
		// Burn CPU cycles now    	
    }    

  		lp_core_ll_set_wakeup_source(0); // Maybe this will kill the LP core definitively, let's test this
        ulp_lp_core_gpio_clear_intr_status();
        lp_timer_ll_clear_lp_alarm_intr_status(&LP_TIMER);	
        ulp_lp_core_intr_disable();
        ulp_lp_core_intr_enable();
  		lp_core_ll_set_wakeup_source(0x14); // Resume it once the interrupt are cleared

	// Wake up the LP core after 3s
    ulp_lp_core_lp_timer_set_wakeup_time(1000000 * 3);
    // Enable interrupt now to wake up the cpu that's waiting for any interrupt
    LP_TIMER.lp_int_en.alarm = 1;


    ulp_lp_core_halt();
}
