#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include "MachXO2.h"
#pragma hdrstop


byte SPIbuffer[20];
byte SPIbufferByteCount = 0;
 
// Most frequent dummy values for the SPI buffer
const byte SPI_BUFFER_DEFAULT[20] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#define SPI_BUFFER_INIT                  { SPIbufferByteCount = 0; memcpy(SPIbuffer, SPI_BUFFER_DEFAULT, 20); }

#define MachXO2_Command             SPIbuffer[0]
#define pMachXO2_Operand            (&SPIbuffer[1])
#define pMachXO2_Data               (&SPIbuffer[4])

#define SS          D10   // SS
#define DATAOUT     D11   // MOSI
#define DATAIN      D12   // MISO
#define SPICLOCK    D13   // SCLK


bool Fipsy_ReadDeviceID(byte *DeviceID);
bool Fipsy_EraseAll(void);
char JEDEC_SeekNextNonWhitespace(File file);
char JEDEC_SeekNextKeyChar(File file);
byte JEDEC_ReadFuseByte(byte *Fusebyte, File file);

void MachXO2_SPITrans(int count);
void SPI_Transaction(int count);


void handleIndex();
void handleFavicon();

void handleFileList();
void handleUploadFile();
void handleRemoveFile();
void handleFileInfo();

void handleID();
void handleErase();
void handleProgram();

// Soft AP SSID and password
const char* softAPssid = "softAPssid";
const char* softAPpassword = "softAPpassword";

// Station mode SSID and password
const char* ssid = "ssid";
const char* password = "password";

// Listen on the default HTTP port (80)
ESP8266WebServer server;

/* The uploaded file used in handleUploadFile needs to be declared
 * in the global scope, otherwise the resulting uploaded file has 0 bytes.
 * This is because handleUploadFile is called multiple times per file upload,
 * writing 2kB of the uploaded file to the LittleFS per function call.
 */
File uploadedFile;

// Declare FSInfo structure
FSInfo fs_info;




void setup()
{
  pinMode(SS, OUTPUT);
  pinMode(DATAOUT, OUTPUT);
  pinMode(DATAIN, INPUT);
  pinMode(SPICLOCK, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(115200);
  SPI.begin();
  LittleFS.begin();

  if (WiFi.softAP(softAPssid, softAPpassword))
  {
    Serial.println("\n\nSet up soft access point");
    Serial.print("SSID: ");
    Serial.println(softAPssid);
    Serial.print("Password: ");
    Serial.println(softAPpassword);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  Serial.printf("\nConnecting to %s...\n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  

  server.on("/", handleIndex);
  server.on("/favicon.ico", handleFavicon);
  
  server.on("/list", handleFileList);
  server.on("/upload", HTTP_POST, [](){
    server.send(200, "text/plain", "{\"success\":1}");
  }, handleUploadFile);
  server.on("/remove", handleRemoveFile);
  server.on("/info", handleFileInfo);
  
  server.on("/id", handleID);
  server.on("/erase", handleErase);
  server.on("/program", handleProgram);

  server.begin();
}




void loop()
{
  server.handleClient();
}




void handleIndex()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  File file = LittleFS.open("/index.html", "r");
  server.streamFile(file, "text/html");
  file.close();
  
  digitalWrite(LED_BUILTIN, HIGH);
}

void handleFavicon()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  File file = LittleFS.open("/favicon.ico", "r");
  server.streamFile(file, "image/x-icon");
  file.close();
  
  digitalWrite(LED_BUILTIN, HIGH);
}




/* handleFileList sends a list of JEDEC files in the file system as JSON
 * if show=all is passed as a query string, all files are shown,
 * including index.html and favicon.ico
 */
void handleFileList()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  String show = server.arg("show"); // parse "show" parameter
  String list = "[";
  
  Dir dir = LittleFS.openDir("/");
  while (dir.next())
  {
    File file = dir.openFile("r");
    if ((String(file.name()).endsWith(".jed")) || (show == "all"))
    {
      if (list != "[")
      {
        list += ",";
      }
      list += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
    }
    file.close();
  }
  list += "]";
  server.send(200, "application/json", list);
  digitalWrite(LED_BUILTIN, HIGH);
}


