#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include "MachXO2.h"
#include <LittleFS.h>
#pragma hdrstop

// Function declarations
bool Fipsy_ReadDeviceID(byte *DeviceID);
bool Fipsy_EraseAll(void);
char JEDEC_SeekNextNonWhitespace(File file);
char JEDEC_SeekNextKeyChar(File file);
byte JEDEC_ReadFuseByte(byte *Fusebyte, File file);

// General purpose subroutine declarations
void MachXO2_SPITrans(int count);
void SPI_Transaction(int count);

/* General purpose buffer used to transfer data on SPI
 * This is bigger than most routines need, but reduces repeated declarations
 * and is bigger than the actual SPI transaction can be, meaning there is
 * always enough room
 */
byte SPIBuf[100];

/* Count of bytes in the SPI buffer 
 * This is filled based on the count to send in a transaction
 * On return from a transaction, this will specify the number of bytes returned
 * bytes returned is the entire SPI transaction, not just the data, so the value
 * should not change unless something went wrong.
 */
byte MachXO2_Count = -1;
 
// Macro to set the most frequently used dummy values for the SPI buffer
byte SPIBUF_DEFAULT[20] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#define SPIBUFINIT                  { MachXO2_Count = 0; memcpy(SPIBuf, SPIBUF_DEFAULT, 20); }

// Define key elements of SPI transactions
#define MachXO2_Command             SPIBuf[0]
#define pMachXO2_Operand            (&SPIBuf[1])
#define pMachXO2_Data               (&SPIBuf[4])

// Define SPI pins
#define SS          D10   // SS
#define DATAOUT     D11   // MOSI
#define DATAIN      D12   // MISO
#define SPICLOCK    D13   // SCLK


// Declare request handlers
void handleIndex();
void handleFaviconICO();

void handleFileList();
void handleUploadFile();
void handleRemoveFile();
void handleFileInfo();

void handleID();
void handleErase();
void handleProgram();

// Define soft AP SSID and password
const char* softAPssid = "softAPssid";
const char* softAPpassword = "softAPpassword";

// Define SSID and password to connect to using station mode
const char* ssid = "ssid";
const char* password = "password";

// ESP8266WebServer instance to listen on the default HTTP port (80)
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
  // Set pin modes
  pinMode(SS, OUTPUT);
  pinMode(DATAOUT, OUTPUT);
  pinMode(DATAIN, INPUT);
  pinMode(SPICLOCK, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // used to indicate code execution
  
  Serial.begin(115200);
  SPI.begin();
  LittleFS.begin();

  // Set up soft access point
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

  // Connect to wireless access point
  Serial.printf("\nConnecting to %s...\n", ssid);
  WiFi.begin(ssid, password);

  // Wait for connection to establish
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
  
  // Define request handlers
  server.on("/", handleIndex);
  server.on("/favicon.ico", handleFaviconICO);
  
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
  
  // Serve index.html file
  File file = LittleFS.open("/index.html", "r");
  server.streamFile(file, "text/html");
  file.close();
  
  digitalWrite(LED_BUILTIN, HIGH);
}

