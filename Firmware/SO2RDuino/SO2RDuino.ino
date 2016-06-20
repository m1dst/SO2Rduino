// SO2Rduino - An SO2R Box built on an Arduino clone
//
// Copyright 2010, Paul Young

// This is a simple implementation of an OTRSP SO2R Device.
// See http://www.k1xm.org/OTRSP for information on the
// protocol.

// This implementation does not use the Arduino libraries
// for much.  It also does not use interrupts except for
// the one ms timer set up by the Arduino environment.
// As long as the main loop takes less than about 1 ms in
// the worst case it can keep up with the UART which is
// run at 9600 baud.

#include "Arduino.h"
#include "avr/pgmspace.h"
#include "pins.h"
#include "eeprom.h"
#include "uart.h"

// Misc. definitions
#define DEBOUNCE 10                     // Debounce time (ms)

// The front panel switches have three values.  They are
// declared as byte because the compiler generates better
// code that way.
typedef enum {
    RADIO_AUTO = 0,
    RADIO_1,
    RADIO_2
} radio_switch;

// Switches and inputs
static boolean          ptt_switch;     // PTT switch is active
static boolean          ptt_computer;   // Computer asserts PTT
static byte             ptt_debounce;   // PTT switch debounce (ms)

static byte             tx_switch;      // Front panel TX switch
static byte             rx_switch;      // Front panel RX switch
static byte             tx_debounce;    // TX switch debounce (ms)
static byte             rx_debounce;    // RX switch debounce (ms)
static byte             debounce;       // Debounce timer

// Outputs
static boolean          tx2;            // Radio 2 selected for TX
static boolean          rx2;            // Listening to radio 2
static boolean          stereo;         // Listening in stereo
static boolean          tx2_computer;   // Computer says select TX 2
static boolean          rx2_computer;   // Computer says select RX 2
static boolean          stereo_computer;// Computer says RX stereo

// Aux outputs
static byte             aux1;           // Aux 1 output
static byte             aux2;           // Aux 2 output

// OTRSP events
static boolean          event_tx;       // TX event is enabled
static boolean          event_rx;       // RX event is enabled
static boolean          event_ab;       // Abort event is enabled
static boolean          event_cr0;      // PTT switch event is enabled

// Special and misc state
static boolean          mono;           // Stereo is not allowed
static boolean          latch;          // Move phones to non-TX radio

// Forward references
static boolean check_switches();
static void do_relays();
static void do_ptt();
static void do_aux();
static void check_key_ptt();
static void do_command();

void
setup()
//----------------------------------------------------------------------
// Initialize the SO2RDuino
//----------------------------------------------------------------------
{

    // Set up the directions, pull-ups, and initial values for the ports.
    // The port-specific values are all in pins.h
    DDRB = PORT_B_DDR;
    PORTB = PORT_B_PULL;

    DDRC = PORT_C_DDR;
    PORTC = PORT_C_PULL;

    DDRD = PORT_D_DDR;
    PORTD = PORT_D_PULL;

    tx_switch = RADIO_AUTO;
    rx_switch = RADIO_AUTO;
    debounce = millis();
    rx_debounce = 0;
    tx_debounce = 0;;

    // Initialize to TX1, RX1, mono, not transmitting
    tx2 = false;
    rx2 = false;
    stereo = false;
    tx2_computer = false;
    rx2_computer = false;
    stereo_computer = false;

    event_tx = false;
    event_rx = false;
    event_ab = false;
    event_cr0 = false;
    
    aux1 = 0;
    aux2 = 0;

    // The values for mono and latch are stored in EEPROM
    mono = eeprom_read_byte(EEPROM_MONO);
    latch = eeprom_read_byte(EEPROM_LATCH);

    // Initialize the uart
    uart_init();
    
    // Set up the aux port
    do_aux();
    
    // Wait for the switches to settle
    delay(DEBOUNCE);

    check_switches();
    do_relays();

}

void
loop()
//----------------------------------------------------------------------
// Main loop
//----------------------------------------------------------------------
{
    if (check_switches()) {
        do_relays();
    }

    check_key_ptt();
    
    if (do_uart()) {
        do_command();
        uart_clear_buffer();
    }
    
    check_key_ptt();
}

