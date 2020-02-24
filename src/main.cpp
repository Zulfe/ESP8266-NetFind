#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <string.h>
#include <map>

// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"

/*
 * This is an array of pins for the LEDs you connect to the board.
 * You can add or remove pins at any time. The code has be written
 * to use an arbitrary number of pins, just make sure you update
 * the NUMPINS variable to reflect the number of defined pins.
 * This code will light them up in the order they're defined in
 * the array, so if {15, 16, 17} is used, and one network is found,
 * the LED connected to pin 15 will be lit. If two networks are found,
 * then the LEDs connected to pins 15 and 16 will be lit.
 */
const uint8_t LED_PINS[] = {32, 33, 25};
const uint8_t NUM_PINS = 3;

/*
 * This is your MAC address filter. The code is configured to compare the
 * MAC address of any found devices to the enabled segments in this filter.
 * You can thus filter on the first segment, the first three segments, just
 * the inner segments, or every other segment.
 * @see APPLY_FILTER
 * 
 * Note that MAC addresses are in hexadecimal, so each segment is really
 * an integer (between 0 and 255), not a string. When defining this filter,
 * use hexadecimal numbers by adding a '0x' before the two characters from
 * the MAC address. (e.g. 1A:2B:3C:4D:5E:6F to {0x1A, 0x2B, 0x3C, ...}).
 */
const uint8_t MAC_FILTER[6] = {0xAC, 0xA3, 0x1E, 0x00, 0x00, 0x00};

/*
 * This is your MAC address filter segment enable. The first boolean corresponds
 * to the first segment in the filter, the second boolean to the second segment
 * in the filter, etc. If all are set to true, you'll be scanning for a single
 * full MAC address.
 * 
 * This is pre-configured to enable filtering on the first three segments.
 */
const bool APPLY_FILTER[6] = {true, true, true, false, false, false};

/*
 * A constant to represent how many segments there are in a MAC address. You
 * don't really want to change this.
 */
const uint8_t NUM_MAC_SEGMENTS = 6;

/*
 * Flags for device count display methods. 
 * DISP_LEDS: display the count using discrete LEDs, connected to LED_PINS pins.
 * PRINT_FOUND_DEVICES: display the devices found by the WiFi scan, numbered, with RSSI.
 */
const bool DISP_LEDS = true;
const bool PRINT_FOUND_DEVICES = true;

/*
 * A map that corresponds the signal strength to a given device to the
 * String version of its MAC address. You can use an OLED screen to
 * display the signal strengths, which may be useful for triangulating
 * the physical location of the device.
 */
std::map<String, int32_t> deviceSignalMap;

/*
 * A variable to keep track of the number of devices that passed the filter. 
 */
uint16_t num_devices = 0;

/**
 * Searches for WiFi access points that meets the 
 * @param filter The first three segments of the MAC address to filter by.
 * Only devices with a MAC address whose first three segments match the provided
 * filter are considered in the count.
 * @returns The number of devices found that match the filter.
 */
uint16_t count_devices(const uint8_t filter[3]) {
	// Scan for local devices.
	int16_t scanResults = WiFi.scanNetworks(false, true);
	// Maintain a count of found devices that pass the filter.
	uint16_t num_filtered = 0;

	// If no devices were found, print a message.
	if (scanResults == 0) Serial.println("No WiFi devices in AP Mode found");

	// If some devices were found, filter and count them.
	else {
		Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices.");
    bool passes_filter = true;

		// For each of the scan results...
		for (int i = 0; i < scanResults; ++i) {
			// Retrieve further information about this results.
			String SSID = WiFi.SSID(i);
			int32_t RSSI = WiFi.RSSI(i);
			uint8_t* BSSID = WiFi.BSSID(i);
			String BSSIDstr = WiFi.BSSIDstr(i);

      if(PRINT_FOUND_DEVICES) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" - ");
        Serial.print(BSSIDstr);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }

      passes_filter = true;
      // Check if this device has a MAC address that passes the filter.
      for(uint8_t i = 0; i < NUM_MAC_SEGMENTS; i++) {
        if(APPLY_FILTER[i] && BSSID[i] != MAC_FILTER[i])
          passes_filter = false;
      }

      // If this device passed the filter...
      if(passes_filter) {
        // Map the BSSID (MAC address) to the measured signal strength.
				std::map<String, int32_t>::iterator it = deviceSignalMap.find(BSSIDstr);
				if(it != deviceSignalMap.end() && it->second != std::abs(RSSI))
					it->second = std::abs(RSSI);
				else
					deviceSignalMap.insert(std::make_pair(BSSIDstr, std::abs(RSSI)));

				// The device has passed the filter. Increment the number of found devices.
				num_filtered++;
      }

		}
	}

	// Clean up RAM by deleting the scan results.
	WiFi.scanDelete();

	return num_filtered;
}

/**
 * Display, either on an OLED display or using the LEDs defined by the LED_PINS pins,
 * the provided count of devices that passed the filter.
 */
void disp_count(const uint16_t count) {
	// If displaying the count using LEDs has been enabled, do so.
	if(DISP_LEDS) {
		for(uint8_t i = 0; i < NUM_PINS; i++) {
			if(i < count) {
				// Serial.println("Setting pin " + String(i) + " HIGH.");
				digitalWrite(LED_PINS[i], HIGH);
			}
			else {
				// Serial.println("Setting pin " + String(i) + " LOW.");
				digitalWrite(LED_PINS[i], LOW);
			}
		}
	}
}

void setup() {
	Serial.begin(115200);
	Serial.println();

	WiFi.persistent(false);

	// If device counts are to be displayed on LEDS, initialize the associated pins.
	if(DISP_LEDS) {
		for(uint8_t i = 0; i < NUM_PINS; i++) {
			pinMode(LED_PINS[i], OUTPUT);
			digitalWrite(LED_PINS[i], LOW);
		}
	}
}

void loop() {

	// Scan networks and count how many pass the filter.
	num_devices = count_devices(MAC_FILTER);

	Serial.println("Counted " + String(num_devices) + " devices that passed the MAC filter!");

	// Display the number of devices that passed the filter.
	disp_count(num_devices);
	
	delay(100);
}