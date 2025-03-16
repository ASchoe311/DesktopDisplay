
# Desktop display code design conceptualization

### Main concept
The project will be made up of two main "components":

1. Arduino MKR Zero running a program using Zephyr RTOS
2. A python program running on the host desktop

These components will interact with each other to achieve desired functionality of displaying useful information on the LCD attached to the Arduino.

The following information will be available for display:

1. Current date
2. Current time
3. CPU temperature
4. CPU usage %
5. CPU fan speed
6. GPU temperature
7. GPU usage %
8. GPU fan speed
9. System memory usage
10.  Currently playing audio
11. The readings from a temperature probe attached to the Arduino
12. Possibly more in the future

### Connection method:
The Arduino and the host PC will be attached together using two connections:

1. Data will be passed from the host PC to the Arduino with a UART connection over CDC-ACM
2. Data will be passed from the Arduino to the host PC over a USB to TTL UART adapter connected to the TX and RX pins on the Arduino

### Data schema
Communication between the PC and Arduino will have a standard schema for the data.

All communications comply with the following schema:
<CommandByte> <DataLengthByte> <DataBytes> <ChecksumByte>

The only exception being the command 0x00, which will only be followed by a <ChecksumByte>. Thus, the data will always be `00 00`.

Checksums will be handled by a simple XOR operation over all preceding bytes.

Commands will be as follows:

1. 0x00 - This is the "ready" command, and will initialize the program. When the arduino is first turned on, it will send this command at half second intervals until it receives the command back from the host PC. This will always be sent as `00 00`
2. 0x01 - This is the command sent from the Arduino to the PC to tell it that the display page has changed. There are five buttons the LCD keypad that each represent a different display page. The byte sent for each button is the hex representation of the first character of its name in ASCII, so as follows:
   > RIGHT -> 0x52
   > 
   > UP -> 0x55
   > 
   > DOWN -> 0x44
   > 
   > LEFT -> 0x4C
   > 
   > SELECT -> 0x53
   
   So a sample command from the Arduino to the PC to tell it to change the display to the page associated with the DOWN button would be `01 01 44 44`
   This will be the only data sent from the Arduino to the PC
3. 0x02 - This byte represents that the current date is being sent. Date format will be MMDDYYYY. So since today's date is 03/14/2025, the data sent from the PC would be `02 04 03 0E 07 E9 E5`
4. 0x03 - This byte represents that the current time is being sent. Time format will be HHMMA where A represents if it is AM (00) or PM (01). If the time is 8:28pm, the data sent will be `03 03 08 1C 01 15`
5. 0x04 - This byte represents that the current CPU temperature is being sent. Temperatures are whole numbers in Celsius, represented as one byte. If the temperature being sent is 67C, the data will be `04 01 43 46`
6. 0x05 - This byte represents that the current CPU usage is being sent. Usages are whole numbers as a percent, represented as one byte. If the usage is 35%, the data will `05 01 23 27`
7. 0x06 - This byte represents that the current CPU fan speed is being sent. Fan speeds are whole numbers as a percent of maximum speed. If the CPU fan is at 25%, the data sent will be `06 01 19 1E`
8. 0x07 - This byte represents GPU temperature. Same format as 0x04
9. 0x08 - This byte represents GPU usage. Same format as 0x05
10. 0x09 - This byte represents GPU fan speed. Same format as 0x06
11. 0x0A - This byte represents that the current memory usage is being sent. Memory usage is sent as a percentage of available memory used. If memory usage is at 39%, the data sent will be `0A 01 27 2C`
12.  0x0B - This byte represents that the current playing audio title is being sent. If the current song is "Too Sweet", the data will be `0B 09 54 6F 6F 20 53 77 65 65 74 26`

### Display Pages
The display will have 5 different "pages" of data to display, with each page being associated with a specific button.

1. SELECT -> This page will be the default, and will simply contain the date and time in the middle of the display with date on top.
2. LEFT -> This page will be dedicated to CPU and memory usage information. It will consist of a custom icons for each data point, CPU and memory usage on top with temperature and fan speed on bottom
3. UP -> This page will be dedicated to GPU information. GPU usage will be on top, GPU temperature and fan speed will be on bottom
4. DOWN -> This page will be for displaying the currently playing song.
5. RIGHT -> This page will be for displaying readings from the temperature sensor

### Handling Communication Failures
Any received message will be checked against the checksum it is sent with. If the checksum does not match the message, the message will be discarded.
### Custom LCD Characters
1. `byte temperatureChar[] = {
  B01110,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110
};`
2. `byte fanChar1[] = {
  B00000,
  B01110,
  B10011,
  B10101,
  B11001,
  B01110,
  B00000,
  B00000
};`
3. `byte fanChar2[] = {
  B00000,
  B01110,
  B11001,
  B10101,
  B10011,
  B01110,
  B00000,
  B00000
};`
4. `byte cpuChar[] = {
  B11000,
  B10000,
  B11011,
  B00011,
  B00010,
  B00010,
  B10100,
  B11100
};`
5. `byte memoryChar[] = {
  B01110,
  B01011,
  B01110,
  B01111,
  B01010,
  B01111,
  B01010,
  B01111
};`