#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

void increment_second();
void increment_minute(uint8_t ripple);
void increment_hour();
void timer_init();
uint8_t read_switch1();
uint8_t read_switch2();

volatile struct {
    uint8_t seconds;
    unsigned int one_minute : 4;
    unsigned int ten_minute : 4;
    unsigned int one_hour : 4;
    unsigned int ten_hour : 4;
} data = {0, 7, 3, 3, 1};

int main()
{
    // shut all peripherals off, we will enable as needed
    power_all_disable();

    // PORTB is wired up to blanking transistors
    // 0011 1110
    DDRB |= 0x3E;
    PORTB &= ~0x3E;
    
    // PORTC is wired up to K155 BCD IC
    // 0000 1111
    DDRC |= 0x0F;
    PORTC &= ~0x0F;
    
    // PD7 is wired to switch input, with external pulldown
    // PD2 is wired to power signal
    DDRD &= ~((1<<PD7)|(1<<PD2));
    PORTD &= ~((1<<PD7)|(1<<PD2));
    
    // wait for system / clocks to stabilize
    _delay_ms(250);
    
    timer_init();

    for (;;) {
        PORTC = data.ten_hour;
        PORTB |= (1<<PB1);
        _delay_ms(1);
        if (read_switch1()) {
            data.seconds = 0;
            increment_hour();
        }
        
        PORTB &= ~(1<<PB1);
        _delay_ms(1);

        // colon on
        PORTB |= (1<<PB2);
        
        PORTC = data.one_hour;
        PORTB |= (1<<PB3);
        _delay_ms(1);
        
        if (read_switch2()) {
            data.seconds = 0;
            increment_minute(0);
        }
        
        // colon off
        PORTB &= ~(1<<PB2);
        
        PORTB &= ~(1<<PB3);
        _delay_ms(1);
        
        PORTC = data.ten_minute;
        PORTB |= (1<<PB4);
        _delay_ms(1);
        PORTB &= ~(1<<PB4);
        _delay_ms(1);
        
        PORTC = data.one_minute;
        PORTB |= (1<<PB5);
        _delay_ms(1);
        PORTB &= ~(1<<PB5);
        _delay_ms(1);
    }

    return 0;
}

void timer_init()
{
    // ensure power is on for timer/counter2
    power_timer2_enable();
    
    // disable overflow interupt
    TIMSK2 &= ~(1<<TOIE2);
    
    // clock from external clock, DS32kHz
    ASSR |= (1<<AS2) | (1<<EXCLK);
    
    // clear counter
    TCNT2 = 0;
    
    // divide by 128 clock prescaler
    TCCR2B = (1<<CS22)|(1<<CS20);
    
    // wait TCCR2B to update
    while (ASSR & ((1<<TCR2BUB)|(1<<TCN2UB))) {}
    
    // enable overflow interrupt
    TIMSK2 = (1<<TOIE2);
    
    // enable interupts
    sei();
}

// timer2 overflow
ISR(TIMER2_OVF_vect)
{
    increment_second();
//    increment_minute(1);
}

void increment_second()
{
    if (++data.seconds == 60) {
        data.seconds = 0;
        increment_minute(1);
    }
}

void increment_minute(uint8_t ripple)
{
    if (++data.one_minute == 10) {
        data.one_minute = 0;
        
        if (++data.ten_minute == 6) {
            data.ten_minute = 0;
            
            if (ripple) {
                increment_hour();
            }
        }
    }
}

void increment_hour()
{
    ++data.one_hour;
    
    if (data.one_hour == 10) {
        data.one_hour = 0;
        ++data.ten_hour;
    } else if (data.ten_hour == 2 && data.one_hour == 4) {
        data.one_hour = 0;
        data.ten_hour = 0;
    }
}

uint8_t read_switch1()
{
    static uint16_t state = 0;
    state = (state<<1) | ((PIND&(1<<PD7))==0) | 0xe000;
    return (state==0xf000);
}

uint8_t read_switch2()
{
    static uint16_t state = 0;
    state = (state<<1) | ((PIND&(1<<PD7))==0) | 0xe000;
    return (state==0xf000);
}

