
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "serial.h"
#include "debug.h"

//-------------------------------------------------------------------------

//Serial comm handler
static HANDLE hSerial;

//Connection status
static int connected;

//Get various information about the connection
static COMSTAT status;

//Keep track of last error
static DWORD errors;

//-------------------------------------------------------------------------

void SerialCreate(char * portName)
{
    //We're not yet connected
    connected = 0;

    //Try to connect to the given port through CreateFile
    hSerial = CreateFile(portName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    //Check if the connection was successfull
    if(hSerial==INVALID_HANDLE_VALUE)
    {
        //If not success full display an Error
        if(GetLastError() == ERROR_FILE_NOT_FOUND)
		{
            //Print Error if neccessary
            Debug_Log("ERROR: Handle was not attached. Reason: %s not available.\n", portName);
        }
        else
        {
            Debug_Log("ERROR!!!");
        }
    }
    else
    {
        //If connected we try to set the comm parameters
        DCB dcbSerialParams = {0};

        //Try to get the current
        if(!GetCommState(hSerial, &dcbSerialParams))
        {
            //If impossible, show an error
            Debug_Log("failed to get current serial parameters!");
        }
        else
        {
            //Define serial connection parameters for the Arduino board
            dcbSerialParams.BaudRate=CBR_115200;//CBR_19200;//CBR_9600;
            dcbSerialParams.ByteSize=8;
            dcbSerialParams.StopBits=ONESTOPBIT;
            dcbSerialParams.Parity=NOPARITY;
            //Setting the DTR to Control_Enable ensures that the Arduino is properly
            //reset upon establishing a connection
            dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

			//Set the parameters and check for their proper application
			if(!SetCommState(hSerial, &dcbSerialParams))
			{
				Debug_Log("ALERT: Could not set Serial Port parameters");
			}
			else
			{
				//If everything went fine we're connected
				connected = 1;
				//We wait 2s as the arduino board will be reseting
				Sleep(2000);
			}
        }
    }
}

void SerialDestroy(void)
{
    //Check if we are connected before trying to disconnect
    if(connected)
    {
        //We're no longer connected
        connected = 0;
        //Close the serial handler
        CloseHandle(hSerial);
    }
}

int SerialGetInQueue(void)
{
    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(hSerial, &errors, &status);

    //Check if there is something to read
    return status.cbInQue;
}

int SerialGetOutQueue(void)
{
    FlushFileBuffers(hSerial);

    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(hSerial, &errors, &status);

    //Check if there is something to write
    return status.cbOutQue;
}

int SerialReadData(char * buffer, unsigned int nbChar)
{
    //Number of bytes we'll have read
    DWORD bytesRead;
    //Number of bytes we'll really ask to read
    //unsigned int toRead;

    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(hSerial, &errors, &status);

    //Check if there is something to read
    if(status.cbInQue > 0)
    {
        //If there is we check if there is enough data to read the required number
        //of characters, if not we'll return an error.
        if(status.cbInQue >= nbChar)
        {
            if( ReadFile(hSerial, buffer, nbChar, &bytesRead, NULL) && (bytesRead == nbChar) )
            {
                return bytesRead;
            }
        }
    }
#if 0
    //Check if there is something to read
    if(status.cbInQue > 0)
    {
        //If there is we check if there is enough data to read the required number
        //of characters, if not we'll read only the available characters to prevent
        //locking of the application.
        if(status.cbInQue > nbChar)
        {
            toRead = nbChar;
        }
        else
        {
            //toRead = status.cbInQue;
            return -1; // wait until there are enough bytes
        }

        //Try to read the require number of chars, and return the number of read bytes on success
        if( ReadFile(hSerial, buffer, toRead, &bytesRead, NULL) && (bytesRead != 0) )
        {
            return bytesRead;
        }
    }
#endif // 0

    //If nothing has been read, or that an error was detected return -1
    return -1;
}

int SerialWriteData(char * buffer, unsigned int nbChar)
{
    DWORD bytesSend;

    //Try to write the buffer on the Serial port
    if(!WriteFile(hSerial, (void *)buffer, nbChar, &bytesSend, 0))
    {
        //In case it don't work get comm error and return false
        ClearCommError(hSerial, &errors, &status);

        Debug_Log("SerialWriteData error %d",errors);

        return 0;
    }
    else
    {
        FlushFileBuffers(hSerial);

        return 1;
    }
}

int SerialIsConnected()
{
    //Simply return the connection status
    return connected;
}

//-------------------------------------------------------------------------

