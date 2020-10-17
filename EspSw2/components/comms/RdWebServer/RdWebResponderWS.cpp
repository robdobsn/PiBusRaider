/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebResponderWS.h"
#include <RdWebConnection.h>
#include "RdWebServerSettings.h"
#include <Logger.h>

// #define DEBUG_RESPONDER_WS
// #define DEBUG_WS_SEND_APP_DATA
#if defined(DEBUG_RESPONDER_WS) || defined(DEBUG_WS_SEND_APP_DATA)
static const char *MODULE_PREFIX = "RdWebRespWS";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponderWS::RdWebResponderWS(RdWebHandler* pWebHandler, const RdWebRequestParams& params, 
            const String& reqStr, RdWebSocketCB webSocketCB, const RdWebServerSettings& webServerSettings)
    : _reqParams(params)
{
    // Store socket info
    _pWebHandler = pWebHandler;
    _requestStr = reqStr;
    _webSocketCB = webSocketCB;

    // Init socket link
    _webSocketLink.setup(webSocketCB, params.getWebConnRawSend(), webServerSettings.pingIntervalMs, true);
}

RdWebResponderWS::~RdWebResponderWS()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderWS::service()
{
    // Service the link
    _webSocketLink.service();

    // Check for data waiting to be sent
    RdWebDataFrame frame;
    if (_txQueue.get(frame))
    {
#ifdef DEBUG_WS_SEND_APP_DATA
        LOG_W(MODULE_PREFIX, "service sendMsg len %d", frame.getLen());
#endif
        // Send
        _webSocketLink.sendMsg(WEBSOCKET_OPCODE_BINARY, frame.getData(), frame.getLen());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle inbound data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::handleData(const uint8_t* pBuf, uint32_t dataLen)
{
#ifdef DEBUG_RESPONDER_WS
    LOG_I(MODULE_PREFIX, "handleData len %d", dataLen);
#endif

    // Handle it with link
    _webSocketLink.handleRxData(pBuf, dataLen);

    // Check if the link is still active
    if (!_webSocketLink.isActive())
    {
        _isActive = false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start responding
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::startResponding(RdWebConnection& request)
{
    // Set link to upgrade-request already received state
    _webSocketLink.upgradeReceived(request.getHeader().webSocketKey, 
                        request.getHeader().webSocketVersion);

    // Now active
    _isActive = true;
#ifdef DEBUG_RESPONDER_WS
    LOG_I(MODULE_PREFIX, "startResponding isActive %d", _isActive);
#endif
    return _isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get response next
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebResponderWS::getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen)
{
    // Get from the WSLink
    uint32_t respLen = _webSocketLink.getTxData(pBuf, bufMaxLen);

    // Done response
#ifdef DEBUG_RESPONDER_WS
    if (respLen > 0)
        LOG_I(MODULE_PREFIX, "getResponseNext respLen %d isActive %d", respLen, _isActive);
#endif
    return respLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* RdWebResponderWS::getContentType()
{
    return "application/json";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leave connection open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderWS::leaveConnOpen()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Send a frame of data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderWS::sendFrame(const uint8_t* pBuf, uint32_t bufLen)
{
    // Add to queue - don't block if full
    RdWebDataFrame frame(pBuf, bufLen);
    _txQueue.put(frame);
#ifdef DEBUG_WS_SEND_APP_DATA
    LOG_W(MODULE_PREFIX, "sendFrame len %d", bufLen);
#endif
}