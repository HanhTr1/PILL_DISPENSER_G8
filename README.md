# PILL_DISPENSER_G8_PROJECT
<img width="1195" height="667" alt="image" src="https://github.com/user-attachments/assets/4c5fbfb5-53c5-4fc3-a40e-4bb241cd948f" />

## PROJECT GOAL
The goal of the project is to implement an automated pill dispenser that dispenses daily medicine to a patient. The dispenser has eight compartments. One of them is used for calibrating the dispenser wheel position leaving the other seven for pills. Dispenser wheel will turn daily at a fixed time to dispense the daily medicine and the piezoelectric sensor will be used to confirm that pill(s) were dispensed. The dispenser state is stored in non-volatile memory so that the state of the device (number of pills left, daily dispensing log, etc) will persist over restarts. The state of the device is communicated to a server using LoRaWAN network. For testing purposes, the time between dispensing pills is reduced to 30 seconds. 
## MININUM REQUIREMENTS
#### BUTTON & LED HANDLING
At ST_WAIT_CALIBRATION, SW0 is to be pressed and blinks an LED while waiting until pressing next button.
#### STEPPER MOTOR & OPTO FORK (CALIBRATION)
These components GP2, GP3, GP6, GP13 & GP28 are used to find falling edge and settle an accurate zero position before dispensing pills. The stepper motor knows how many steps it has moved when the device is powered on/off, and it will run to aligned over the dispensing hole.
#### PIEZO SENSOR (PILL DETECTION)
The piezo sensor GPIO27 is used to detect pill dropped every 30 seconds after button SW2 pressed. When a pill falls, it hits a small metal plate attached to a piezo element and the changes of pill will be recorded.
## ADVANCED REQUIREMENTS
#### EEPROM
EEPROM is used to store the dispenser’s state so that all important information is recovered when power loss, reboot, or reset. It stores total pill, pills_left, slot_done,logs, CRC check, FSM state
#### LORAWAN COMMUNICATION
LoRaWAN is used to send notifications and logs remotely about the device’s operation. Messages are like Booted, LoRaWAN Connected, Calibration Done, Dispense OK, Dispense Fail, Power Loss Recovery, Cycle Finished Reset...
#### RECALIBRATE
It means realigning the dispenser’s rotating wheel when power loss, reboot, or reset so that every pill compartment lines up exactly with the dispense hole and continue the progress.
## STATE MACHINE
  - ST_BOOT,
    Stabilize the device when it is just powered up
  - ST_LORA_CONNECT,
    5 attempts for LoRaWAN join to send logs/status. Calls lorawan_init(). Calls handle_lorawan() to join. Sends messages.
  - ST_CHECK_EEPROM,
    Detect if previous session was interrupted by power loss. Restore state, motor position, slot_done, pills_left.
  - ST_RECOVERY,
    Recover from power loss,  reboot, or reset. If motor uncalibrated ->cannot recover -> go to calibration. Calls stepper_recovery() to rewinds the nearest valid slot boundary. Logs RECOVERY DONE. If pills    remain, resume dispensing. Otherwise, it move to FINISHED.
  - ST_WAIT_CALIBRATION,
    User must press the calibration button (SW0). LED blinks.  Calls wait_calib_button_handler()
  - ST_CALIBRATION,
    Stepper run until optical sensor detects index marker. Defines DAY 1 position. Calls stepper_calibrate(). Sets calibrated flag. Applies slot offset. Saves state to EEPROM.        
  - ST_WAIT_DISPENSING,
    User must press DISPENSE START button (SW2) & LED on. Calls wait_dispensing_button_handler()    
  - ST_DISPENSING,
    Dispense 1 pill every interval 30s. Saves current slot number -> slot_done. Moves one slot (512 steps). Clears pill sensor. Waits for piezo pulse. If pulse -> pill detected. Decrease pills_left. If no pulse ->pill missing ->log fail. Save everything to EEPROM.        
  - ST_FINISHED
    When all 7 pills dispensed, reset: slot_done = 0, pills_left = 7, calibrated = false, Saves new state to EEPROM. Waiting for starting next cycle.