static boolean
check_switches()
//----------------------------------------------------------------------
// Check front panel switches and the PTT switch
//----------------------------------------------------------------------
{
    
    // When a switch opens or closes the contacts can bounce a
    // few times, opening and closing before it settles.  To
    // eliminate this the code ignores the switch for a few
    // milliseconds after it changes.
    byte now = millis();
    boolean changed = false;

    // Switches are closed when LOW

    // Only do once per millisecond max
    if (now == debounce) {
        return false;
    }
    debounce = now;
    
    // Check the front-panel TX switch
    if (tx_debounce) {
        tx_debounce --;
    }
    else {
        if (!GET_TX1_SWITCH) {
            if (tx_switch != RADIO_1) {
                tx_debounce = DEBOUNCE;
                tx_switch = RADIO_1;
                changed = true;
            }
        }
        else {
            if (!GET_TX2_SWITCH) {
                if (tx_switch != RADIO_2) {
                    tx_debounce = DEBOUNCE;
                    tx_switch = RADIO_2;
                    changed = true;
                }
            }
            else {
                if (tx_switch != RADIO_AUTO) {
                    tx_debounce = DEBOUNCE;
                    tx_switch = RADIO_AUTO;
                    changed = true;
                }
            }
        }
    }

    // Check the front-panel RX switch
    if (rx_debounce) {
        rx_debounce--;
    }
    else {
        if (!GET_RX1_SWITCH) {
            if (rx_switch != RADIO_1) {
                rx_debounce = DEBOUNCE;
                rx_switch = RADIO_1;
                changed = true;
            }
        }
        else {
            if (!GET_RX2_SWITCH) {
                if (rx_switch != RADIO_2) {
                    rx_debounce = DEBOUNCE;
                    rx_switch = RADIO_2;
                    changed = true;
                }
            }
            else {
                if (rx_switch != RADIO_AUTO) {
                    rx_debounce = DEBOUNCE;
                    rx_switch = RADIO_AUTO;
                    changed = true;
                }
            }
        }
    }
 
    // Check the PTT switch
    // Note:  PTT switch is LOW when active.
    if (ptt_debounce) {
        ptt_debounce--;
    }
    else {
        if (GET_PTT_SWITCH) {
            // Switch is open
            if (ptt_switch) {
                // Switch was closed
                ptt_debounce = DEBOUNCE;
                ptt_switch = false;
                do_ptt();
                if (event_cr0) {
                    uart_send_prog_string(PSTR("$CR00\r"));
                }
            }
        }
        else {
            // Switch is closed
            if (!ptt_switch) {
                // Switch was closed
                ptt_debounce = DEBOUNCE;
                ptt_switch = true;
                do_ptt();
                if (event_cr0) {
                    uart_send_prog_string(PSTR("$CR01\r"));
                }
            }
        }
    }
    return changed;
}

static void
do_ptt()
//----------------------------------------------------------------------
// Set or clear PTT
//----------------------------------------------------------------------
{
    if (ptt_computer || ptt_switch) {
        if (tx2) {
            SET_PTT2;
        }
        else {
            SET_PTT1;
        }
    }
    else {
        CLEAR_PTT;
    }
}

static void
do_relays()
//----------------------------------------------------------------------
// Set the relays as appropriate
//----------------------------------------------------------------------
{
    boolean tx2_current;
    boolean rx2_current;
    boolean stereo_current;

    // The switches get priority.  Otherwise do what the computer says.
    if (tx_switch == RADIO_AUTO) {
        tx2_current = tx2_computer;
    }
    else {
        tx2_current = (tx_switch == RADIO_2);
    }

    // RX is similar to TX except that the computer can say stereo.
    // The switches get priority.  Otherwise do what the computer says.
    if (rx_switch == RADIO_AUTO) {
        rx2_current = rx2_computer;
        stereo_current = stereo_computer && !mono;
    }
    else {
        rx2_current = (rx_switch == RADIO_2);
        stereo_current = false;
    }

    if (tx2 != tx2_current) {
        // If the abort event is going to be sent do it before sending
        // the transmit changed event.  This is because it takes a few
        // milliseconds to send an event and the abort is more important.
        if (ptt_switch || ptt_computer || (!GET_CW_KEY)) {
            // Transmitting while changing the transmitter.  Do what is possible
            // to stop this and send an abort if the event is turned on.
            CLEAR_CW;
            CLEAR_PTT;
            if (event_ab) {
                uart_send_prog_string(PSTR("$AB\r"));
            }
        }
        if (event_tx) {
           uart_send_prog_string((tx2_current) ? PSTR("$TX2\r") : PSTR("$TX1\r"));
        }
    }

    // Send a receive changed event if desired
    if (event_rx && ((rx2 != rx2_current) || (stereo != stereo_current))) {
        uart_send_prog_string((rx2_current) ? PSTR("$RX2") : PSTR("$RX1"));
        uart_send_prog_string((stereo_current) ? PSTR("S\r") : PSTR("\r"));
    }

    tx2 = tx2_current;
    rx2 = rx2_current;
    stereo = stereo_current;

    // Do the latch after checking because it is vendor-specific and
    // does not cause an event.
    if ((rx_switch == RADIO_AUTO) && ptt_computer && latch) {
        rx2_current = !tx2_current;
        stereo_current = false;
    }

    // Now set the relays and the LEDs

    if (tx2_current) {
        SET_TX2;
        CLEAR_TX1;
    }
    else {
        SET_TX1;
        CLEAR_TX2;
    }

    //For stereo The RX2 relay must not be set.
    if (stereo_current) {
        CLEAR_RX2;
        SET_STEREO;
        SET_RX1_LED;
        SET_RX2_LED;
    }
    else {
        if (rx2_current) {
            SET_RX2;
            CLEAR_STEREO;
            CLEAR_RX1_LED;
            SET_RX2_LED;
        }
        else {
            CLEAR_RX2;
            CLEAR_STEREO;
            SET_RX1_LED;
            CLEAR_RX2_LED;
        }
    }
}

