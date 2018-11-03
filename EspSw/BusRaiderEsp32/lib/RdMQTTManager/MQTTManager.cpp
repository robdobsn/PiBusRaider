// MQTT Manager
// Rob Dobson 2018

#include "MQTTManager.h"

#ifdef MQTT_USE_ASYNC_MQTT

void MQTTManager::onMqttConnect(bool sessionPresent)
{
    Log.notice("MQTTManager: Connected to MQTT, session \n", sessionPresent ? "yes" : "no");
    if (_mqttInTopic.length() > 0)
    {
        uint16_t packetIdSub = _mqttClient.subscribe(_mqttInTopic.c_str(), 2);
        Log.notice("MQTTManager: Subscribing to %s at QoS 2, packetId: %d\n", _mqttInTopic.c_str(), packetIdSub);
    }
    if (_mqttOutTopic.length() > 0 && _mqttMsgToSendWhenConnected.length() > 0)
    {
        // Send message "queued" to be sent
        uint16_t packetIdPub1 = _mqttClient.publish(_mqttOutTopic.c_str(), 1, true, _mqttMsgToSendWhenConnected.c_str());
        Log.trace("MQTTManager: Published to %s at QoS 1, packetId: %d\n", _mqttOutTopic.c_str(), packetIdPub1);
        _mqttMsgToSendWhenConnected = "";
    }
}

void MQTTManager::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    if (_wasConnected)
    {
        // Set last connected time here so we hold off for a short time after disconnect
        // before trying to reconnect
        _lastConnectRetryMs = millis();
        Log.notice("MQTTManager: Disconnect (was connected) reason\n", reason);
        // Set server for next connection (in case it was changed)
        _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
        _wasConnected = false;
        return;
    }

    Log.notice("MQTTManager: Disconnect reason %d\n", reason);
    // Set server for next connection (in case it was changed)
    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _wasConnected = false;
}

void MQTTManager::onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
    Log.verbose("MQTTManager: Subscribe acknowledged packetId: %d qos: %d\n", packetId, qos);
}

void MQTTManager::onMqttUnsubscribe(uint16_t packetId)
{
    Log.verbose("MQTTManager: Unsubscribe acknowledged packetId: %d\n", packetId);
}

void MQTTManager::onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    if (len > MAX_PAYLOAD_LEN)
    {
        Log.warning("MQTTManager: received topic %s msgTooLong qos %d dup %d retain %d index %d total %d\n",
                    topic, properties.qos, properties.dup, properties.retain, index, total);
        len = MAX_PAYLOAD_LEN;
    }

    Log.verbose("MQTTManager: rx topic %s qos %d dup %d retain %d len %d idx %d total %d\n",
                topic, properties.qos, properties.dup, properties.retain,
                len, index, total);

    // Copy the message and ensure terminated
    char *payloadStr = new char[len + 1];
    memcpy(payloadStr, payload, len);
    payloadStr[len] = 0;
    // Log.verbose("MQTTManager: rx payload: %s\n", reqStr);
    String respStr;
    _restAPIEndpoints.handleApiRequest(payloadStr, respStr);

    // const int MAX_RESTAPI_REQ_LEN = 200;
    // if (len < MAX_RESTAPI_REQ_LEN)
    // {
    //     char reqStr[len + 1];
    //     for (int i = 0; i < len; i++)
    //         reqStr[i] = payload[i];
    //     reqStr[len] = 0;
    //     Log.notice("MQTTManager: rx payload: %s\n", reqStr);
    //     String respStr;
    //     _restAPIEndpoints.handleApiRequest(reqStr, respStr);
    // }

#ifdef STRESS_TEST_MQTT
    char* pHello = strstr(payloadStr, "Hello_");
    if (!pHello)
    {
        Log.trace("NoHELLO topic %s msg %s qos %d dup %d retain %d index %d total %d\n", 
            topic, payloadStr, properties.qos, properties.dup, properties.retain, index, total);
    }
    else
    {
        int count = atoi(payloadStr+6);
        if (curRxHelloCount != -1)
        {
            if (curRxHelloCount + 1 != count)
            {
                Log.trace("RxCountMismatch exp %d topic %s msg %s qos %d dup %d retain %d index %d total %d\n", 
                    curRxHelloCount + 1, topic, payloadStr, properties.qos, properties.dup, properties.retain, index, total);
            }
        }
        curRxHelloCount = count;
    }
#endif

    // Cleanup
    delete[] payloadStr;
}

void MQTTManager::onMqttPublish(uint16_t packetId)
{
    Log.verbose("MQTTManager: Publish acknowledged packetId: %d\n", packetId);
}

