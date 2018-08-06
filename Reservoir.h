/*
  Reservoir.h - Library for handling aeroponic fluid reservoir.
  Michael Wiest August 2018
  Released into the public domain.
*/
#ifndef Reservoir_h
#define Reservoir_h

#include "Arduino.h"

class Reservoir
{
public:
  Reservoir(int pH_sensor_pin, int mixing_pump_pin,
            int pH_up_pin, int grow_nutrient_pin,
            int bloom_nutrient_pin,
            float pH_target);
  float get_ph();
  void handle_pumps();
  unsigned long ph_last_dosed; // Wait an additional 10s on startup to check pH.


private:
  // pH Sampling stuff
  const int ph_sampling_array_length;
  const int ph_array_length;
  const int samplingInterval;
  const int printInterval;
  const float offset;
  int ph_array[ph_sampling_array_length];   //Store the average value of the sensor feedback
  int ph_array_index; //starting index of array.


  // peristaltic flow rate
  const float peristaltic_flow_rate; //  mL/s
  float current_pH;
  const float ph_tolerance; // How far ph can deviate from set point.
  const float ph_target_default;

  // Nutrient timing config
  const float nutrients_volume; // mL per period of feeding.
  const long nutrient_feed_period; // feed every 6 hrs. This is in seconds.
  unsigned long nutrient_last_dosed; // This is in seconds.
  long nutrient_run_time; // This gets populated based on how much to dose and the flow rate.
  bool nutrient_running;
  long nutrient_last_checked; // time at which it was turne on.


  // pH Fixing Config. All of this is only for pH up. In the future would need to duplicate the
  // last three variables for the pH down pump as well.
  const float ph_fix_dose; // mL
  const long ph_fix_delay; // How many milliseconds to wait after ph alteration to alter again.
  long ph_run_time; // How long a pH fix runs for. This could be hard coded as it is derived from
                        // the value above, but this gives more flexibility if you want to call it from
                        // the cloud API for a variable amount.
  bool ph_running; // Is the pH fix pump running.
  long ph_last_checked; // Time at which the ph pump was turned on.



  // Mixing pump config.
  const long mixing_pump_time; // Mix for 5s when specified.
  bool mixing_pump_running; // Starts off
  long mixing_pump_last_checked; // time at which it was turned on.



  int _pH_sensor_pin;
  int _mixing_pump_pin;
  int _pH_up_pin;
  int _grow_nutrient_pin;
  int _bloom_nutrient_pin;
  float _pH_target;

  void handle_background_pump();


}


#endif
