// CommandSerial 
// Rob Dobson 2018

#pragma once
#include "Arduino.h"
#include "HardwareSerial.h"
#include "Utils.h"
#include "MiniHDLC.h"
#include "ConfigBase.h"
#include "FileManager.h"

typedef std::function<void (const uint8_t *framebuffer, int framelength)> CommandSerialFrameRxFnType;

class CommandSerial
{
private:
    // Serial
    HardwareSerial *_pSerial;

    // Serial port details
    int _serialPortNum;
    int _baudRate;

    // HDLC processor
    MiniHDLC _miniHDLC;

    // Upload of files
    bool _uploadFromFSInProgress;
    bool _uploadFromAPIInProgress;
    int _blockCount;
    unsigned long _uploadStartMs;
    unsigned long _uploadLastBlockMs;
    String _uploadTargetCommandWhenComplete;
    String _uploadFileType;
    static const int MAX_UPLOAD_MS = 600000;
    static const int MAX_BETWEEN_BLOCKS_MS = 20000;
    static const int DEFAULT_BETWEEN_BLOCKS_MS = 10;

    // Frame handling callback
    CommandSerialFrameRxFnType _frameRxCallback;

    // File manager
    FileManager& _fileManager;

public:
    CommandSerial(FileManager& fileManager) : 
            _miniHDLC(std::bind(&CommandSerial::sendCharToCmdPort, this, std::placeholders::_1), 
                std::bind(&CommandSerial::frameHandler, this, std::placeholders::_1, std::placeholders::_2),
                true, false),
            _fileManager(fileManager)
    {
        _pSerial = NULL;
        _serialPortNum = -1;
        _uploadFromFSInProgress = false;
        _uploadFromAPIInProgress = false;
        _uploadStartMs = 0;
        _uploadLastBlockMs = 0;
        _blockCount = 0;
        _baudRate = 115200;
    }

    void setup(ConfigBase& config);

    // Set callback on frame received
    void setCallbackOnRxFrame(CommandSerialFrameRxFnType frameRxCallback);
    
    // Log message
    void logMessage(String& msg);

    // Event message
    void eventMessage(String& msgJson);

    // Event message
    void responseMessage(String& msgJson);

    // Upload in progress
    bool uploadInProgress();

    // Service 
    void service();

    void sendFileStartRecord(const char* fileType, String& filename, int fileLength);
    void sendFileBlock(size_t index, uint8_t *data, size_t len);
    void sendFileEndRecord(int blockCount);
    void sendTargetCommand(const String& targetCmd);
    void sendTargetData(const String& cmdName, const uint8_t* pData, int len, int index);
    void uploadAPIBlockHandler(const char* fileType, String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool finalBlock);

    // Upload a file from the file system
    bool startUploadFromFileSystem(String& fileSystemName, String& filename,
                    const char* pTargetCmdWhenDone = NULL);

private:
    void sendCharToCmdPort(uint8_t ch);
    void frameHandler(const uint8_t *framebuffer, int framelength);
    void uploadCommonBlockHandler(const char* fileType, String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool finalBlock);
};