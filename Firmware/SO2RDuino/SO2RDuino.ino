// SO2Rduino - An SO2R Box built on an Arduino clone
//
// Copyright 2010, Paul Young
// mods by DF5RF 2023
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

#define PSTR(x)         (x)
// Switches and inputs
static boolean          ptt_switch;     // PTT switch is active
static boolean          ptt_computer;   // Computer asserts PTT
static byte             ptt_debounce;   // PTT switch debounce (ms)

static byte             tx_switch;      // Front panel TX switch
static byte             rx_switch;      // Front panel RX switch
static boolean          stereo_switch;  // RX1+RX2 pressed says RX stereo
static byte             tx_debounce;    // TX switch debounce (ms)
static byte             rx_debounce;    // RX switch debounce (ms)
static byte             debounce;       // Debounce timer
static boolean          ptt;            // ptt signal detected

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
#define BUFLEN 64
static char inBuf[BUFLEN];
static String commandString;
static int commandReceived;             // 0 or len if command 

// Forward references
static boolean check_switches();
static void do_relays();
static void do_ptt();
static void check_key_ptt();
static void do_command();

void
setup()
//----------------------------------------------------------------------
// Initialize the SO2RDuino
//----------------------------------------------------------------------
{

  pinMode(8, OUTPUT); // HPL switches to R2 or back R1
  pinMode(9, OUTPUT); // 
  pinMode(10, OUTPUT);// HPR switches to R2 or back R1; STEREO switches to Left to Radio 1 and Right to R2 
  pinMode(11, OUTPUT);// TX switches TX2 to TXout (keyer signal, mono (hand/computer key))
  pinMode(12, OUTPUT);// 
  pinMode(13, OUTPUT);// switch attenuator for SDR on PTT
  
  pinMode(7, INPUT); 
  pinMode(6, INPUT); 
  pinMode(5, INPUT); // CW-out Signal
  pinMode(4, INPUT); // Special-button
  pinMode(3, INPUT); // PTT Radio 2
  pinMode(2, INPUT); // PTT Radio 1   


    tx_switch = RADIO_AUTO;
    rx_switch = RADIO_AUTO;
    debounce = millis();
    rx_debounce = DEBOUNCE;
    tx_debounce = DEBOUNCE;
    commandReceived=0;
    
    // Initialize to TX1, RX1, mono, not transmitting
    tx2 = false;
    rx2 = false;
    stereo = false;
    tx2_computer = false;
    rx2_computer = false;
    stereo_computer = false;
    stereo_switch = false;

    // all events on by default
    event_tx = true;
    event_rx = true;
    event_ab = true;
    event_cr0 = true;
    
    aux1 = 0;
    aux2 = 0;

    // The values for mono and latch are stored in EEPROM
    mono = false; // do not use that eeprom_read_byte(EEPROM_MONO);
    latch = eeprom_read_byte(EEPROM_LATCH);

    // Initialize the uart
    Serial.begin(38400);
    while(!Serial) {
      ;
    }
    
    // Wait for the switches to settle
    delay(DEBOUNCE);

    //check_switches();
    do_relays();
    /*
    CLEAR_RX2;
    delay(500);
    SET_RX2;
    delay(500);
    SET_TX1;
    delay(500);
    SET_TX2;
    delay(500);
    CLEAR_RX2;
    delay(500);
    SET_TX1;
    delay(500);*/
}

void
loop()
//----------------------------------------------------------------------
// Main loop
//----------------------------------------------------------------------
{
    if (check_switches()) {
        do_relays();
        stereo_switch=false;
    }

    check_key_ptt();
    if (commandReceived>3) {
        commandString.toCharArray(inBuf,BUFLEN);
        commandReceived=0;
        commandString="";
        //Serial.write("\rrcvd:");Serial.write(inBuf);
        do_command();
    }
    else if (commandReceived>0) {
        //Serial.write("?\r");
    }
    
    check_key_ptt();
}

void serialEvent() {
  while (Serial.available()) {
    char ib = (char)Serial.read();
    commandString += ib;
    if (ib == '\r') {
      commandReceived = 4;
    }
  }
}