void handleFaviconICO()
{
  digitalWrite(LED_BUILTIN, LOW);
  
  // Serve favicon.ico file (icon)
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
  
  // Iterate over files inside the root directory
  Dir dir = LittleFS.openDir("/");
  while (dir.next())
  {
    File file = dir.openFile("r");
    if ((String(file.name()).endsWith(".jed")) || (show == "all"))
    {
      // Separate by comma if there are multiple files
      if (list != "[")
      {
        list += ",";
      }
      // Append file to list
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
  // Define HTTPUpload structure
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START)
  {
    digitalWrite(LED_BUILTIN, LOW);
    
    // Create a file with name upload.filename
    uploadedFile = LittleFS.open(upload.filename, "w");
  }
  
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadedFile)
    {
      // Write 2 kB of data to the file system
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

  // Check if file is JEDEC
  if (!filename.endsWith(".jed"))
  {
    server.send(403, "text/plain", "Not allowed to remove " + filename);
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  if (LittleFS.remove(filename)) // if file is removed successfully
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
  
  // Read and print the IDs
  if (!Fipsy_ReadDeviceID(id))
  {
    server.send(500, "text/plain", "Data pointer provided is NULL");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  // Send ID to client at /id
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

  // Declare necessary variables
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

  // Check if file is JEDEC
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
  
  // Read the file characters until the starting STX (0x02) is found
  do
  {
    key = file.read();
  }
  while ((key != 0x02) && (key != 0x03));
  
  if (key == 0x03) // if there is no STX (checked files will not evaluate to true)
  {
    server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
    file.close();
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  // Next look for the fuse table specifically
  while (key != 'L')
  {
    if ((key = JEDEC_SeekNextKeyChar(file)) == 0) // checked files will not evaluate to true
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  
  /* We are now at the fuse table and pointing to the starting address.
   * The documentation says it is followed by white space, but does not really guarantee what kind
   * of white space, so we must read off the address characters one at a time.
   * We do assume from the documentation that the first L key character found will contain the address zero,
   * and fuse data will start from the beginning of flash.
   */
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
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_INIT_ADDRESS;
  MachXO2_SPITrans(4);
  
  /* We are now at the fuse data
   * Proceed to write flash locations per page until the delimiter is reached.
   */
  
  do
  {
    // Setup the write and increment command
    SPIBUFINIT;
    MachXO2_Command = MACHXO2_CMD_PROG_INCR_NV;
    pMachXO2_Operand[2] = 0x01;
    
    // Get the first byte and check that we are not at the delimiter
    byteStatus = JEDEC_ReadFuseByte(pMachXO2_Data, file);
    
    if (byteStatus == 0) // checked files will not evaluate to true
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
    
    // If we did not get the delimiter, this should be a valid row
    if (byteStatus != '*')
    {
      // Attempt to collect the rest of the page
      for (int i = 1; i < 16; i++)
      {
        byteStatus = JEDEC_ReadFuseByte(&pMachXO2_Data[i], file);
        if (byteStatus != 1) // checked files will not evaluate to true
        {
          server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
          file.close();
          digitalWrite(LED_BUILTIN, HIGH);
          return;
        }
      }
      
      // We can now send the command
      MachXO2_SPITrans(20);
      delay(1);
    }
  }
  while (byteStatus != '*'); // Repeat for every page until the delimiter is reached
  
  /* Note that for our chip the JEDEC file contains two blocks of data.
   * The first is the configuration for the present design, and the second is the remainder of the
   * configuration memory, as the address for the second block changes for each design.
   * This second block of data is ignored.
   */
  
  // Go find the key for our next thing of interest, the feature row fuses
  // We just read a delimiter, so we are at the start of the next field
  if ((key = JEDEC_SeekNextNonWhitespace(file)) == 0) // checked files will not evaluate to true
  {
    server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
    file.close();
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  // Look for the key character
  while (key != 'E')
  {
    if ((key = JEDEC_SeekNextKeyChar(file)) == 0) // checked files will not evaluate to true
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  
  // We are now at the feature row bits, pointed at the fuse data
  // Read the data into the local arrays
  for (int i = 0; i < 8; i++)
  {
    if (JEDEC_ReadFuseByte(&featurerow[i], file) != 1) // checked files will not evaluate to true
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  for (int i = 0; i < 2; i++)
  {
    if (JEDEC_ReadFuseByte(&feabits[i], file) != 1) // checked files will not evaluate to true
    {
      server.send(500, "text/plain", "File check failed: file format is not valid for JEDEC");
      file.close();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    }
  }
  
  // Call our routine to program these values
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_PROG_FEATURE;
  memcpy(pMachXO2_Data, featurerow, 8);
  MachXO2_SPITrans(12);
  delay(1);

  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_PROG_FEABITS;
  memcpy(pMachXO2_Data, feabits, 2);
  
  // Prevent the SPI from being disabled
  pMachXO2_Data[1] &= 0xBF;
  
  // Send the command
  MachXO2_SPITrans(6);
  delay(1);
  
  /* Program the DONE bit (internal)
   * This effectively tells the SDM (self download mode) that it is allowed to run
   * and allows the device to enter user mode when loading is complete (ie done)
   */
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_PROGRAM_DONE;
  MachXO2_SPITrans(4);
  delay(1);
  
  // Now that everything is programmed, reload the configuration from flash
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_REFRESH;
  MachXO2_SPITrans(3);
  delay(1);

  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_PROGRAM_DONE;
  MachXO2_SPITrans(4);
  
  server.send(200, "text/plain", "Programming successful");
  file.close();
  digitalWrite(LED_BUILTIN, HIGH);
}



void MachXO2_SPITrans(int count)
{
  MachXO2_Count = count;
  SPI_Transaction(MachXO2_Count);
}

/* SPI_Transaction completes the data transfer to and/or from the device
   per the methods required by this system. This uses the global defined
   SPI port handle, which is assumed to be open if this call is reached
   from a routine in this code. It is also assumed that the arguments are
   valid based on the controlled nature of calls to this routine.
*/

void SPI_Transaction(int count)
{
  byte consumedData[100];
  byte incomingByte;
  byte outputValues[count];
  
  memcpy(consumedData, SPIBuf, count);  // Make a copy of the variable, otherwise SPIBuf gets altered
  
  // Initialize transaction with a maximum SCLK speed of 45 MHz
  digitalWrite(SS, LOW); // SS enabled
  SPI.beginTransaction(SPISettings(45000000, MSBFIRST, SPI_MODE0));
  
  for (int i = 0; i < count; i++)
  {
    byte sentByte = consumedData[i];
    incomingByte = SPI.transfer(sentByte); // Send byte and capture returned byte
    outputValues[i] = incomingByte;
  }
  
  digitalWrite(SS, HIGH); // SS disabled
  SPI.endTransaction();
  
  for (int i = 0; i < count; i++)
  {
    SPIBuf[i] = outputValues[i];
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
  
  // Construct the command
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_READ_DEVICEID;
  MachXO2_SPITrans(8);
  
  // Copy the data
  memcpy(DeviceID, pMachXO2_Data, 4);
  
  // Return success
  return true;
}

/* Fipsy_EraseAll clears the configuration from all portions of the FPGA
   and returns true if it was successful, false otherwise.
   
   For this library, this is the first step to programming. This is the
   function that enters the programming mode, so it must be completed
   before the programming operation.
*/

bool Fipsy_EraseAll(void)
{
  byte busy = 0x80;
  uint32_t timeout = 0;
  
  // Send command to enter programming mode
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_ENABLE_OFFLINE;
  pMachXO2_Operand[0] = 0x08;
  MachXO2_SPITrans(4);
  delay(1);
  
  // Send command to erase everything
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_ERASE;
  pMachXO2_Operand[0] = 0x0F;
  MachXO2_SPITrans(4);
  
  // Look at busy status every so often until it is clear or until timeout
  // The busy bit is in the MSB, but still means we can test nonzero
  do
  {
    // Do a wait between polls of the busy bit
    delay(100);
    timeout += 100;
    if (timeout > 1000)
    {
      return false;
    }
    
    // Go read the busy bit
    SPIBUFINIT;
    MachXO2_Command = MACHXO2_CMD_CHECK_BUSY;
    MachXO2_SPITrans(5);
    busy = pMachXO2_Data[0];
  }
  while (busy);
  
  // Return success
  return true;
}

/* JEDEC File Parsing Support Subroutines */

/* The following private functions all operate on a JEDEC file using a
   file stream pointer for an open text file established globally by
   the caller. Coding this way removes the need to pass the file pointer
   to each function and among them. Because these are private functions
   and used in a manner controlled in this module, we do not do additional
   checking of the pointers and other variables in use.
 */

/* JEDEC_SeekNextNonWhitespace parses the file until it finds a character
   that is not white space as defined for a JEDEC file. That character is
   returned if found. If there is an error, a 0 is returned.
   
   Note that the delimiter ('*') is also white space in this context. This is
   like a line terminator in a sense, so if we have not already read it, we
   don't want to now read it and return it. This could happen if it is
   the first character in a file, or if a field ended with "**" or more.
   These technically null fields should be ignored in this search.
   
   So define white space as ' ', CR, LF, NULL, and '*' at least. But in
   reality we equally ignore any character less than space, which includes
   most control characters, including the JEDEC file start and end STX/EOT.
*/

char JEDEC_SeekNextNonWhitespace(File file)
{
  char c;
  
  // Read until we find something other than whitespace or '*'
  do
  {
    c = file.read();
    if (c == 0x03) // if at end of file
    {
      return 0;
    }
  }
  while ((c <= ' ') || (c == '*'));
  
  // Return what was found
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
  
  // Look for end of line, point to start of next field
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
   Generally, unless something is wrong with the file, the full byte will be
   collected or '*' will be returned. Other white space is automatically removed.
   A return value of 0 can be interpreted as a format error. A return value
   of '*' means the caller should assume the file pointer now points to the
   start of the next field.
   
   Note that anything other than 1,0,* characters can be considered white space.
   If a character is out of place or replaced, and it disrupts the count of 1s
   and 0s, then an error will eventually be found. If the bad character is there
   without affecting the result, then it has no impact this way. In a perfect
   file, the characters so removed are truly white space.
*/

byte JEDEC_ReadFuseByte(byte *FuseByte, File file)
{
  char bits[10];
  char c;
  byte cnt = 0;
  
  // Default byte value
  *FuseByte = 0;
  
  do
  {
    c = file.read();
    if (c == 0x03) // if at end of file
    {
      return 0;
    }
    
    // Record valid characters
    if (c == '0')
    {
      bits[cnt++] = '0';
    }
    
    if (c == '1')
    {
      bits[cnt++] = '1';
    }
    
    // If delimiter found, return it
    if (c == '*')
    {
      return c;
    }
  }
  while (cnt < 8);
  
  // Convert the characters to binary
  if (bits[0] == '1') *FuseByte += 128;
  if (bits[1] == '1') *FuseByte += 64;
  if (bits[2] == '1') *FuseByte += 32;
  if (bits[3] == '1') *FuseByte += 16;
  if (bits[4] == '1') *FuseByte += 8;
  if (bits[5] == '1') *FuseByte += 4;
  if (bits[6] == '1') *FuseByte += 2;
  if (bits[7] == '1') *FuseByte += 1;
  
  // Return normal success
  return 1;
}
