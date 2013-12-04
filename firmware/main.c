#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

#define ON_INTERVAL_US  600
#define OFF_INTERVAL_US 300
#define COLON_PWM 30

typedef struct {
    uint8_t seconds;
    unsigned int one_minute : 4;
    unsigned int ten_minute : 4;
    unsigned int one_hour : 4;
    unsigned int ten_hour : 4;
} display_t;

void increment_second(display_t* display);
void increment_minute(display_t* display, uint8_t ripple);
void increment_hour(display_t* display);
void toggle_display();
void timer_init();
void colon_init();
uint8_t read_switch1();
uint8_t read_switch2();
uint8_t read_switch3();

display_t clock = {0, 7, 3, 3, 1};
display_t timer;
display_t* current_display = &clock;

int main()
{
    // shut all peripherals off, we will enable as needed
    power_all_disable();

    // PORTB is wired up to blanking transistors, and switch 3
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
    
    colon_init();

    for (;;) {
        if (!current_display) {
            PORTC = 10;
            PORTB |= (1<<PB4);
            if (read_switch3()) {
                toggle_display();
            }
            PORTB &= ~(1<<PB4);
            _delay_ms(10);
        } else {
            // 10 hour, PB1
            PORTC = current_display->ten_hour;
            PORTB |= (1<<PB1);
            _delay_us(ON_INTERVAL_US);
            if (read_switch1()) {
                clock.seconds = 0;
                increment_hour(&clock);
            }
            PORTB &= ~(1<<PB1);
            
            _delay_us(OFF_INTERVAL_US);

            // 1 hour, PB3
            PORTC = current_display->one_hour;

            PORTB |= (1<<PB3);
            _delay_us(ON_INTERVAL_US);
            if (read_switch2()) {
                clock.seconds = 0;
                increment_minute(&clock, 0);
            }
            PORTB &= ~(1<<PB3);
            _delay_us(OFF_INTERVAL_US);
            
            // 10 minute, PB4
            PORTC = current_display->ten_minute;
            PORTB |= (1<<PB4);

            _delay_us(ON_INTERVAL_US);
            if (read_switch3()) {
                toggle_display();
            }
            PORTB &= ~(1<<PB4);
            _delay_us(OFF_INTERVAL_US);

            // 1 minute, PB5
            PORTC = current_display->one_minute;
            PORTB |= (1<<PB5);
            _delay_us(ON_INTERVAL_US);
            PORTB &= ~(1<<PB5);
            PORTB |= (1<<PB0);
            _delay_us(OFF_INTERVAL_US);
        }
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

void colon_init()
{
    // colon blanking is on PB2/OC1B
    power_timer1_enable();
    
    // set at bottom, clear on match
    TCCR1A = (1<<COM1B1) | (1<<WGM10);
    
    // fast 8bit pwm, clock from cpu
    TCCR1B = (1<<WGM12) | (1<<CS12);

    // trial and error to determined this is as dim as we can go without flickering
    OCR1B = COLON_PWM;
}

// timer2 overflow
ISR(TIMER2_OVF_vect)
{
    increment_second(&clock);
    if (current_display == &timer) {
        increment_minute(&timer, 1);
    }
}

void increment_second(display_t* display)
{
    if (++display->seconds == 60) {
        display->seconds = 0;
        increment_minute(display, 1);
    }
}

void increment_minute(display_t* display, uint8_t ripple)
{
    if (++display->one_minute == 10) {
        display->one_minute = 0;
        
        if (++display->ten_minute == 6) {
            display->ten_minute = 0;
            
            if (ripple) {
                increment_hour(display);
            }
        }
    }
}

void increment_hour(display_t* display)
{
    ++display->one_hour;
    
    if (display->one_hour == 10) {
        display->one_hour = 0;
        if (++display->ten_hour == 10) {
            display->ten_hour = 0;
        }
    } else if (display == &clock && (display->ten_hour == 2 && display->one_hour == 4)) {
        display->one_hour = 0;
        display->ten_hour = 0;
    }
}

void toggle_display()
{
    if (current_display == &clock) {
        timer.seconds = 0;
        timer.one_minute = 0;
        timer.ten_minute = 0;
        timer.one_hour = 0;
        timer.ten_hour = 0;
        current_display = &timer;
    } else if (current_display == &timer) {
        current_display = 0;
        TCCR1B &= ~(1<<CS12);
        PORTB &= ~(1<<PB2);
    } else {
        current_display = &clock;
        TCCR1B |= (1<<CS12);
        OCR1B = COLON_PWM;
    }
}

// hooked up to PB1
uint8_t read_switch1()
{
    static uint16_t state = 0;
    state = (state<<1) | ((PIND&(1<<PD7))==0) | 0xe000;
    return (state==0xf000);
}

// hooked up to PB3
uint8_t read_switch2()
{
    static uint16_t state = 0;
    state = (state<<1) | ((PIND&(1<<PD7))==0) | 0xe000;
    return (state==0xf000);
}

// hooked up to PB4
uint8_t read_switch3()
{
    static uint16_t state = 0;
    state = (state<<1) | ((PIND&(1<<PD7))==0) | 0xe000;
    return (state==0xf000);
}
