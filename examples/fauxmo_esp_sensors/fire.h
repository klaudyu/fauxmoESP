#ifndef FIRE_EFFECT_H
#define FIRE_EFFECT_H



#include <Preferences.h>
extern Preferences preferences;

class FireEffect {
private:
  bool state;
  int stp;
  float minratio;
  float maxIntensity;
  float rate;
  float brightness;
  float maxfreq;
  float minfreq;
  unsigned long last_time;
  String id;

public:

  void load_settings() {
    minratio = preferences.getFloat((id+"minratio").c_str(), 0.5);
    maxIntensity = preferences.getFloat((id+"maxIntensity").c_str(), 100);
    minfreq = preferences.getFloat((id+"minfreq").c_str(), 2);
    maxfreq = preferences.getFloat((id+"maxfreq").c_str(), 4);
    state = preferences.getBool((id+"state").c_str(), true);
    Serial.printf("Loaded settings: minratio=%f, maxIntensity=%f, minfreq=%f, maxfreq=%f, state=%d\n", minratio, maxIntensity, minfreq, maxfreq, state);
  } 

  FireEffect(const String&id,float minRatio = 0.5, float maxIntens = 100, float maxFreq = 4, float minFreq = 2)
      : id(id), stp(0), minratio(minRatio), maxIntensity(maxIntens), rate(1), brightness(0), maxfreq(maxFreq), minfreq(minFreq), last_time(millis())
  {
    load_settings();  
  }


  void set_state(bool value) { state = value; }
  /**
   * @brief Updates the brightness of the fire effect.
   * 
   * This function calculates the interval since the last update and determines the new brightness value based on the interval and the current brightness. 
   * If the interval is less than 10 milliseconds, the function returns the current brightness without making any changes.
   * If the interval is greater than or equal to 10 milliseconds, the function calculates the new brightness based on certain conditions and updates the brightness accordingly.
   * 
   * @return The updated brightness value.
   */
  float update() {
    float interval = millis() - last_time;
    if (state == false) return 0;
    if (interval < 10) return brightness;
    last_time = millis();

    if (stp <= 0) {
      // Generate a new random brightness value within the specified range
      float new_brightness = random(minratio * maxIntensity*1000, maxIntensity*1000)/1000;
      if (brightness > maxIntensity || brightness < minratio * maxIntensity) {
        // If the current brightness is outside the range, gradually adjust it towards the new brightness
        stp = 200 / interval;
        rate = (new_brightness - brightness) / stp;
      } else {
        // If the current brightness is within the range, calculate the rate of change based on the duration and interval
        int duration = random(1000*1000 / maxfreq, 1000*1000 / minfreq)/1000;
        rate = float(maxIntensity - minratio * maxIntensity) * interval / duration;
        if (new_brightness < brightness) {
          rate = -rate;
        }
        stp = (new_brightness - brightness) / rate;
        if (stp >500) stp = 500;
      }
    } else {
      // Gradually adjust the brightness towards the new brightness
      stp -= 1;
      brightness += rate;
    }
    return brightness;
  }

  void save_settings() {

    if (!preferences.putFloat((id+"minratio").c_str(), minratio) ||
        !preferences.putFloat((id+"maxIntensity").c_str(), maxIntensity) ||
        !preferences.putFloat((id+"minfreq").c_str(), minfreq) ||
        !preferences.putFloat((id+"maxfreq").c_str(), maxfreq) ||
        !preferences.putBool((id+"state").c_str(), state)) {
      Serial.println("Error saving preferences");
    }

    float _minratio = preferences.getFloat((id+"minratio").c_str());
    float _maxIntensity = preferences.getFloat((id+"maxIntensity").c_str());
    float _minfreq = preferences.getFloat((id+"minfreq").c_str());
    float _maxfreq = preferences.getFloat((id+"maxfreq").c_str());
    bool _state = preferences.getBool((id+"state").c_str());
    Serial.printf("Saved settings: minratio=%f, maxIntensity=%f, minfreq=%f, maxfreq=%f, state=%d\n", _minratio, _maxIntensity, _minfreq, _maxfreq, _state);

  }
  

  float get_intensity() { return maxIntensity; }
  void set_intensity(float value) { maxIntensity = value; stp=-1; }

  void set_wind(float value) { 
    minratio = 1-value/100;
    maxfreq = .2 + value/20;
    minfreq = .2 + value/40;
    stp=-1;
   }

  void set_max_intensity(float value) { maxIntensity = value; }
  void set_min_ratio(float value) { minratio = value; }
  void set_max_freq(float value) { maxfreq = value; }
  void set_min_freq(float value) { minfreq = value; }
};

#endif