#else

void MQTTManager::onMqttMessage(char *topic, byte *payload, unsigned int len)
{
    Log.verbose("MQTTManager: rx topic: %s len %d\n", topic, len);
    const int MAX_RESTAPI_REQ_LEN = 200;
    if (len < MAX_RESTAPI_REQ_LEN)
    {
        char reqStr[len + 1];
        for (int i = 0; i < len; i++)
            reqStr[i] = payload[i];
        reqStr[len] = 0;
        // Log.notice("MQTTManager: rx payload: %s\n", reqStr);
        String respStr;
        _restAPIEndpoints.handleApiRequest(reqStr, respStr);
    }
}
#endif

void MQTTManager::setup(ConfigBase& hwConfig, ConfigBase *pConfig)
{
    // Check enabled
    _mqttEnabled = hwConfig.getLong("mqttEnabled", 0) != 0;
    _pConfigBase = pConfig;
    if ((!_mqttEnabled) || (!pConfig))
        return;

    // Get the server, port and in/out topics if available
    _mqttServer = pConfig->getString("MQTTServer", "");
    String mqttPortStr = pConfig->getString("MQTTPort", String(DEFAULT_MQTT_PORT).c_str());
    _mqttPort = mqttPortStr.toInt();
    _mqttInTopic = pConfig->getString("MQTTInTopic", "");
    _mqttOutTopic = pConfig->getString("MQTTOutTopic", "");
    if (_mqttServer.length() == 0)
        return;

    // Debug
    Log.notice("MQTTManager: server %s:%d, inTopic %s, outTopic %s\n", _mqttServer.c_str(), _mqttPort, _mqttInTopic.c_str(), _mqttOutTopic.c_str());

#ifdef MQTT_USE_ASYNC_MQTT
    // Setup handlers for MQTT events
    _mqttClient.onConnect(std::bind(&MQTTManager::onMqttConnect, this, std::placeholders::_1));
    _mqttClient.onDisconnect(std::bind(&MQTTManager::onMqttDisconnect, this, std::placeholders::_1));
    _mqttClient.onSubscribe(std::bind(&MQTTManager::onMqttSubscribe, this, std::placeholders::_1, std::placeholders::_2));
    _mqttClient.onUnsubscribe(std::bind(&MQTTManager::onMqttUnsubscribe, this, std::placeholders::_1));
    _mqttClient.onMessage(std::bind(&MQTTManager::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    _mqttClient.onPublish(std::bind(&MQTTManager::onMqttPublish, this, std::placeholders::_1));
#endif

    // Setup server and callback on receive
    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);

#ifndef MQTT_USE_ASYNC_MQTT
    _mqttClient.setCallback(std::bind(&MQTTManager::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3));
#endif
}

String MQTTManager::formConfigStr()
{
    // This string is stored in NV ram for configuration on power up
    return "{\"MQTTServer\":\"" + _mqttServer + "\",\"MQTTPort\":\"" + String(_mqttPort) + "\",\"MQTTInTopic\":\"" +
            _mqttInTopic + "\",\"MQTTOutTopic\":\"" + _mqttOutTopic + "\"}";
}

void MQTTManager::setMQTTServer(String &mqttServer, String &mqttInTopic, 
                String &mqttOutTopic, int mqttPort)
{
    _mqttServer = mqttServer;
    if (mqttInTopic.length() > 0)
        _mqttInTopic = mqttInTopic;
    if (mqttOutTopic.length() > 0)
        _mqttOutTopic = mqttOutTopic;
    _mqttPort = mqttPort;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }

    // Check if enabled
    if (!_mqttEnabled)
        return;
    // Disconnect so re-connection with new credentials occurs
    if (_mqttClient.connected())
    {
        Log.trace("MQTTManager: setMQTTServer disconnecting to allow new connection\n");
#ifdef MQTT_USE_ASYNC_MQTT
        _mqttClient.disconnect(false);
#else
        _mqttClient.disconnect();
#endif
        // Indicate that server credentials need to be set when disconnect is complete
        _wasConnected = true;
    }
    else
    {
        Log.notice("MQTTManager: setMQTTServer %s:%n\n", _mqttServer.c_str(), _mqttPort);
        _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    }
}

void MQTTManager::reportJson(String& msg)
{
    report(msg.c_str());
}

void MQTTManager::report(const char *reportStr)
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

    // Check if connected
    if (!_mqttClient.connected())
    {
        // Store until connected
        if (_mqttMsgToSendWhenConnected.length() == 0)
            _mqttMsgToSendWhenConnected = reportStr;
        return;
    }

    // Send immediately
