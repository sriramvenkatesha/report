#include <SoftwareSerial.h> //Used for transmitting to the device

SoftwareSerial softSerial(2, 3); //RX, TX

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

#define EPC_COUNT 100
uint8_t EPC_recv[EPC_COUNT][12]; //the array of EPC data received
#define READTAGSTIMEOUT 10000 //to prevent the reader from overheating
unsigned long lastReadTime;

void init_array() {
  //need to initialise for every new reading
  int i;
  for (int i = 0; i < EPC_COUNT; i++) {
    EPC_recv[i][0] = 0;
  }
}

void update_EPC_array(uint8_t *msg) {
  //checks if the read tag is in the array and adds if it is not

  int i = 0, j = 0;
  bool found = false;

  while (i < EPC_COUNT && EPC_recv[i][0] != 0) {
    found = true;

    for (j = 0; j < 12; j++) {
      if (EPC_recv[i][j] != msg[31 + j]) {
        found = false;
        j = 12;
      }
    }

    if (found == true) {
      return;
    }

    i++;
  }

  if (i == EPC_COUNT) {
    Serial.println(F("Cannot add more to array"));
    return;
  }

  //add to array
  for (j = 0; j < 12; j++) {
    EPC_recv[i][j] = msg[31 + j];
  }
  //  Serial.println(F("Entry added"));
}

int count_entries() {
  //returns size of the EPC data array
  int i = 0;
  while (i < EPC_COUNT) {
    if (EPC_recv[i][0] == 0) {
      break;
    } else {
      i++;
    }
  }
  return i;
}

void refresh_array_count() {
  if (millis() - lastReadTime > READTAGSTIMEOUT) {
    Serial.print(F("tag count: "));
    Serial.println(count_entries());
    lastReadTime = millis();
    init_array();
  }
  update_EPC_array(nano.msg);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial); //Wait for the serial port to come online

  if (setupNano(38400) == false) //Configure nano to run at 9600bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  nano.setReadPower(700); //5.00 dBm. Higher values may caues USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  Serial.println(F("Press a key to begin scanning for tags."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  nano.startReading(); //Begin scanning for tags

  init_array();
}

void loop()
{
  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      //If we have a full record we can pull out the fun bits
      int rssi = nano.getTagRSSI(); //Get the RSSI for this tag read

      long freq = nano.getTagFreq(); //Get the frequency this tag was detected at

      long timeStamp = nano.getTagTimestamp(); //Get the time this was read, (ms) since last keep-alive message

      byte tagEPCBytes = nano.getTagEPCBytes(); //Get the number of bytes of EPC from response

      //      Serial.print(F(" rssi["));
      //      Serial.print(rssi);
      //      Serial.print(F("]"));
      //
      //      Serial.print(F(" freq["));
      //      Serial.print(freq);
      //      Serial.print(F("]"));
      //
      //      Serial.print(F(" time["));
      //      Serial.print(timeStamp);
      //      Serial.print(F("]"));

      //Print EPC bytes, this is a subsection of bytes from the response/msg array

      //      Serial.print(F(" epc["));
      //      for (byte x = 0 ; x < tagEPCBytes ; x++)
      //      {
      //        if (nano.msg[31 + x] < 0x10) Serial.print(F("0")); //Pretty print
      //        Serial.print(nano.msg[31 + x], HEX);
      //        Serial.print(F(" "));
      //      }
      //      Serial.print(F("]"));
      //      Serial.println();

      //      update_EPC_array(nano.msg);
      //
      //      Serial.print(F("tag count: "));
      //      Serial.println(count_entries());

      refresh_array_count();
    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      //      Serial.println("Bad CRC");
    }
    else
    {
      //Unknown response
      Serial.print("Unknown error");
    }
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (softSerial.available()) softSerial.read();

  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}

