
#include <avr/sleep.h>
#include <avr/wdt.h>

#define ONBOARDLED 1                      //LED on pin 1
#define BTN_PIN 0
#define INTERRUPT_PIN 2
#define KEEP_RUNNING 2000         //milliseconds
#define BODS 7                     //BOD Sleep bit in MCUCR
#define BODSE 2                    //BOD Sleep enable bit in MCUCR

volatile unsigned long lastTriggered = 0;
volatile boolean triggered = false;

void setup(void)
{
    //to minimize power consumption while sleeping, output pins must not source
    //or sink any current. input pins must have a defined level; a good way to
    //ensure this is to enable the internal pullup resistors.

    for (byte i=0; i<5; i++) {     //make all pins inputs with pullups enabled
        pinMode(i, INPUT);
        digitalWrite(i, HIGH);
    }

    pinMode(ONBOARDLED, OUTPUT);          //make the led pin an output
    digitalWrite(ONBOARDLED, LOW);        //drive it low so it doesn't source current
    
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN, INPUT_PULLUP);
    
    blink(300, 3);
}

void loop(void)
{
    goToSleep();

    ledsRollAnimation(); // Wake up signal
    
    short count = 0;
    
    boolean awake = true;
    boolean running = false;
    unsigned long lastCommand = 0;
    unsigned long deltaTime = 0;
    unsigned long start = millis();

    //delay(500); // Waiting for command from interrupt
        
    GIFR  |= _BV(PCIF);    // clear any outstanding interrupts
    GIMSK |= _BV(PCIE); // Turn on Pin Change interrupt
    PCMSK |= _BV(PCINT0); // Which pins are affected by the interrupt
    
    // Loop that run while the LED are animated
    while(awake) {
    
        if (triggered) {
            deltaTime = millis() - lastTriggered;
            if (digitalRead(BTN_PIN) == HIGH) { // not down
                triggered = false;
                if (running) { // cancel animation and go to sleep
                    awake = false;
                    break;
                } else {
                    count++;
                }
            } else if (deltaTime > 1500) { // If still down, long press
                    changeLedsMode();
                    triggered = false;
            }
            delay(50); // debounce
            lastCommand = millis();
        }

        // Timeout to run LED
        if (!running){
            if (count > 0 && (millis() - lastCommand > 2000)) {
                running = true;
            
                // Blink to show the time
                blink(500, count);
                
            } else if (count == 0 && (millis() - start > 30000)) { // Config Timeout
                awake = false;
                break;
            }
        } else {
            // Do led animation
            delay(KEEP_RUNNING);
            awake = false;
        }
        
        //delay(KEEP_RUNNING);           //opportunity to measure active supply current 
    }
}

/**
 * Leds methods below
 */

void showLedsCount(short c)
{
    blink(c * 10, 1);
}

void changeLedsMode()
{
    blink(700, 1);
}

void ledsRollAnimation() 
{
    fastBlink();
}

/**
 * Meta methods below
 */

void fastBlink()
{
    blink(100, 5);
}

void blink(int d, int n)
{
  for (byte i=0; i<n; i++) {     //wake up, flash the LED
        digitalWrite(ONBOARDLED, HIGH);
        delay(d);
        digitalWrite(ONBOARDLED, LOW);
        delay(d);
    }
}


void goToSleep(void)
{
    fastBlink();
    byte adcsra, mcucr1, mcucr2;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
    GIMSK = 0; // Disable all interrupt
    GIMSK |= _BV(INT0);                       //enable INT0
    adcsra = ADCSRA;                          //save ADCSRA
    ADCSRA &= ~_BV(ADEN);                     //disable ADC
    cli();                                    //stop interrupts to ensure the BOD timed sequence executes as required
    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    mcucr2 = mcucr1 & ~_BV(BODSE);            //if the MCU does not have BOD disable capability,
    MCUCR = mcucr1;                           //  this code has no effect
    MCUCR = mcucr2;
    sei();                                    //ensure interrupts enabled so we can wake up again
    sleep_cpu();                              //go to sleep
    sleep_disable();                          //wake up here
    ADCSRA = adcsra;                          //restore ADCSRA

    //GIFR  |= _BV(PCIF);    // clear any outstanding interrupts
    //GIMSK |= _BV(PCIE); // Turn on Pin Change interrupt
    //PCMSK |= _BV(PCINT0); // Which pins are affected by the interrupt
}

//external interrupt 0 wakes the MCU
ISR(INT0_vect)
{
    GIMSK = 0;                   //disable external interrupts (only need one to wake up)
}

ISR(PCINT0_vect)
{
    triggered = true;
    //lastTriggered = digitalRead(BTN_PIN) == HIGH ? millis() : 0;
    lastTriggered = millis();
}