void handleUploadFile()
{
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START)
  {
    digitalWrite(LED_BUILTIN, LOW);
    
    uploadedFile = LittleFS.open(upload.filename, "w");
  }
  
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadedFile)
    {
      uploadedFile.write(upload.buf, upload.currentSize);
    }
  }
  
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadedFile)
    {
      uploadedFile.close();
      server.send(201, "text/plain", "Uploaded " + upload.filename + " (size: " + upload.totalSize + " B)");
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}


void handleRemoveFile()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  String filename = server.arg("filename"); // parse "filename" parameter
  
  if (!LittleFS.exists(filename)){
    server.send(404, "text/plain", "File does not exist");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  if (!filename.endsWith(".jed"))
  {
    server.send(403, "text/plain", "Not allowed to remove " + filename);
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  if (LittleFS.remove(filename))
  {
    server.send(200, "text/plain", "Removed " + filename);
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  server.send(500, "text/plain", "Could not remove " + filename);
  digitalWrite(LED_BUILTIN, HIGH);
}


// handleFileInfo sends a string containing information about the file system
void handleFileInfo()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  String fileInfo = "";
  
  if (LittleFS.info(fs_info))
  {
    fileInfo += "Total bytes: " + String(fs_info.totalBytes);
    fileInfo += "\nUsed bytes: " + String(fs_info.usedBytes);
    fileInfo += "\nBlock size: " + String(fs_info.blockSize);
    fileInfo += "\nPage size: " + String(fs_info.pageSize);
    fileInfo += "\nMax open files: " + String(fs_info.maxOpenFiles);
    fileInfo += "\nMax path length: " + String(fs_info.maxPathLength);
  }
  
  else
  {
    fileInfo = "FSInfo failed";
  }

  server.send(200, "text/plain", fileInfo);
  digitalWrite(LED_BUILTIN, HIGH);
}




void handleID()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  byte id[4];
  
  if (!Fipsy_ReadDeviceID(id))
  {
    server.send(500, "text/plain", "Data pointer provided is NULL");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  char ID[12];
  sprintf(ID, "%02X %02X %02X %02X", id[0], id[1], id[2], id[3]);
  server.send(200, "text/plain", ID);
  
  digitalWrite(LED_BUILTIN, HIGH);
}


void handleErase()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  if (!Fipsy_EraseAll())
  {
    server.send(500, "text/plain", "Unable to erase device");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  server.send(200, "text/plain", "Erase successful");
  digitalWrite(LED_BUILTIN, HIGH);
}


void handleProgram()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  String filename = server.arg("filename"); // parse "filename" parameter

  File file;
  char key;
  int addr_digits = 0;
  byte byteStatus;
  byte featurerow[10];
  byte feabits[4];
  
  if (!LittleFS.exists(filename))
  {
    server.send(404, "text/plain", "File does not exist");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  if (!filename.endsWith(".jed"))
  {
    server.send(403, "text/plain", "File format must be JEDEC (.jed)");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  if (!Fipsy_EraseAll())
  {
    server.send(500, "text/plain", "Unable to erase device and proceed with programming");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  file = LittleFS.open(filename, "r");
  if (!file)
  {
    server.send(500, "text/plain", "File open failed");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  do
  {
    key = file.read();
  }
  while ((key != 0x02) && (key != 0x03));
  
  if (key == 0x03)
  {
    server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
    file.close();
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  // Look for fuse table
  while (key != 'L')
  {
    if ((key = JEDEC_SeekNextKeyChar(file)) == 0)
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  
  // Count the digits in the address
  do
  {
    key = file.read();
    if (key == '0')
    {
      addr_digits += 1;
    }
  }
  while (key == '0');
  
  // Clear the address in the device
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_INIT_ADDRESS;
  MachXO2_SPITrans(4);

  do
  {
    // Setup the write and increment command
    SPI_BUFFER_INIT;
    MachXO2_Command = MACHXO2_CMD_PROG_INCR_NV;
    pMachXO2_Operand[2] = 0x01;
    
    // Get the first byte and check that we are not at the delimiter
    byteStatus = JEDEC_ReadFuseByte(pMachXO2_Data, file);
    
    if (byteStatus == 0)
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
    
    if (byteStatus != '*')
    {
      for (int i = 1; i < 16; i++)
      {
        byteStatus = JEDEC_ReadFuseByte(&pMachXO2_Data[i], file);
        if (byteStatus != 1)
        {
          server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
          file.close();
          digitalWrite(LED_BUILTIN, HIGH);
          return;
        }
      }

      MachXO2_SPITrans(20);
      delay(1);
    }
  }
  while (byteStatus != '*');
  
  if ((key = JEDEC_SeekNextNonWhitespace(file)) == 0)
  {
    server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
    file.close();
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  // Look for feature row
  while (key != 'E')
  {
    if ((key = JEDEC_SeekNextKeyChar(file)) == 0)
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  
  for (int i = 0; i < 8; i++)
  {
    if (JEDEC_ReadFuseByte(&featurerow[i], file) != 1)
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }

  for (int i = 0; i < 2; i++)
  {
    if (JEDEC_ReadFuseByte(&feabits[i], file) != 1)
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }

  // Program feature row
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_PROG_FEATURE;
  memcpy(pMachXO2_Data, featurerow, 8);
  MachXO2_SPITrans(12);
  delay(1);

  // Program feabits
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_PROG_FEABITS;
  memcpy(pMachXO2_Data, feabits, 2);
  pMachXO2_Data[1] &= 0xBF; // prevent SPI from being disabled
  MachXO2_SPITrans(6);
  delay(1);
  
  // Program the DONE bit (internal)
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_PROGRAM_DONE;
  MachXO2_SPITrans(4);
  delay(1);
  
  // Reload the configuration from flash
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_REFRESH;
  MachXO2_SPITrans(3);
  delay(1);

  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_PROGRAM_DONE;
  MachXO2_SPITrans(4);
  
  server.send(200, "text/plain", "Programming successful");
  file.close();

  digitalWrite(LED_BUILTIN, HIGH);
}




void MachXO2_SPITrans(int count)
{
  SPIbufferByteCount = count;
  SPI_Transaction(SPIbufferByteCount);
}


/* SPI_Transaction completes the data transfer to and/or from the device
   per the methods required by this system. This uses the global defined
   SPI port handle, which is assumed to be open if this call is reached
   from a routine in this code. It is also assumed that the arguments are
   valid based on the controlled nature of calls to this routine.
*/
void SPI_Transaction(int count)
{
  byte sentData[20];
  byte incomingByte;
  byte receivedData[count];
  
  memcpy(sentData, SPIbuffer, count); // SPIbuffer gets altered (copy needed)
  
  // Initialize transaction with a maximum SCLK speed of 45 MHz
  digitalWrite(SS, LOW); // SS enabled
  SPI.beginTransaction(SPISettings(45000000, MSBFIRST, SPI_MODE0));
  
  for (int i = 0; i < count; i++)
  {
    receivedData[i] = SPI.transfer(sentData[i]); // send byte and capture returned byte
  }
  
  digitalWrite(SS, HIGH); // SS disabled
  SPI.endTransaction();
  
  for (int i = 0; i < count; i++)
  {
    SPIbuffer[i] = receivedData[i];
  }
}


/* Fipsy_ReadDeviceID retrieves the device identification number from the FPGA
 * connected to the SPI port and returns true if it succeeded, false otherwise
 */
bool Fipsy_ReadDeviceID(byte *DeviceID)
{
  if (DeviceID == NULL)
  {
    return false;
  }
  
  // Read device ID
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_READ_DEVICEID;
  MachXO2_SPITrans(8);
  
  memcpy(DeviceID, pMachXO2_Data, 4);
  
  return true;
}


/* Fipsy_EraseAll clears the configuration from all portions of the FPGA
   and returns true if it was successful, false otherwise.
*/
bool Fipsy_EraseAll(void)
{
  byte busy = 0x80;
  uint32_t timeout = 0;
  
  // Enter programming mode
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_ENABLE_OFFLINE;
  pMachXO2_Operand[0] = 0x08;
  MachXO2_SPITrans(4);
  delay(1);
  
  // Erase everything
  SPI_BUFFER_INIT;
  MachXO2_Command = MACHXO2_CMD_ERASE;
  pMachXO2_Operand[0] = 0x0F;
  MachXO2_SPITrans(4);
  
  // Look at busy status every 100 ms
  do
  {
    delay(100);
    timeout += 100;
    if (timeout > 1000)
    {
      return false;
    }
    
    // Read the busy bit
    SPI_BUFFER_INIT;
    MachXO2_Command = MACHXO2_CMD_CHECK_BUSY;
    MachXO2_SPITrans(5);
    busy = pMachXO2_Data[0];
  }
  while (busy);
  
  return true;
}


/* JEDEC_SeekNextNonWhitespace parses the file until it finds a character
   that is not white space as defined for a JEDEC file. That character is
   returned if found. If there is an error, a 0 is returned.
   
   Note that the delimiter ('*') is also white space in this context. This is
   like a line terminator in a sense, so if we have not already read it, we
   don't want to now read it and return it. This could happen if it is
   the first character in a file, or if a field ended with "**" or more.
   These technically null fields should be ignored in this search.
*/
char JEDEC_SeekNextNonWhitespace(File file)
{
  char c;
  
  do
  {
    c = file.read();
    if (c == 0x03) // if at end of file
    {
      return 0;
    }
  }
  while ((c <= ' ') || (c == '*'));
  
  return c;
}


/* JEDEC_SeekNextKeyChar reads the specified file stream until the next key
   character has been read. The key character (ie key word) is the first
   character of a field (ie after the previous field's delimiter) after any
   white space. Thus, in order to do this search, this function will also
   search for the start of the next field. If we know we are at the start
   of a field already, then we should not use this routine but instead just
   look for the character. This routine will find the next key character.
   
   The key character found is returned by value. If the end of the file
   is reached, or some other error occurs, a 0 is returned.
*/
char JEDEC_SeekNextKeyChar(File file)
{
  char c;
  char key;
  
  // Look for end of line
  do
  {
    c = file.read();
    if (c == 0x03) // if at end of file
    {
      return 0;
    }
  }
  while (c != '*');

  // Return next non-whitespace character
  key = JEDEC_SeekNextNonWhitespace(file);
  return key;
}


/* JEDEC_ReadFuseByte reads from the specified file stream until it has
   collected eight binary characters and converts those characters to a byte
   to return by reference. The value returned is 1 if this happened correctly,
   0 if an error was encountered, and '*' if the field has ended ('*' found).
*/
byte JEDEC_ReadFuseByte(byte *FuseByte, File file)
{
  char bits[10];
  char c;
  byte cnt = 0;

  *FuseByte = 0; // Default byte value
  
  do
  {
    c = file.read();
    if (c == 0x03) // if at end of file
    {
      return 0;
    }
    
    if (c == '0')
    {
      bits[cnt++] = '0';
    }
    
    if (c == '1')
    {
      bits[cnt++] = '1';
    }
    
    if (c == '*')
    {
      return c;
    }
  }
  while (cnt < 8);
  
  if (bits[0] == '1') *FuseByte += 128;
  if (bits[1] == '1') *FuseByte += 64;
  if (bits[2] == '1') *FuseByte += 32;
  if (bits[3] == '1') *FuseByte += 16;
  if (bits[4] == '1') *FuseByte += 8;
  if (bits[5] == '1') *FuseByte += 4;
  if (bits[6] == '1') *FuseByte += 2;
  if (bits[7] == '1') *FuseByte += 1;
  
  return 1;
}