#include <Arduino.h>

#define DCBUS2_OFFSET_BITS 0
#define DCBUS2_V_PER_BIT 1.234
#define DCBUS1_OFFSET_BITS 74
#define DCBUS1_V_PER_BIT 0.560

#define TARGET_VOLTAGE 150
#define DCBUS1_PIN A0
#define DCBUS2_PIN A1
#define BOOST_LOW_SWITCH_PIN  9
#define BOOST_HIGH_SWITCH_PIN 10

#define EVERY_N_MILLISECONDS(ms) for(static unsigned long t0 = 0; ENM_compare_and_update(t0, ms); )

static unsigned long timestamp_age(unsigned long timestamp_ms)
{
	return millis() - timestamp_ms;
} 
static bool ENM_compare_and_update(unsigned long &t0, const unsigned long &interval)
{
	bool trigger_now = timestamp_age(t0) >= interval;
	if(trigger_now)
		t0 = millis();
	return trigger_now;
}

volatile int16_t dcbus1_raw = 0;
volatile int16_t dcbus2_raw = 0;
volatile int16_t input_voltage_V = 0;
volatile int16_t output_voltage_V = 0;


// Times are referenced to ICR1 value (ICR1 = 100%, 0 = 0%)
static void set_ontimes(uint16_t lowswitch_offtime, uint16_t highswitch_ontime)
{
	// OC1A: Low side
	OCR1A = lowswitch_offtime;
	// OC1B: High side
	OCR1B = highswitch_ontime;

	// We need to rewind TCNT1 to OCR1A so that the lowside gets turned off.
	// Otherwise it will stay on for a full PWM cycle until it reaches the
	// lowered OC1A on the next cycle and by that point we've had quite the
	// boost pulse. At 10kHz such a pulse will create about 50V of extra voltage
	// on the MG side.
	if(TCNT1 >= OCR1A) {
		TCNT1 = OCR1A-1;
  }
} 

// Sets high side switch PWM, and low side switch PWM based on it
static void set_pwm_buck_active(uint16_t highswitch_ontime)
{
	set_ontimes(ICR1+1, highswitch_ontime);
}

static void set_pwm_boost_active(uint16_t lowswitch_ontime)
{
	if(lowswitch_ontime > ICR1) {
		lowswitch_ontime = ICR1;
  }
	uint16_t lowswitch_offtime = ICR1 - lowswitch_ontime;
	set_ontimes(lowswitch_offtime, 0);
}

static void set_pwm_inactive()
{
	set_ontimes(ICR1, 0);
}

void setup() {
	Serial.begin(115200);
	Serial.println("Starting DCDC converter");
}

void loop() {
    dcbus2_raw = analogRead(DCBUS2_PIN) - DCBUS2_OFFSET_BITS;
	input_voltage_V = ((int32_t)dcbus2_raw * (int32_t)(DCBUS2_V_PER_BIT*1000)) / 1000;
	dcbus1_raw = analogRead(DCBUS1_PIN) - DCBUS1_OFFSET_BITS;
	output_voltage_V = ((int32_t)dcbus1_raw * (int32_t)(DCBUS1_V_PER_BIT*1000)) / 1000;
  
  EVERY_N_MILLISECONDS(200){
		Serial.print(output_voltage_V);
		Serial.print(" ");
		Serial.println(input_voltage_V);
	}

  if(input_voltage_V < TARGET_VOLTAGE && ((millis()/1000)&1)==0){
		set_pwm_boost_active(ICR1 * 0.01);
	} else if (input_voltage_V > TARGET_VOLTAGE && ((millis()/1000)&1)==0) {
    	set_pwm_buck_active(ICR1 * 0.01);
	} else {
		set_pwm_inactive();
    }

}