static void
do_aux()
//----------------------------------------------------------------------
// Set the Auxiliary ports
//----------------------------------------------------------------------
{
    byte data;
    byte i;
    
    // Two of the pins are shared with relays.  Switching
    // the relays can put junk in the shift register but
    // it does ot matter because the junk is never latched
    // to the output.
    
    CLEAR_AUX_STROBE;

    data = aux2;
    
    // Each port is four bits.  Check the most significant
    // bit, then the next etc.  The 74HC594 clocks at a max
    // of 5 MHz, add some delay to make sure it has time for
    // each part of the cycle.
    for (i=0; i<4; i++) {
        
        // Set the clock low
        CLEAR_AUX_CLK;
        asm("nop");
        asm("nop");
        
        // Set the data bit
        if (data & 0x08) {
            SET_AUX_DATA;
        }
        else {
            CLEAR_AUX_DATA;
        }
        asm("nop");
        
        // Set up for the next bit
        data <<= 1;
        
        // Raise the clock pin to latch the data
        SET_AUX_CLK;
        asm("nop");
        asm("nop");
    }
    
    data = aux1;

    for (i=0; i<4; i++) {
        
        // Set the clock low
        CLEAR_AUX_CLK;
        asm("nop");
        asm("nop");
        
        // Set the data bit
        if (data & 0x08) {
            SET_AUX_DATA;
        }
        else {
            CLEAR_AUX_DATA;
        }
        asm("nop");
        
        // Set up for the next bit
        data <<= 1;
        
        // Raise the clock pin to latch the data
        SET_AUX_CLK;
        asm("nop");
        asm("nop");
    }
    
    // Raise the strobe to latch the data in the
    // outputs
    SET_AUX_STROBE;
    
    // The relays now probably have the wrong bits
    // set.  This took a few microseconds, they have
    // not even started to move yet.  Set them back.
    do_relays();
}

static void
check_key_ptt()
//----------------------------------------------------------------------
// Check the CW key and computer PTT
//----------------------------------------------------------------------
{
    // If the key is closed the result is LOW
    if (GET_CW_KEY) {
        CLEAR_CW;
    }
    else {
        if (tx2) {
            SET_CW2;
        }
        else {
            SET_CW1;
        }
    }
    
    // If DTR is set the result is LOW
    if (GET_PTT_DTR) {
        if (ptt_computer) {
            ptt_computer = false;
            do_ptt();
        }
    }
    else {
        if (!ptt_computer) {
            ptt_computer = true;
            do_ptt();
        }
    }
}

static void
do_command()
//----------------------------------------------------------------------
// Parse and handle a command from the computer
//----------------------------------------------------------------------
{
    // Commands are not checked very thoroughly - the computer
    // should not send garbage.
    
#define COMPARE(command)  (memcmp_P(in_buf, PSTR(command), \
                                    sizeof(command)-1) == 0)

#define QCOMPARE(command) (memcmp_P(in_buf+1, PSTR(command), \
                                    sizeof(command)-1) == 0)

#define AFTER(command) (sizeof(command)-1)