#ifdef MQTT_USE_ASYNC_MQTT
    int publishRslt = _mqttClient.publish(_mqttOutTopic.c_str(), 1, true, reportStr);
#else
    int publishRslt = _mqttClient.publish(_mqttOutTopic.c_str(), reportStr, true);
#endif
    Log.verbose("MQTTManager: Published to %s at QoS 0, publishRslt %d\n", _mqttOutTopic.c_str(), publishRslt);
}

// Note do not put any Log messages in here as MQTT may be used for logging
// and an infinite loop would result
void MQTTManager::reportSilent(const char *reportStr)
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

    // Check if connected
    if (!_mqttClient.connected())
    {
        // Store until connected
        if (_mqttMsgToSendWhenConnected.length() == 0)
            _mqttMsgToSendWhenConnected = reportStr;
        return;
    }

    // Send immediately
#ifdef MQTT_USE_ASYNC_MQTT
    _mqttClient.publish(_mqttOutTopic.c_str(), 1, true, reportStr);
#else
    _mqttClient.publish(_mqttOutTopic.c_str(), reportStr, true);
#endif
}

void MQTTManager::service()
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

#ifdef MQTT_USE_ASYNC_MQTT
    if (!_mqttClient.connected())
    {
        if (_wifiManager.isConnected() && _mqttServer.length() > 0)
        {
            if (Utils::isTimeout(millis(), _lastConnectRetryMs, CONNECT_RETRY_MS))
            {
                Log.notice("MQTTManager: Connecting to MQTT client\n");
                _mqttClient.connect();
                _lastConnectRetryMs = millis();
            }
        }
    }

#ifdef STRESS_TEST_MQTT
if (millis() > _stressTestLastSendTime + 500)
{
    String testMsg = "test 0_" + String(_stressTestCounts[0]++);
    _mqttClient.publish("test/lol", 0, true, testMsg.c_str());
    testMsg = "test 1_" + String(_stressTestCounts[1]++);
    _mqttClient.publish("test/lol", 1, true, testMsg.c_str());
    testMsg = "test 2_" + String(_stressTestCounts[2]++);
    _mqttClient.publish("test/lol", 2, true, testMsg.c_str());
    _stressTestLastSendTime = millis();
}
#endif

#else
    // See if we are connected
    if (_mqttClient.connected())
    {
        // Service the client
        _mqttClient.loop();
    }
    else
    {

        // Check if we were previously connected
        if (_wasConnected)
        {
            // Set last connected time here so we hold off for a short time after disconnect
            // before trying to reconnect
            _lastConnectRetryMs = millis();
            Log.notice("MQTTManager: Disconnected, status code %d\n", _mqttClient.state());
            // Set server for next connection (in case it was changed)
            _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
            _wasConnected = false;
            return;
        }

        // Check if ready to reconnect
        if (_wifiManager.isConnected() && _mqttServer.length() > 0)
        {
            if (Utils::isTimeout(millis(), _lastConnectRetryMs, CONNECT_RETRY_MS))
            {
                Log.notice("MQTTManager: Connecting to MQTT client\n");
                if (_mqttClient.connect(_wifiManager.getHostname().c_str()))
                {
                    // Connected
                    Log.notice("MQTTManager: Connected to MQTT\n");
                    _wasConnected = true;
                    
                    // Subscribe to in topic
                    if (_mqttInTopic.length() > 0)
                    {
                        bool subscribedOk = _mqttClient.subscribe(_mqttInTopic.c_str(), 0);
                        Log.notice("MQTTManager: Subscribing to %s at QoS 0, subscribe %s\n", _mqttInTopic.c_str(), 
                                        subscribedOk ? "OK" : "Failed");
                    }

                    // Re-send last message sent when disconnected (if any)
                    if (_mqttOutTopic.length() > 0 && _mqttMsgToSendWhenConnected.length() > 0)
                    {
                        // Send message "queued" to be sent
                        bool publishedOk = _mqttClient.publish(_mqttOutTopic.c_str(), _mqttMsgToSendWhenConnected.c_str(), true);
                        Log.verbose("MQTTManager: Published to %s at QoS 0, result %s\n", _mqttOutTopic.c_str(), 
                                                publishedOk ? "OK" : "Failed");
                        _mqttMsgToSendWhenConnected = "";
                    }

                }
                else
                {
                    Log.notice("MQTTManager: Connect failed\n");
                }
                _lastConnectRetryMs = millis();
            }
        }
    }
#endif
}


