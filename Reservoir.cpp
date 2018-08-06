#include "Arduino.h"
#include "Reservoir.h"


Reservoir::Reservoir((int pH_sensor_pin, int mixing_pump_pin,
          int pH_up_pin, int grow_nutrient_pin,
          int bloom_nutrient_pin,
          float pH_target)
{
  pinMode(pH_sensor_pin, INPUT);
  pinMode(mixing_pump_pin, OUTPU);
  pinMode(pH_up_pin, OUTPUT);
  pinMode(grow_nutrient_pin, OUTPUT);
  pinMode(bloom_nutrient_pin, OUTPUT);

  _pH_sensor_pin = pH_sensor_pin;
  _mixing_pump_pin = mixing_pump_pin;
  _pH_up_pin = pH_up_pin;
  _grow_nutrient_pin = grow_nutrient_pin;
  _bloom_nutrient_pin = bloom_nutrient_pin;
  _pH_target = pH_target;

  // Defined values for getting the pH.
  ph_sampling_array_length = 40;    //times of collection
  ph_array_length = 40;
  samplingInterval = 20;
  printInterval = 800;
  offset = 0.23; // Kind of a hack with the photon to get the pH to work.
  ph_array_index=0;

  // peristaltic flow rate
  peristaltic_flow_rate = 1.1; //  mL/s
  current_ph = 0; // Instantiate current ph measurement
  ph_tolerance = 0.30;
  ph_target_default = 6.5;

  // Nutrient timing config
  nutrients_volume = 1; // mL per period of feeding.
  nutrient_feed_period = 21600; // feed every 6 hrs. This is in seconds.
  nutrient_last_dosed = 0; //ad This is in seconds.
  nutrient_run_time = 0; // This gets populated based on how much to dose and the flow rate.
  nutrient_running = false;
  nutrient_last_checked = 0; // time at which it was turned on.

  // pH Fixing Config. All of this is only for pH up. In the future would need to duplicate the
  // last three variables for the pH down pump as well.
  ph_fix_dose = 3; // mL
  ph_last_dosed = 20000; // Wait an additional 10s on startup to check pH.
  ph_fix_delay = 60000; // How many milliseconds to wait after ph alteration to alter again.
  ph_run_time = 0; // How long a pH fix runs for. This could be hard coded as it is derived from
                        // the value above, but this gives more flexibility if you want to call it from
                        // the cloud API for a variable amount.
  ph_running = false; // Is the pH fix pump running.
  ph_last_checked = 0; // Time at which the ph pump was turned on.


  // Mixing pump config.
  mixing_pump_time = 5000; // Mix for 5s when specified.
  mixing_pump_running = false; // Starts off
  mixing_pump_last_checked = 0; // time at which it was turned on.


}

float Reservoir::get_ph()
{
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float phValue,voltage;
  if (millis()-samplingTime > samplingInterval)
  {
      ph_array[ph_array_index++]=analogRead(_pH_sensor_pin);
      if(ph_array_index==ph_array_length)ph_array_index=0;
      voltage = average_array(ph_array, ph_array_length) * 5.0 / 1024 / 6;
      pH_value = 3.5 * voltage + offset;
      samplingTime=millis();
  }
  if (millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
        printTime=millis();
  }
  return pH_value;
}


void Reservoir::handle_pumps()
{
  // Each of these basically turns off the associated pumps that run in the background.
  handle_background_pump(&mixing_pump_running, mixing_pump_last_checked, mixing_pump_time, _mixing_pump_pin);
  handle_background_pump(&nutrient_running, nutrient_last_checked, nutrient_run_time, _grow_nutrient_pin);
  handle_background_pump(&ph_running, ph_last_checked, ph_run_time, _pH_up_pin);



}


void Reservoir::handle_background_pump(bool *running,
                                       long turned_on,
                                       long run_time,
                                       int pin_index) {
    if (*running & (currentMillis - turned_on >= run_time)) {
        digitalWrite(pin_index, LOW);
        *running = false;
    }
}
