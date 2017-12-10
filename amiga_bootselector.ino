///////////////////////////////////////
// Amiga Bootselector
//
// (c) 2017 Marko Oette (http://retro.oette.info)
// 
// https://github.com/moette/amiga_bootselector
//

/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
   PORTB Wiring

      ,------->/SEL0 (out)
      | ,----->/SEL2 (out)
      | | ,--->LED1  (out)
   B00111111
       | | '-->LED2  (out)
       | '---->/SEL3 (out)
       '------>/SEL1 (out)
*/

/*
   PIND Wiring

      ,-------</SEL0 (in)
      | ,-----</SEL2 (in)
      | | ,---
   B00111111
       | | '--
       | '----</SEL3 (in)
       '------</SEL1 (in)


*/

#include <EEPROM.h>

#define PINMASK  B00111100
#define SEL0MASK B00100000
#define SEL0POS 5
#define SEL1MASK B00010000
#define SEL1POS 4
#define SEL2MASK B00001000
#define SEL2POS 3
#define SEL3MASK B00000100
#define SEL3POS 2

#define LED1MASK B00000010
#define LED2MASK B00000001

#define CIA_RESET A5          // LOW Active

#define WOMLOADDELAY 500

#define EPROM_ADDR_SETUPFLAG  1
#define EPROM_ADDR_ACTIVEFLAG 2

bool b_led1On = false;        // led 1 is turned on?
bool b_led2On = false;        // led 2 is turned on?
bool b_enabled = true;        // true if the swapping of SEL lines is enabled
bool b_waitForReady = true;   // true when the main loop is waiting for the drives to be querried

short SRC1 = SEL0POS, DST1 = SEL1POS;

/**
 * Will flash the LED times.
 * Blocks execution 180ms x times!!
 */
inline void flashLED(short mask, short times, int dly = 80 )
{
  while( times --> 0 )
  {
      // Blink the LEDs
      PORTB = mask;
      delay(dly);
      PORTB = 0;
      delay(dly);
  }
}

/**
 * Swaps two bits in the given byte.
 */
inline unsigned short swapBits(unsigned short n, unsigned short p1, unsigned short p2)
{
  return (((n >> p1) & 1) == ((n >> p2) & 1) ? n : ((n ^ (1 << p2)) ^ (1 << p1)));
}

/**
 * Called periodically to forward digital readings from /SEL0-3 to the output pins.
 */
short old = 0;
inline void forward(bool swapped)
{
  unsigned short rd = (PINMASK & PIND);
  
  if( swapped )
    PORTB = (b_led1On ? LED1MASK : 0) | (b_led2On ? LED2MASK : 0) | (swapBits(rd, DST1, SRC1)); // DF0, DF1
  else
    PORTB = (b_led1On ? LED1MASK : 0) | (b_led2On ? LED2MASK : 0) | rd;

  // Animate LEDs
  /* if( old != rd )
  {
    old = rd;
    b_led1On = b_led1On ? false : true;
  }*/
}

/**
 * Waits until the /SEL3 line was triggered or /SEL0 was active for at least 500ms (A1K).
 */
inline void waitForReady()
{
  // get start time
  unsigned long stime = millis();
  
  // Wait for the drives to be scanned by kickstart or for the A1k WOM loader...
  while (true)
  {
    // Pass through the org. values...
    forward(false);

    // If SEL3 goes low the last drive is scanned and we can switch
    if ( ( PIND & SEL3MASK ) != SEL3MASK )
    {
      delay(10);
      break;
    }
    // TODO: Add Amiga 1000 WOM Loader detection code
    /* else if( ( PIND & SEL0MASK ) == SEL0MASK && (micros() - wtime) > WOMLOADDELAY)
    {
      // If SEL0 is LOW for a longer period, the A1k WOM LOADER is active
      b_led2On = true;
      b_led1On = false;
      break;
    }*/
  }
}

/**
 * Init the atmega. 
 * Set the output pins. 
 * Read the setup from the EEPROM.
 */
void setup()
{
  // Enable outputs
  DDRB = DDRB | B00111111;

  // Read EPROM
  if( ( EEPROM.read(EPROM_ADDR_SETUPFLAG) == 255 ) )
  {
    // EEPROM was never written.
    flashLED((LED1MASK|LED2MASK), 3, 50);
    
    // Write defaults.
    EEPROM.write(EPROM_ADDR_ACTIVEFLAG, 1);
    b_enabled = true;

    //TODO READ+WRITE DEST-SRC1+2

    // Done. Save setup sate.
    EEPROM.write(EPROM_ADDR_SETUPFLAG, 1);
  }
  else
  {
    b_enabled = ( EEPROM.read(EPROM_ADDR_ACTIVEFLAG) == 1 );

    //TODO READ+WRITE DEST-SRC1+2
  }

  // Turn LEDs on to signal we are alive
  // PORTB = (LED1MASK | (b_enabled ? LED2MASK:0));
  

  // Wait for RESET line to go HIGH
  while( digitalRead(CIA_RESET) == LOW )
  {
    
  }
}

/**
 * Main loop. 
 * Handles RESET line.
 * Handles drive initialization.
 * Handles SEL0-3 forwarding.
 */
void loop()
{
  // Handle RESET
  if( digitalRead(CIA_RESET) == LOW )
  {
    unsigned long rstart = millis();
    while(digitalRead(CIA_RESET) == LOW)
    {
      flashLED(LED1MASK, 1);
    }
    unsigned long rduration = millis() - rstart;
    if( rduration > 6000 )
    {
      //RESET TO DEFAULTS!
      EEPROM.update(EPROM_ADDR_SETUPFLAG, 255 );
      flashLED(LED1MASK, 5, 40);
    }
    else if( rduration > 2500 )
    {
        // TOGGLE ON/OF
        b_enabled = (b_enabled ? false : true);
        EEPROM.update(EPROM_ADDR_ACTIVEFLAG, b_enabled ? 1 : 0);

        flashLED(LED2MASK, 3, 50);
    }
    b_waitForReady = true;
  }

  // Wait for Kickstart / WOM Loader if enabled
  if( b_enabled && b_waitForReady )
  {
    b_led1On = true;
    b_led2On = true;
    waitForReady();
    b_led1On = false;
    b_led2On = false;
    b_waitForReady = false;
  }

  // Forward the SEL signals
  forward(b_enabled);
}
