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


#if 0
void LP_CORE_ISR_ATTR ulp_lp_core_lp_io_intr_handler(void)
{
	ulp_lp_core_intr_disable();
    ulp_lp_core_gpio_clear_intr_status();
    IntEN = false;
    //lp_core_printf("IO INT\r\n");
}

void LP_CORE_ISR_ATTR ulp_lp_core_lp_timer_intr_handler(void)
{
    lp_timer_ll_clear_lp_alarm_intr_status(&LP_TIMER);
    ulp_lp_core_intr_disable();

	
}
#endif

int main (void)
{
    // Burn CPU time for a given amount of time so it's visible in the power profiler
    uint64_t now = ulp_lp_core_lp_timer_get_cycle_count();
    uint64_t then = now + ulp_lp_core_lp_timer_calculate_sleep_ticks(2000000); // 2s
    while (ulp_lp_core_lp_timer_get_cycle_count() < then)
    {
		// Burn CPU cycles now    	
    }    

	//lp_core_printf("Hello from the LP core!!\r\n");
	//ulp_lp_core_intr_enable();

	// Wake up the LP core after 3s
    ulp_lp_core_lp_timer_set_wakeup_time(1000000 * 3);
    // Enable interrupt now to wake up the cpu that's waiting for any interrupt
    LP_TIMER.lp_int_en.alarm = 1;

    ulp_lp_core_halt();    
}