// Parse the commands

    // Handle the queries
    if (in_buf[0] == '?') {

        // Check for 'ping' - just the ? by itself
        if (in_buf[AFTER("?")] == '\0') {
            uart_send_prog_string(PSTR("?\r"));
            return;
        }

        // Return TX1 or TX2
        if (QCOMPARE("TX")) {
            uart_send_prog_string((tx2) ? PSTR("TX2\r") : PSTR("TX1\r"));
           return; 
        }

        // Return RX1 or RX2 or RX1S or RX2S
        if (QCOMPARE("RX")) {
            uart_send_prog_string((rx2) ? PSTR("RX2") : PSTR("RX1"));
            uart_send_prog_string((stereo) ? PSTR("S\r") : PSTR("\r"));
            return;
        }

        // Return AUX1 value
        if (QCOMPARE("AUX1")) {
            uart_send_prog_string(PSTR("AUX1"));
            if (aux1 >= 10) {
                uart_send_char('1');
                uart_send_char((aux1-10) + '0');
            }
            else {
                uart_send_char(aux1 + '0');
            }
            uart_send_char('\r');
            return;
        }

        // Return AUX2 value
        if (QCOMPARE("AUX2")) {
            uart_send_prog_string(PSTR("AUX2"));
            if (aux2 >= 10) {
                uart_send_char('1');
                uart_send_char((aux2-10) + '0');
            }
            else {
                uart_send_char(aux2 + '0');
                uart_send_char('\r');
             }
            return;
        }
        
        // Return the SO2R device's name
        if (QCOMPARE("NAME")) {
            uart_send_prog_string(PSTR("NAMESO2Rduino\r"));
            return;
        }
        
        // Return the state of the footswitch
        if (QCOMPARE("CR0")) {
            uart_send_prog_string((ptt_switch)
                ? PSTR("CR01\r") : PSTR("CR00\r"));
            return;
        }
        
        // Return whether footswitch events are enabled
        if (QCOMPARE("ECR0")) {
            uart_send_prog_string((event_cr0)
                ? PSTR("ECR01\r") : PSTR("ECR00\r"));
            return;
        }

        // Return whether receiver events are enabled
        if (COMPARE("ERX")) {
            uart_send_prog_string((event_rx)
                ? PSTR("ERX1\r") : PSTR("ERX0\r"));
            return;
        }
    
        // Return whether transmitter events are enabled
        if (COMPARE("ETX")) {
            uart_send_prog_string((event_tx)
                ? PSTR("ETX1\r") : PSTR("ETX0\r"));
            return;
        }
        
        // Return whether mono is forced (no stereo allowed)
        if (COMPARE("VMONO")) {
            uart_send_prog_string((mono)
                ? PSTR("VMONO1\r") : PSTR("VMONO0\r"));
            return;
            }
        
        // Return whether latch feature is on
        if (COMPARE("VLATCH")) {
            uart_send_prog_string((latch)
                ? PSTR("VLATCH1\r") : PSTR("VLATCH0\r"));
            return;
        }
 
        // Unknown query command
        uart_send_string(in_buf);
        uart_send_char('\r');
        return;
    }

    // Handle transmitter change
    if (COMPARE("TX")) {
        tx2_computer = (in_buf[AFTER("TX")] == '2');
        do_relays();
       return; 
    }

    // Handle receiver change
    if (COMPARE("RX")) {
        rx2_computer = (in_buf[AFTER("TX")] == '2');
        stereo_computer = ((in_buf[AFTER("TXn")] == 'S') ||
                  (in_buf[AFTER("TXn")] == 'R'));
        do_relays();
        return;
    }

    // Handle aux output 1 - values are four bits
    if (COMPARE("AUX1")) {
        aux1 = atoi((char *)&in_buf[AFTER("AUXn")]) & 15;
        do_aux();
        return;
    }
    
    // Handle aux output 2 - values are four bits
    if (COMPARE("AUX2")) {
        aux2 = atoi((char *)&in_buf[AFTER("AUXn")]) & 15;
        do_aux();
        return;
    }
    
    // Turn on or off PTT footswitch events
    if (COMPARE("ECR0")) {
        event_cr0 = (in_buf[AFTER("ECRn")] == '1');
        if (event_cr0) {
            uart_send_prog_string((ptt_switch)
                ? PSTR("$CR01\r") : PSTR("$CR00\r"));
        }
        return;
    }
    
    // Turn on or off receiver events
    if (COMPARE("ERX")) {
        event_rx = (in_buf[AFTER("ERX")] == '1');
        if (event_rx) {
            uart_send_prog_string((rx2) ? PSTR("$RX2") : PSTR("$RX1"));
            uart_send_prog_string((stereo) ? PSTR("S\r") : PSTR("\r"));
        }
        return;
    }

    // Turn on or off transmitter events
    if (COMPARE("ETX")) {
        event_tx = (in_buf[AFTER("ETX")] == '1');
        if (event_tx) {
             uart_send_prog_string((tx2) ? PSTR("$TX2\r") : PSTR("$TX1\r"));
        }
        return;
    }
    
    // Turn on or off forced mono feature
    if (COMPARE("VMONO")) {
        boolean n;
        n = (in_buf[AFTER("VMONO")] == '1');
        if (n != mono) {
            mono = n;
            eeprom_write_byte(EEPROM_MONO, mono);
            do_relays();
        }
        return;
    }
    
    // Turn on or off latch feature
    if (COMPARE("VLATCH")) {
        boolean n;
        n = (in_buf[AFTER("VLATCH")] == '1');
        if (n != latch) {
            latch = n;
            eeprom_write_byte(EEPROM_LATCH, latch);
            do_relays();
        }
        return;
    }
    
    // Unknown commands are ignored, as are commands that try to
    // set something which is read-only such as NAME or CR0.
    return;
}
