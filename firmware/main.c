#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;                                     
    uint8_t day;       
    uint8_t month;
    uint16_t year;      
} time;

static time t = {1, 1, 1, 24, 5, 1983};

static void timer_init() {
    // ensure power is on for timer/counter2
//    PRR &= ~(1<<PRTIM2);
    power_timer2_enable();

    // disable overflow interupt
    TIMSK2 &= ~(1<<TOIE2);

    // clock from external async watch 32.768Hz crystal on TOSC1/TOSC2
    ASSR |= (1<<AS2);

    // clear counter
    TCNT2 = 0;
    
    // divide by 128 clock prescaler
    TCCR2B = (1<<CS22)|(1<<CS20);

    // wait TCCR2B to update
    while (ASSR & ((1<<TCR2BUB)|(1<<TCN2UB))) {}
    
    // enable overflow interrupt
    TIMSK2 = (1<<TOIE2);

    // we will enable interupts later
//    sei();
}

int main() {
    // shut all peripherals off, we will enable as needed
    power_all_disable();

    // wait for system / clocks to stabilize
    _delay_ms(500);
    
    DDRD |= 0x0F;
    PORTD = 0x00;
    
    DDRB |= 1<<PB0;
    PORTB &= ~(1<<PB0);

    timer_init();
    
    do {
        set_sleep_mode(SLEEP_MODE_PWR_SAVE);
        cli();
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu();
        sleep_disable();
        
        // rinse, repeat
    } while(1);

    return 0;
}

// check for leap year
static uint8_t not_leap_year() {
    if (!(t.year%100))
        return (uint8_t)(t.year%400);
    else
        return (uint8_t)(t.year%4);
}

// timer2 overflow
ISR(TIMER2_OVF_vect)
{
    // time keeping from app note AVR134
    if (++t.second == 60) {
        t.second = 0;
        
        if (++t.minute == 60) {
            t.minute = 0;
            
            if (++t.hour == 24) {
                t.hour = 0;
                
                if (++t.day == 32) {
                    t.month++;
                    t.day = 1;
                } else if (t.day == 31) {                    
                    if ((t.month == 4) || (t.month == 6) || (t.month == 9) || (t.month == 11)) {
                        t.month++;
                        t.day=1;
                    }
                } else if (t.day == 30) {
                    if(t.month == 2) {
                        t.month++;
                        t.day = 1;
                    }
                } else if (t.day == 29) {
                    if((t.month == 2) && (not_leap_year())) {
                        t.month++;
                        t.day = 1;
                    }                
                }                          
                
                if (t.month == 13) {
                    t.month = 1;
                    t.year++;
                }
            }
        }
    }
    
    // tube off
    PORTB &= ~(1<<PB0);
    
    // output digit
    PORTD = (t.second%10); // & 0x0F;
    
    // tube on
    PORTB |= (1<<PB0);
}

