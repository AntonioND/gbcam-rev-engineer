#ifndef __SERIAL__
#define __SERIAL__

//Initialize Serial communication with the given COM port
void SerialCreate(char * portName);

//Close the connection
//NOTA: for some reason you can't connect again before exiting
//the program and running it again
void SerialDestroy(void);

int SerialGetInQueue(void); //Returns number of bytes to read
int SerialGetOutQueue(void); //Return number of bytes to output

//Read data in a buffer, if nbChar is greater than the
//maximum number of bytes available, it will return only the
//bytes available. The function return -1 when nothing could
//be read, the number of bytes actually read.
int SerialReadData(char * buffer, unsigned int nbChar);

//Writes data from a buffer through the Serial connection
//return true on success.
int SerialWriteData(char * buffer, unsigned int nbChar);

//Check if we are actually connected
int SerialIsConnected();

#endif // __SERIAL__
