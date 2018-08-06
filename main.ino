// This #include statement was automatically added by the Particle IDE.
#include <Sunrise.h>









/*
Code for controlling a hydroponic monitoring system.
Current inputs are:
    - ph of reservoir
    - Liquid height
    - reservoir temperature
    - ambient light level

Current outputs are:
    - Timing for sprayer pump (s)
    - Dosing for nutrients
    - Dosing for ph fixing fluid
    - Mixing pump to ensure well mixed system after dosing of fluid.

Configurable variables are:
    - pump_on: How long to spray the plants for (ms)
    - pump_off: How long to wait between spraying sessions (ms)
    - ph_target: target ph value for the reservoir
    - ph_tolerance: how much the ph can deviate from the target
    - ph_fix_dose: how much of the ph fixing fluid to dose.
    - nutrients_volume: how much of the nutrient fluid to dose per dosing period.
    - nutrient_feed_period: how often to dose system with nutrients (s). In seconds because of size constraints writing to eeprom.
    - ph_fix_delay: how long to wait between pH fixng doses to dose again (ms)

*/



// ---------------------------------------
//  -------  Pin definitions  ------------
// ---------------------------------------
#define ph_sensor_pin A3
// #define temp_pin 11
// #define light_sensor_pin A1
const byte relay_pins[6] {D2,D3,D4,D5,D6, D7};
const int relay_on_vals[6] {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
const int relay_off_vals[6] {LOW, LOW, LOW, LOW, LOW, LOW};

#define liquid_heightpin A2

// ---------------------------------------
// -------- Constant Variables -----------
// ---------------------------------------

const int ph_sampling_array_length = 40;    //times of collection
const int ph_array_length = 40;
const int samplingInterval = 20;
const int printInterval = 800;
const float offset = 0.23; // Kind of a hack with the photon to get the pH to work.
const int lcd_height = 4;
const int lcd_width = 20;

// EEPROM addresses
const int ph_target_address = 0;
const int last_dose_address = 10;
const int light_period_address = 20;

int ph_array[ph_sampling_array_length];   //Store the average value of the sensor feedback
int ph_array_index=0; //starting index of array.

// Pins for relay shield output.
// Indices within the relay_pins array for which relays each pump corresponds to.
const int main_pump_index = 0;
const int mixing_pump_index = 2;
const int ph_fix_index = 3;
const int nutrient_peristaltic_index = 4;
// const int light_index = 5;
// const int extra_index = 5;

// peristaltic flow rate
const float peristaltic_flow_rate = 1.1; //  mL/s


// Vals that get changed every loop.
double current_ph = 0; // Instantiate current ph measurement

// ph Config
double ph_target; // Target ph
const double ph_target_default = 6.5;
const double ph_tolerance = 0.30; // How far ph can deviate from set point.


// Nutrient timing config
const float nutrients_volume = 1; // mL per period of feeding.
const long nutrient_feed_period = 21600; // feed every 6 hrs. This is in seconds.
unsigned long nutrient_last_dosed = 0; //ad This is in seconds.
long nutrient_run_time = 0; // This gets populated based on how much to dose and the flow rate.
bool nutrient_running = false;
long nutrient_last_checked = 0; // time at which it was turne on.


// pH Fixing Config. All of this is only for pH up. In the future would need to duplicate the
// last three variables for the pH down pump as well.
const float ph_fix_dose = 5; // mL
unsigned long ph_last_dosed = 20000; // Wait an additional 10s on startup to check pH.
const long ph_fix_delay = 60000; // How many milliseconds to wait after ph alteration to alter again.
long ph_run_time = 0; // How long a pH fix runs for. This could be hard coded as it is derived from
                      // the value above, but this gives more flexibility if you want to call it from
                      // the cloud API for a variable amount.
bool ph_running = false; // Is the pH fix pump running.
long ph_last_checked = 0; // Time at which the ph pump was turned on.


// Periodic pump config.
bool pump_running = false; // Starts off
const long pump_on =  30000; // pump on 30s
const long pump_off = 300000; //pump off 5mins
unsigned long pump_last_checked = 0;
unsigned long currentMillis = 0; // Non RTC Millis
unsigned long current_time = 0; // Time to be read from RTC.

// Mixing pump config.
const long mixing_pump_time = 5000; // Mix for 5s when specified.
bool mixing_pump_running = false; // Starts off
long mixing_pump_last_checked = 0; // time at which it was turned on.

// Variables that get formatted specially when displayed to the cloud variables
String prs; // pump running string
String last_dosed_string; // last dosed string
String paused_s; // if system is paused string.

// If you want to pause the system set to true.
bool system_paused = false;

// Light related stuff
const double default_light_period = 43200;
double light_period;
bool light_on = false;
double sunrise_seconds; // Minutes after midnight of sunrise, for a given day.
long lamp_turned_on;
int today; // Current day. For detecting a new day for turning on light.
int day_light_last_on = 99; // Dummy value for checking whether or not it is a new
// day for turning the lamp on and off.
double current_lat = 45.5231;
double current_lon = -122.6765;

// ---------------------------------------
// ---------- Particle Functions --------
// ---------------------------------------

bool s0 = Particle.function("doseNutrient", feed_system_iot);

bool s1 = Particle.function("mixSystem", mix_system_iot);

bool s2 = Particle.function("setPH", set_ph_iot);

bool s3 = Particle.function("togglePump", toggle_pump_iot);

bool s4 = Particle.function("togglePause", toggle_pause_iot);

bool s5 = Particle.function("hoursOfLight", set_light_period_iot);



// ---------------------------------------
// Setup Runs Once.
// ---------------------------------------

Sunrise sunrise(current_lat, current_lon, -7);
void setup() {

  sunrise.Actual(); // Not sure what this does. But example has it.

  for(int i = 0; i < 5; i++)  {
      pinMode(relay_pins[i],OUTPUT);
      digitalWrite(relay_pins[i], relay_off_vals[relay_pins[i]]);
  }
  // Really not sure why these need to be here. should have been
  // covered by the above loop. But that doesn't work....
  digitalWrite(relay_pins[ph_fix_index], HIGH);
  digitalWrite(relay_pins[nutrient_peristaltic_index], HIGH);

  Serial.begin(9600);
  Particle.syncTime();
  Time.zone(-7);

  Wire.begin();
  pinMode(ph_sensor_pin,INPUT);

  nutrient_last_dosed = read_last_fed_eeprom();
  ph_target = get_ph_setpoint();
  light_period = get_light_period();
  // If nothing has been set in EEPROM use this as default.
  if (ph_target == 0xFFFF) {
      ph_target = ph_target_default;
  }

  if (nutrient_last_dosed == 0xFFFF) {
      nutrient_last_dosed = Time.now();
  }

  if (light_period == 0xFFFF) {
      light_period = default_light_period;
  }

  Particle.variable("pH", current_ph);
  Particle.variable("pH_target", ph_target);
  Particle.variable("pump_running", prs);
  Particle.variable("last_dosed", last_dosed_string);
  Particle.variable("paused", paused_s);

    // Mix the reservoir to begin.
  mixing_pump_on();

}


// ---------------------------------------
// ------------ Main Loop ----------------
// ---------------------------------------

void loop() {
    // Various timekeeping updates.
    today = Time.day();
    sunrise_seconds = sunrise.Rise(Time.month(), Time.day()) * 60;
    currentMillis = millis();
    current_time = Time.local();

    // Each of these basically turns off the associated pumps that run in the background.
    handle_background_pump(&mixing_pump_running, mixing_pump_last_checked, mixing_pump_time, mixing_pump_index);
    handle_background_pump(&nutrient_running, nutrient_last_checked, nutrient_run_time, nutrient_peristaltic_index);
    handle_background_pump(&ph_running, ph_last_checked, ph_run_time, ph_fix_index);


    // These update the particle variables so they are well formatted.
    prs = bool_to_string(pump_running);
    paused_s = bool_to_string(system_paused);
    last_dosed_string = Time.format(nutrient_last_dosed, "%b %d %H:%m:%S");


    current_ph = get_ph(ph_sensor_pin);

    // If the system isn't paused then do all of the automatic dosing, mixing, etc.
    // The other tasks above occur every loop if not paused because they are
    // turning stuff off, not turning stuff on.
    if (!system_paused) {
        handle_main_pump();
        handle_nutrients();
        handle_ph();
        // handle_light();
    }

    delay(500);
}



// -------------------------------
// ----- EXPOSED PHOTON FUNCTIONS -----
// -------------------------------

int feed_system_iot(String mls_to_feed) {

    float volume_max = 20.0;

    float volume_f = float(atof(mls_to_feed));
    if (volume_f > volume_max) {
        return -1;
    }
    else {
        handle_nutrients_helper(float(volume_f));
        return 0;
    }
}


int mix_system_iot(String extra) {
    if (mixing_pump_running) {
        return -1;
    }
    else {
        mixing_pump_on();
        return 0;
    }

}

int toggle_pause_iot(String extra) {
    system_paused = !system_paused;
    Particle.publish("System (un)paused", String(system_paused));
    return 0;
}

int toggle_pump_iot(String extra) {
     if (pump_running) {
        turn_main_pump_off();
        return 0;
    }
    else {
        turn_main_pump_on();
        return 0;
    }
}

int set_ph_iot(String target_ph) {
    double ph_min = 4.0;
    double ph_max = 9.0;

    double ph_f = double(atof(target_ph));
    if (ph_f < ph_min || ph_f > ph_max) {
        Particle.publish("ph_target out of range", String(ph_f));
        return -1;
    }
    else {
        set_ph_setpoint(ph_f);
        Particle.publish("Updated ph_target", String(ph_f));
        return 0;
    }


}

int set_light_period_iot(String hours) {
    double hours_d = double(atof(hours));
    if (hours_d > 24.0) {
        Particle.publish("Hours > 24. Error");
        return -1;
    }
    else {
        set_light_period(hours_d);
        Particle.publish("Updated light_period", hours);
        return 0;
    }

}







// -------------------------------
// ----- MY FUNCTIONS -----
// -------------------------------


// ----- DOSING/TIMING STUFF -----

// Dose reservoir with pH fixer
void handle_ph() {
    // The second conditional is because of a weird thing when the value
    // is reset and on startup.
    if ((currentMillis - ph_last_dosed >= ph_fix_delay) & (currentMillis - ph_last_dosed < 1000000000)){
        // Can only handle pH being below target.
        Serial.println("pH Timing met");
        if (current_ph < ph_target - ph_tolerance) {
            handle_ph_helper();
            mixing_pump_on();

        }
    }
}

void handle_ph_helper() {
    dose_peristaltic(ph_fix_index, ph_fix_dose, &ph_running, &ph_last_checked, &ph_run_time);
    ph_last_dosed = currentMillis;
    Particle.publish("Raised pH!");
}


void mixing_pump_on() {
    Particle.publish("Mixed System");
    digitalWrite(relay_pins[mixing_pump_index], relay_on_vals[mixing_pump_index]);
    mixing_pump_running = true;
    mixing_pump_last_checked = currentMillis;
}

void handle_background_pump(bool *running, long turned_on, long run_time, int relay_index) {
    if (*running & (currentMillis - turned_on >= run_time)) {
        digitalWrite(relay_pins[relay_index], relay_off_vals[relay_index]);
        *running = false;
    }
}


// Dose nutrients into system. Foreground task. Cannot handle being turned off.
void handle_nutrients() {
    // If the main pump is dosing it might be unsafe to feed then. Because of
    // unmixed nutrients. Therefore don"t dose then.
    if (!pump_running) {
        if ((current_time - nutrient_last_dosed >= nutrient_feed_period)) {
            handle_nutrients_helper(nutrients_volume);
            mixing_pump_on();
        }
    }
}

void handle_nutrients_helper(float volume_to_dose) {
    dose_peristaltic(nutrient_peristaltic_index, volume_to_dose, &nutrient_running, &nutrient_last_checked, &nutrient_run_time);
    nutrient_last_dosed = current_time;
    // Write to the eeprom that the system was fed.
    write_last_fed_eeprom(nutrient_last_dosed);
    Particle.publish("Nutrients Dosed", String(volume_to_dose));
}

// void handle_light() {
//     double seconds_after_midnight = Time.hour() * 60 * 60 + Time.minute() * 60 + Time.second();
//
//     if ((!light_on) & (day_light_last_on != today) & (seconds_after_midnight > sunrise_seconds)) {
//         turn_light_on();
//     }
//     else if ((light_on) & (seconds_after_midnight > sunrise_seconds + light_period)) {
//         turn_light_off();
//     }
// }

// Helper function to run a peristaltic pump for a specified amount of volume.
// Foreground task.
void dose_peristaltic(int pump_pin_index, float volume, bool *running, long *turned_on, long *run_time) {
    long t_to_dose = (volume / peristaltic_flow_rate) * 1000; // 1000 to convert to milliseconds
    *run_time = t_to_dose;
    *running = true;
    *turned_on = currentMillis;
    digitalWrite(relay_pins[pump_pin_index], relay_on_vals[pump_pin_index]);
}

// Runs the main timing for the spraying pump. Could use similar logic for
// each of the two peristaltic dosing functions above as well.
// Background task.
void handle_main_pump() {
  // If the pump is off
 if (pump_running == false) {
   if (currentMillis - pump_last_checked >= pump_off) {
     // time is up, so turn the pump on.
     turn_main_pump_on();

   }
 }
 else {  // if the pump is running
   if (currentMillis - pump_last_checked >= pump_on) {
     // time is up, so turn the pump off.
    turn_main_pump_off();
   }
 }
}

void turn_main_pump_on() {

      digitalWrite(relay_pins[main_pump_index], relay_on_vals[main_pump_index]);
      pump_running = true;
      Particle.publish("Changed pump state", String(pump_running));
     // and save the time when we made the change
      pump_last_checked = currentMillis;
}

void turn_main_pump_off() {

      digitalWrite(relay_pins[main_pump_index], relay_off_vals[main_pump_index]);
      pump_running = false;
      Particle.publish("Changed pump state", String(pump_running));
         // and save the time when we made the change
      pump_last_checked = currentMillis;
}

// void turn_light_on() {
//     digitalWrite(relay_pins[light_index], relay_on_vals[light_index]);
//     light_on = true;
//     day_light_last_on = today;
//     Particle.publish("Changed light state", String(light_on));
// }

// void turn_light_off() {
//     digitalWrite(relay_pins[light_index], relay_off_vals[light_index]);
//     light_on = false;
//     Particle.publish("Changed light state", String(light_on));
//
// }
// ------ READING WRITING EEPROM --------

// Specify the value to write.
void write_last_fed_eeprom(long to_write) {
    EEPROM.put(last_dose_address, to_write);
}

long read_last_fed_eeprom() {
  return long(EEPROM.get(last_dose_address, nutrient_last_dosed));
}

void set_ph_setpoint(double to_write) {
    ph_target = to_write;
    EEPROM.put(ph_target_address, to_write);
}

double get_ph_setpoint() {
  return double(EEPROM.get(ph_target_address, ph_target));
}

double get_light_period() {
    return double(EEPROM.get(light_period_address, light_period));
}

void set_light_period(double hours) {
    double to_write = hours * 60 * 60;
    light_period = to_write;
    EEPROM.put(light_period_address, to_write);
}

String bool_to_string(bool some_bool) {
    if (some_bool) {
        return "True";
    }
    else {
        return "False";
    }
}



















// -------------------------------
// ----- FOUND FUNCTIONS -----
// -------------------------------


// Returns ph from a sensor specified by the pin.
float get_ph(int pin_to_check) {
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float phValue,voltage;
  if (millis()-samplingTime > samplingInterval)
  {
      ph_array[ph_array_index++]=analogRead(pin_to_check);
      if(ph_array_index==ph_array_length)ph_array_index=0;
      voltage = average_array(ph_array, ph_array_length) * 5.0 / 1024 / 6;
      phValue = 3.5 * voltage + offset;
      samplingTime=millis();
  }
  if (millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
        printTime=millis();
  }
  return phValue;
}

// Helper function for the get_ph function
double average_array(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if (number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  } else {
    if (arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for (i=2;i<number;i++){
      if (arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      } else {
        if (arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        } else{
          amount+=arr[i]; //min<=arr<=max
        }
      }
    }
    avg = (double)amount/(number-2);
  }
  return avg;
}




double c_to_f(double celsius) {
  return 1.8 * celsius + 32;
}


// Return distance from sonar sensor to target in cm. Not really accurate
// below 5cm.
// float get_dist() {
//       float conv = 29.1;
//       long duration, distance;
//       digitalWrite(trigPin, LOW);  // Added this line
//       delayMicroseconds(2); // Added this line
//       digitalWrite(trigPin, HIGH);
//       delayMicroseconds(10); // Added this line
//       digitalWrite(trigPin, LOW);
//       duration = pulseIn(echoPin, HIGH);
//       return (duration/2) / conv; // cm conversion
// }
