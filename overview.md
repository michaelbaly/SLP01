## general operation
+ send location and events report
+ communicate with backend server with IP message or SMS text message
+ device expect ack for every message transmitted
+ device will store report/events in absence of cell coverage
+ device will accept usr provided configurable parameter values(in range specified)
+ device provide preset values for user configurable parameter
+ device will ack every remote command received from user
+ device will respond to request for location with a location report
+ device will allow server to update/change configuration
+ device will send daily heartbeat location report --- **frequency based on configuration**.
+ device will send parking alert event when vehicle has been stationary for more
than one hour
+ device will support circular and polygan geofences.

## OTA
+ will work with ignition both on and off
+ replace the current image with new image
+ able to store two main image simultaneously
+ error happend in the download process, able to go back to the older one.
+ will ultilize the microprocessor resources, other capabilities will be suspended
during the process
+ will ultilize flash memory and will overwrite any stored messages if image require more memory
than available
+ upon conclusion or early termination of download, device will report the resulting status

## gps
+ device will receive gps signal at least once every 15sec when ignition is on
+ device will provide basic sanity check on new GPS location
+ if gps location is suspect:
1 save last good location; 2 provide location aging
+ drift control: define its location based on last "dynamic" GPS fix record

## watch mode
+ device provide initial vehicle location within 30sec
+ device provide updates location every 60min

## repo mode/theft mode
+ provide initial vehicle location within 20sec
+ updates on location every 3~5min

## late payment/audible warning
+ device trigger notification
+ device will notify the user that the late payment was delivered

## device engine starter disable/enable vehicle
+ the device will send respond to a request to disable vehicle by sending an ack
to the requet and sending GPS position at the time requeest received

## manual starter override
* have following features:
 + **1** MSO feature will be enabled whenever the vehicle is in starter-disable state;
 + **2** there will be a manual override counter to track and determine the number of
 times this feature get used;
 + **3** in order to initiate an ignition on/off cycle, the ignition needs to be off
 for 15sec
 + **4** manual override require the following key sequence to re-enable the starter;
 + **5** manual counter can be reset;
 + **6** "emergency enable"command will override MSO feature.

## device theft/tow away detection and reporting
+ detected via accelerometer
+ enable when ignition is off
+ tow event is generated if there is a change in postion that is more than a predefined
limit during ignition off

## external sensor support
+ sensor connected to analog inputs will be monitored periodically.
+ a user will be able to remotely control digital outputs

## data flow mirroring
+ ablity to mirror the data flow to a second back-up server.

## accelerometer --- “six degrees of motion detection”
+ be able to detect motion during ignition on/off state
+ be able to provide raw accelerometer samples at regular intervals.
* able to wake up device from sleep mode to full power mode"need to be able to tune this feature sensitivity"
* Ability to adjust/tune accelerometer sensitivity so that driver behavior parameters can be set to capture “hard breaking, acceleration, hard turns, impacts, etc
* Always store last 30 seconds of accelerometer data so historical data can be retrieved by system

## device installation
+ device have one LED to indicate cell signal and one LED to indicat GPS signal
+ blink while seraching, solid when signal confirmed.
* Ability to toggle LED’s on or off to save power but not affect device state or preperformance