byte SerialSend_prog_string(char s1[BUFLEN]) {
  return Serial.write(s1);
}
byte SerialSend_string(char s2[BUFLEN]) {
  return Serial.write(s2);
}
byte SerialSend_char(char ce) {
  return Serial.write(ce);
}
void Serial_clear() {
  
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
        if (!GET_TX_TOGGLE) {
            // wait until release
            while (!GET_TX_TOGGLE) {
              delay(10);
            }
            tx_debounce = DEBOUNCE;
            if (!tx2) {
                tx_switch = RADIO_2;
                changed = true;
            }
            else {
                tx_switch = RADIO_1;
                changed = true;
            }
        }
     }

    // Check the front-panel RX switch
    if (rx_debounce) {
        rx_debounce--;
    }
    else {
      bool swrx1=!GET_RX1_SWITCH;
      bool swrx2=!GET_RX2_SWITCH;
        if (swrx1 && !swrx2) {
            if (rx2) {
                rx_debounce = DEBOUNCE;
                rx_switch = RADIO_1;
                changed = true;
            }
        }
        if (swrx2 && !swrx1) {
            if (!rx2) {
                rx_debounce = DEBOUNCE;
                rx_switch = RADIO_2;
                changed = true;
            }
        }
        if (swrx1 && swrx2) {
            rx_debounce = DEBOUNCE;
            stereo_switch=true;
            rx_switch = RADIO_1;
            changed = true;
        }
    }
 #ifdef PTTSW
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
                    SerialSend_prog_string(PSTR("$CR00\r"));
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
                    SerialSend_prog_string(PSTR("$CR01\r"));
                }
            }
        }
    }
    #endif
    return changed;
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
#ifdef SIMPLE_SWITCHING
    if (tx2_computer) {
        SET_TX2;
        CLEAR_TX1;
    }
    else {
        SET_TX1;
        CLEAR_TX2;
    }
    if (rx2_computer) {
      SET_RX2;
    }
    else {
      CLEAR_RX2;
    }
    tx2=tx2_computer;
    rx2=rx2_computer;
    return;
#endif
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
        stereo_current = stereo_switch;
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
                SerialSend_prog_string(PSTR("$AB\r"));
            }
        }
        if (event_tx) {
           SerialSend_prog_string((tx2_current) ? PSTR("$TX2\r") : PSTR("$TX1\r"));
        }
    }

    // Send a receive changed event if desired
    if (event_rx && ((rx2 != rx2_current) || (stereo != stereo_current))) {
        SerialSend_prog_string((rx2_current) ? PSTR("$RX2") : PSTR("$RX1"));
        SerialSend_prog_string((stereo_current) ? PSTR("S\r") : PSTR("\r"));
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
            CLEAR_STEREO;
            SET_RX2;
            CLEAR_RX1_LED;
            SET_RX2_LED;
        }
        else {
            CLEAR_STEREO;
            CLEAR_RX2;
            SET_RX1_LED;
            CLEAR_RX2_LED;
        }
    }
}


