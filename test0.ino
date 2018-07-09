
const byte relay_pins[6] {D2,D3,D4,D5,D6, D7};
#define ph_sensor_pin A1


const int ph_sampling_array_length = 40;    //times of collection
const int ph_array_length = 40;
const int samplingInterval = 20;
const int printInterval = 800;
const float offset = 0.42; // Kind of a hack with the photon to get the pH to work.


int ph_array[ph_sampling_array_length];   //Store the average value of the sensor feedback
int ph_array_index=0; //starting index of array.

double current_ph = 0; // Instantiate current ph measurement


void setup() {
  for(int i = 0; i < 6; i++)  {
    pinMode(relay_pins[i],OUTPUT);
  }
  Serial.begin(9600);
}

void loop() {
  // for(int i = 3; i < 6; i++)  {
  //   pinMode(relay_pins[i],OUTPUT);
  //   digitalWrite(relay_pins[i], HIGH);
  //   delay(1000);
  //   digitalWrite(relay_pins[i], LOW);
  //   delay(1000);
  //
  // }
  delay(1000);
  Serial.println("2");
  Serial.println("hello");
  current_ph = get_ph(ph_sensor_pin);
  Serial.println(current_ph);
}


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
