#include <Adafruit_NeoPixel.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#define BTN_PIN 0
#define ONBOARDLED 1                      //LED on pin 1
#define INTERRUPT_PIN 2
#define LEDS_PIN 3
#define STRIPSIZE 8 // Limited by max 256 bytes ram. At 3 bytes/LED you get max ~85 pixels

#define KEEP_RUNNING 2000         //milliseconds
#define BODS 7                     //BOD Sleep bit in MCUCR
#define BODSE 2                    //BOD Sleep enable bit in MCUCR

Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, LEDS_PIN, NEO_GRB + NEO_KHZ800);

volatile unsigned long lastTriggered = 0;
volatile boolean triggered = false;

#define ROSE   0xFF00C5
#define YELLOW 0xFFFB00

#define NB_MODE 2
uint32_t colorMode = ROSE;
uint8_t mode = 0; //0 = random rose, 1 = rainbow wave

const byte brightness = 60;

void setup(void)
{
    //to minimize power consumption while sleeping, output pins must not source
    //or sink any current. input pins must have a defined level; a good way to
    //ensure this is to enable the internal pullup resistors.

    for (uint8_t i=0; i<5; i++) {     //make all pins inputs with pullups enabled
        pinMode(i, INPUT);
        digitalWrite(i, HIGH);
    }

    pinMode(ONBOARDLED, OUTPUT);          //make the led pin an output
    digitalWrite(ONBOARDLED, LOW);        //drive it low so it doesn't source current
    
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN, INPUT_PULLUP);
    
    strip.begin();
    strip.clear();
    strip.show();
    
    blink(300, 3);
}

void loop(void)
{
    goToSleep();

    strip.setBrightness(brightness);
    strip.show();
    
    ledsRollAnimation(); // Wake up signal
    
    uint8_t count = 1;
    
    boolean awake = true;
    boolean running = false;
    unsigned long lastCommand = 0;
    unsigned long deltaTime = 0;
    unsigned long start = millis();
    bool prevWaslongCommand = false;
    
    lastCommand = start;
    lastTriggered = start;
    triggered = false;
    
    GIFR  |= _BV(PCIF);    // clear any outstanding interrupts
    GIMSK |= _BV(PCIE); // Turn on Pin Change interrupt
    PCMSK |= _BV(PCINT0); // Which pins are affected by the interrupt
    
    // Loop that run while the LED are animated
    while(awake) {
    

        if (!running){
            if (triggered) {
                deltaTime = millis() - lastTriggered;
                if (digitalRead(BTN_PIN) == HIGH) { // not down
                    if(prevWaslongCommand == false) {
                        /* Update animation duration */
                        count++;
                        if (count > 8) {
                            count = 1;
                        }
                    } else {
                        // Button is just released from a long press
                        prevWaslongCommand = false;
                    }
                    triggered = false;
                } else if (deltaTime > 1000) { // If still down, long press
                    /* Change animation style */
                    switch(++mode) {
                    case 1:
                      colorMode = YELLOW;
                      break;
                    case 0:
                    default:
                        mode = 0;
                        colorMode = ROSE;
                    }
                    prevWaslongCommand = true;
                    triggered = false;
                }
                delay(80); // debounce
                lastCommand = millis();
            }
            
            showLedsCount(count);
            
            if (count > 0 && (millis() - lastCommand > 2000)) {
                /* Start animation */
                running = true;

                fadeOff(500);
                // TODO: start code for animation
                
            } else if (count == 0 && (millis() - start > 30000)) { // Config Timeout
                /* Timeout, go to sleep */
                awake = false;
                break;
            }
        } else {
           if (triggered) {
                deltaTime = millis() - lastTriggered;
                if (digitalRead(BTN_PIN) == HIGH) { // not down
                    triggered = false;
                    /* Cancel animation and go to sleep */
                    awake = false;
                    break;
                }
                delay(80); // debounce
            }
            /* TODO led animation */
            delay(KEEP_RUNNING);
            awake = false;
        }
    }
}

/**
 * Leds methods below
 */

void showLedsCount(short count)
{
    for(uint8_t i=0; i<STRIPSIZE; i++) {
        if (i < count)
            strip.setPixelColor(i, colorMode);
        else
            strip.setPixelColor(i, 0);
    }
  
    strip.show();
}

void ledsRollAnimation() 
{
    colorWipe(ROSE, 100);
    colorWipe(strip.Color(0, 0, 0), 75);
    colorWipe(ROSE, 100);
    fadeOff(1000);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint8_t i=0; i<STRIPSIZE; i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

/*
 * Duration of the fade in ms
 */
void fadeOff(uint8_t duration)
{
    uint8_t wait = duration / brightness;
    for(short i=brightness; i>=0; i--){
        strip.setBrightness(i);
        strip.show();
        delay(wait);
    }
    strip.clear();
    strip.setBrightness(brightness); // Reset brightness to default
    strip.show();
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
  for (uint8_t i=0; i<n; i++) {     //wake up, flash the LED
        digitalWrite(ONBOARDLED, HIGH);
        delay(d);
        digitalWrite(ONBOARDLED, LOW);
        delay(d);
    }
}


void goToSleep(void)
{
    fastBlink();

    strip.clear(); // Initialize all pixels to 'off'
    strip.setBrightness(0);
    strip.show(); // Update

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