static void
check_key_ptt()
//----------------------------------------------------------------------
// Check the CW key to derive PTT info
//----------------------------------------------------------------------
{
  static int keying=0;
  static int period=0;
  keying+=GET_CW_KEY;
  period++;
  if (period>15) {
    if (keying>5) {
      if (!ptt) {
        //SerialSend_string("$PTT1\r");
      }
      ptt=true;
    }
    else {
      if (ptt) {
        //SerialSend_string("$PTT0\r");
      }
      ptt=false;
    }
    period=0;
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
   
#define COMPARE(command)  (strncmp(command,inBuf,sizeof(command)-1)==0)
#define COMPARE2(command) (inBuf[0] == command[0] && inBuf[1]==command[1])

#define QCOMPARE(command) (strncmp(command,&inBuf[1],sizeof(command)-1)==0)
#define QCOMPARE2(command) (inBuf[1] == command[0] && inBuf[2]==command[1])
#define QCOMPARE3(command) (inBuf[1] == command[0] && inBuf[2]==command[1] && inBuf[3]==command[2])

#define AFTER(command) (sizeof(command)-1)

// Parse the commands

    // Handle the queries
    if (inBuf[0] == '?') {

        // Check for 'ping' - just the ? by itself
        if (inBuf[AFTER("?")] == '\0') {
            SerialSend_prog_string(PSTR("?\r"));
            return;
        }

        // Return TX1 or TX2
        if (QCOMPARE2("TX")) {
            SerialSend_prog_string((tx2) ? PSTR("TX2\r") : PSTR("TX1\r"));
           return; 
        }

        // Return RX1 or RX2 or RX1S or RX2S
        if (QCOMPARE2("RX")) {
            SerialSend_prog_string((rx2) ? PSTR("RX2") : PSTR("RX1"));
            SerialSend_prog_string((stereo) ? PSTR("S\r") : PSTR("\r"));
            return;
        }

        // Return AUX1 value
        if (QCOMPARE("AUX1")) {
            SerialSend_prog_string(PSTR("AUX1"));
            if (aux1 >= 10) {
                SerialSend_char('1');
                SerialSend_char((aux1-10) + '0');
            }
            else {
                SerialSend_char(aux1 + '0');
            }
            SerialSend_char('\r');
            return;
        }

        // Return AUX2 value
        if (QCOMPARE("AUX2")) {
            SerialSend_prog_string(PSTR("AUX2"));
            if (aux2 >= 10) {
                SerialSend_char('1');
                SerialSend_char((aux2-10) + '0');
            }
            else {
                SerialSend_char(aux2 + '0');
                SerialSend_char('\r');
             }
            return;
        }
        
        // Return the SO2R device's name
        if (QCOMPARE("NAME")) {
            SerialSend_prog_string(PSTR("NAMESO2Rduino\r"));
            return;
        }
        
        // Return the state of the footswitch
        if (QCOMPARE("CR0")) {
            SerialSend_prog_string((ptt_switch)
                ? PSTR("CR01\r") : PSTR("CR00\r"));
            return;
        }
        
        // Return whether footswitch events are enabled
        if (QCOMPARE("ECR0")) {
            SerialSend_prog_string((event_cr0)
                ? PSTR("ECR01\r") : PSTR("ECR00\r"));
            return;
        }

        // Return whether receiver events are enabled
        if (COMPARE("ERX")) {
            SerialSend_prog_string((event_rx)
                ? PSTR("ERX1\r") : PSTR("ERX0\r"));
            return;
        }
    
        // Return whether transmitter events are enabled
        if (COMPARE("ETX")) {
            SerialSend_prog_string((event_tx)
                ? PSTR("ETX1\r") : PSTR("ETX0\r"));
            return;
        }
        
        // Return whether mono is forced (no stereo allowed)
        if (COMPARE("VMONO")) {
            SerialSend_prog_string((mono)
                ? PSTR("VMONO1\r") : PSTR("VMONO0\r"));
            return;
            }
        
        // Return whether latch feature is on
        if (COMPARE("VLATCH")) {
            SerialSend_prog_string((latch)
                ? PSTR("VLATCH1\r") : PSTR("VLATCH0\r"));
            return;
        }
 
        // Unknown query command
        SerialSend_string(inBuf);
        SerialSend_char('\r');
        return;
    }

    // Handle transmitter change
    if (COMPARE2("TX")) {
        tx2_computer = (inBuf[AFTER("TX")] == '2');
        do_relays();
       return; 
    }

    // Handle receiver change
    if (COMPARE2("RX")) {
        rx2_computer = (inBuf[AFTER("RX")] == '2');
        stereo_computer = ((inBuf[AFTER("RXn")] == 'S') ||
                  (inBuf[AFTER("RXn")] == 'R'));
        do_relays();
        return;
    }

    // Handle direct relay: PQ[1..6][0|1]
    if (COMPARE2("PQ")) {
        byte rel=inBuf[AFTER("TX")]-0x30+7;
        byte val=inBuf[AFTER("TX")+1]-0x30;
        /*
        if (rel==13 && (val==1 || val==0)) {
          Serial.write("PQ-13-OK");
        }
        else {
          Serial.write("PQ-13-ERR");
        }*/
        digitalWrite(rel,val>0?HIGH:LOW);
       return; 
    }


  
    // Turn on or off PTT footswitch events
    if (COMPARE("ECR0")) {
        event_cr0 = (inBuf[AFTER("ECRn")] == '1');
        if (event_cr0) {
            SerialSend_prog_string((ptt_switch)
                ? PSTR("$CR01\r") : PSTR("$CR00\r"));
        }
        return;
    }
    
    // Turn on or off receiver events
    if (COMPARE("ERX")) {
        event_rx = (inBuf[AFTER("ERX")] == '1');
        if (event_rx) {
            SerialSend_prog_string((rx2) ? PSTR("$RX2") : PSTR("$RX1"));
            SerialSend_prog_string((stereo) ? PSTR("S\r") : PSTR("\r"));
        }
        return;
    }

    // Turn on or off transmitter events
    if (COMPARE("ETX")) {
        event_tx = (inBuf[AFTER("ETX")] == '1');
        if (event_tx) {
             SerialSend_prog_string((tx2) ? PSTR("$TX2\r") : PSTR("$TX1\r"));
        }
        return;
    }
    
    // Turn on or off forced mono feature
    if (COMPARE("VMONO")) {
        boolean n;
        n = (inBuf[AFTER("VMONO")] == '1');
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
        n = (inBuf[AFTER("VLATCH")] == '1');
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
