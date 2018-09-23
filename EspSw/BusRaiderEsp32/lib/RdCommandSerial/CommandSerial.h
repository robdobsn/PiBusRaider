// CommandSerial 
// Rob Dobson 2018

#pragma once
#include "Arduino.h"
#include "HardwareSerial.h"
#include "Utils.h"
#include "MiniHDLC.h"
#include "ConfigBase.h"
#include "ArduinoLog.h"

typedef void CommandSerialFrameRxFnType(const uint8_t *framebuffer, int framelength);

class CommandSerial
{
  private:
    // Serial
    static HardwareSerial *_pSerial;

    // Serial port details
    int _serialPortNum;
    int _baudRate;

    // HDLC processor
    MiniHDLC _miniHDLC;

    // Upload of files
    bool _uploadInProgress;
    int _blockCount;
    unsigned long _uploadStartMs;
    unsigned long _uploadLastBlockMs;
    static const int MAX_UPLOAD_MS = 600000;
    static const int MAX_BETWEEN_BLOCKS_MS = 20000;

    // Frame handling callback
    static CommandSerialFrameRxFnType* _pFrameRxCallback;

  public:
    CommandSerial() : _miniHDLC(sendChar, frameHandler, true)
    {
        _pSerial = NULL;
        _serialPortNum = -1;
        _uploadInProgress = false;
        _blockCount = 0;
        _baudRate = 115200;
        _pFrameRxCallback = NULL;

    }

    static void sendChar(uint8_t ch)
    {
        if (_pSerial)
            _pSerial->write(ch);
    }

    static void frameHandler(const uint8_t *framebuffer, int framelength)
    {
        // Handle received frames
        if (_pFrameRxCallback)
            _pFrameRxCallback(framebuffer, framelength);
        Log.notice("HDLC frame received, len %d\n", framelength);
    }

    void setup(ConfigBase& config, CommandSerialFrameRxFnType* pFrameRxCallback)
    {
        // Callback
        _pFrameRxCallback = pFrameRxCallback;

        // Get config
        ConfigBase csConfig(config.getString("commandSerial", "").c_str());
        Log.trace("CommandSerial: config %s\n", csConfig.getConfigData());

        // Get serial port
        _serialPortNum = csConfig.getLong("portNum", -1);
        _baudRate = csConfig.getLong("baudRate", 115200);

        // Debug
        Log.trace("CommandSerial: portNum %d, baudRate %d\n", _serialPortNum, _baudRate);

        // Setup port
        if (_serialPortNum == -1)
            return;

        // Setup serial port
        _pSerial = new HardwareSerial(_serialPortNum);
        if (_pSerial)
        {
            if (_serialPortNum == 1)
            {
                _pSerial->begin(_baudRate, SERIAL_8N1, 16, 17, false);
                Log.trace("CommandSerial: portNum %d, baudRate %d, rxPin, txPin\n",
                             _serialPortNum, _baudRate, 16, 17);
            }
            else if (_serialPortNum == 2)
            {
                _pSerial->begin(_baudRate, SERIAL_8N1, 26, 25, false);
                Log.trace("CommandSerial: portNum %d, baudRate %d, rxPin, txPin\n",
                             _serialPortNum, _baudRate, 26, 25);
            }
            else
            {
                _pSerial->begin(_baudRate);
                Log.trace("CommandSerial: portNum %d, baudRate %d, rxPin, txPin\n",
                             _serialPortNum, _baudRate, 3, 1);
            }
        }
    }
    
    // Log message
    void logMessage(String& msg)
    {
        String frame = "{\"cmdName\":\"logMessage\",\"msg\":\"" + msg + "}\0";
        _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
    }

    // Service 
    void service()
    {
        // Check if port enabled
        if (!_pSerial)
            return;

        // See if characters to be processed
        for (int rxCtr = 0; rxCtr < 1000; rxCtr++)
        {
            int rxCh = _pSerial->read();
            if (rxCh < 0)
                break;

            // Handle char
            _miniHDLC.handleChar(rxCh);
        }

        if (_uploadInProgress)
        {
            // Check for timeouts
            if (Utils::isTimeout(millis(), _uploadLastBlockMs, MAX_BETWEEN_BLOCKS_MS))
            {
                _uploadInProgress = false;
                Log.notice("CommandSerial: Upload timed out\n");
            }
            if (Utils::isTimeout(millis(), _uploadStartMs, MAX_UPLOAD_MS))
            {
                _uploadInProgress = false;
                Log.notice("CommandSerial: Upload timed out\n");
            }
        }
    }

    void sendFileStartRecord(String& filename, int fileLength)
    {
        String frame = "{\"cmdName\":\"ufStart\",\"fileName\":\"" + filename + "\",\"fileLen\":" + String(fileLength) + "}";
        _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
    }

    void sendFileBlock(size_t index, uint8_t *data, size_t len)
    {
        String header = "{\"cmdName\":\"ufBlock\",\"index\":" + String(index) + ",\"len\":" + String(len) + "}";
        int headerLen = header.length();
        uint8_t* pFrameBuf = new uint8_t[headerLen + len + 1];
        memcpy(pFrameBuf, header.c_str(), headerLen);
        pFrameBuf[headerLen] = 0;
        memcpy(pFrameBuf + headerLen + 1, data, len);
        _miniHDLC.sendFrame(pFrameBuf, headerLen + len + 1);
        delete [] pFrameBuf;
    }

    void sendFileEndRecord(int blockCount)
    {
        String frame = "{\"cmdName\":\"ufEnd\",\"blockCount\":\"" + String(blockCount) + "\"}";
        _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
    }

    void sendTargetCommand(String& targetCmd)
    {
        String frame = "{\"cmdName\":\"" + targetCmd + "\"}\0";
        _miniHDLC.sendFrame((const uint8_t*)frame.c_str(), frame.length());
    }

    void fileUploadPart(String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool final)
    {
        Log.notice("CommandSerial: %s, total %d, idx %d, len %d, final %d\n", filename.c_str(), fileLength, index, len, final);

        // Check if first block in an upload
        if (!_uploadInProgress)
        {
            _uploadInProgress = true;
            _uploadStartMs = millis();
            _blockCount = 0;
            sendFileStartRecord(filename, fileLength);
        }

        // Send the block
        sendFileBlock(index, data, len);
        _blockCount++;

        // For timeouts        
        _uploadLastBlockMs = millis();

        // Check if that was the final block
        if (final)
        {
            sendFileEndRecord(_blockCount);
            _uploadInProgress = false;
        }
    }

};