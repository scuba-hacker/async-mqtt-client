#include "Connect.hpp"

using AsyncMqttClientInternals::ConnectOutPacket;

ConnectOutPacket::ConnectOutPacket(bool cleanSession,
                                   const char* username,
                                   const char* password,
                                   const char* willTopic,
                                   bool willRetain,
                                   uint8_t willQos,
                                   const char* willPayload,
                                   uint16_t willPayloadLength,
                                   uint16_t keepAlive,
                                   const char* clientId)
#if ASYNCMQTT_USE_PSRAM_BUFFER
: _psramBuffer(nullptr), _psramSize(0)
#endif
{
  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.CONNECT;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.CONNECT_RESERVED;

  uint16_t protocolNameLength = 4;
  char protocolNameLengthBytes[2];
  protocolNameLengthBytes[0] = protocolNameLength >> 8;
  protocolNameLengthBytes[1] = protocolNameLength & 0xFF;

  char protocolLevel[1];
  protocolLevel[0] = 0x04;

  char connectFlags[1];
  connectFlags[0] = 0;
  if (cleanSession) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.CLEAN_SESSION;
  if (username != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.USERNAME;
  if (password != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.PASSWORD;
  if (willTopic != nullptr) {
    connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL;
    if (willRetain) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_RETAIN;
    switch (willQos) {
      case 0:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS0;
        break;
      case 1:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS1;
        break;
      case 2:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS2;
        break;
    }
  }

  char keepAliveBytes[2];
  keepAliveBytes[0] = keepAlive >> 8;
  keepAliveBytes[1] = keepAlive & 0xFF;

  uint16_t clientIdLength = strlen(clientId);
  char clientIdLengthBytes[2];
  clientIdLengthBytes[0] = clientIdLength >> 8;
  clientIdLengthBytes[1] = clientIdLength & 0xFF;

  // Optional fields
  uint16_t willTopicLength = 0;
  char willTopicLengthBytes[2];
  char willPayloadLengthBytes[2];
  if (willTopic != nullptr) {
    willTopicLength = strlen(willTopic);
    willTopicLengthBytes[0] = willTopicLength >> 8;
    willTopicLengthBytes[1] = willTopicLength & 0xFF;

    if (willPayload != nullptr && willPayloadLength == 0) willPayloadLength = strlen(willPayload);

    willPayloadLengthBytes[0] = willPayloadLength >> 8;
    willPayloadLengthBytes[1] = willPayloadLength & 0xFF;
  }

  uint16_t usernameLength = 0;
  char usernameLengthBytes[2];
  if (username != nullptr) {
    usernameLength = strlen(username);
    usernameLengthBytes[0] = usernameLength >> 8;
    usernameLengthBytes[1] = usernameLength & 0xFF;
  }

  uint16_t passwordLength = 0;
  char passwordLengthBytes[2];
  if (password != nullptr) {
    passwordLength = strlen(password);
    passwordLengthBytes[0] = passwordLength >> 8;
    passwordLengthBytes[1] = passwordLength & 0xFF;
  }

  uint32_t remainingLength = 2 + protocolNameLength + 1 + 1 + 2 + 2 + clientIdLength;  // always present
  if (willTopic != nullptr) remainingLength += 2 + willTopicLength + 2 + willPayloadLength;
  if (username != nullptr) remainingLength += 2 + usernameLength;
  if (password != nullptr) remainingLength += 2 + passwordLength;
  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(remainingLength, fixedHeader + 1);

  uint32_t neededSpace = 1 + remainingLengthLength;
  neededSpace += 2;
  neededSpace += protocolNameLength;
  neededSpace += 1;
  neededSpace += 1;
  neededSpace += 2;
  neededSpace += 2;
  neededSpace += clientIdLength;
  if (willTopic != nullptr) {
    neededSpace += 2;
    neededSpace += willTopicLength;

    neededSpace += 2;
    if (willPayload != nullptr) neededSpace += willPayloadLength;
  }
  if (username != nullptr) {
    neededSpace += 2;
    neededSpace += usernameLength;
  }
  if (password != nullptr) {
    neededSpace += 2;
    neededSpace += passwordLength;
  }

#if ASYNCMQTT_USE_PSRAM_BUFFER
  // Get buffer from PSRAM pool and use it to build the packet
  uint8_t* psramBuffer = PSRAMBufferPool::allocate(neededSpace);
  size_t bufferOffset = 0;
#else
  // Use regular vector allocation
  _data.reserve(neededSpace);
#endif

#if ASYNCMQTT_USE_PSRAM_BUFFER
  // Copy data directly to PSRAM buffer
  memcpy(psramBuffer + bufferOffset, fixedHeader, 1 + remainingLengthLength);
  bufferOffset += 1 + remainingLengthLength;

  psramBuffer[bufferOffset++] = protocolNameLengthBytes[0];
  psramBuffer[bufferOffset++] = protocolNameLengthBytes[1];

  psramBuffer[bufferOffset++] = 'M';
  psramBuffer[bufferOffset++] = 'Q';
  psramBuffer[bufferOffset++] = 'T';
  psramBuffer[bufferOffset++] = 'T';

  psramBuffer[bufferOffset++] = protocolLevel[0];
  psramBuffer[bufferOffset++] = connectFlags[0];
  psramBuffer[bufferOffset++] = keepAliveBytes[0];
  psramBuffer[bufferOffset++] = keepAliveBytes[1];
  psramBuffer[bufferOffset++] = clientIdLengthBytes[0];
  psramBuffer[bufferOffset++] = clientIdLengthBytes[1];

  memcpy(psramBuffer + bufferOffset, clientId, clientIdLength);
  bufferOffset += clientIdLength;

  if (willTopic != nullptr) {
    memcpy(psramBuffer + bufferOffset, willTopicLengthBytes, 2);
    bufferOffset += 2;
    memcpy(psramBuffer + bufferOffset, willTopic, willTopicLength);
    bufferOffset += willTopicLength;

    memcpy(psramBuffer + bufferOffset, willPayloadLengthBytes, 2);
    bufferOffset += 2;
    if (willPayload != nullptr) {
      memcpy(psramBuffer + bufferOffset, willPayload, willPayloadLength);
      bufferOffset += willPayloadLength;
    }
  }
  if (username != nullptr) {
    memcpy(psramBuffer + bufferOffset, usernameLengthBytes, 2);
    bufferOffset += 2;
    memcpy(psramBuffer + bufferOffset, username, usernameLength);
    bufferOffset += usernameLength;
  }
  if (password != nullptr) {
    memcpy(psramBuffer + bufferOffset, passwordLengthBytes, 2);
    bufferOffset += 2;
    memcpy(psramBuffer + bufferOffset, password, passwordLength);
    bufferOffset += passwordLength;
  }

  // Store PSRAM buffer pointer and size directly
  _psramBuffer = psramBuffer;
  _psramSize = neededSpace;

  // Vector should remain empty when using PSRAM
#else
  // Use original vector insertion method
  _data.insert(_data.end(), fixedHeader, fixedHeader + 1 + remainingLengthLength);

  _data.push_back(protocolNameLengthBytes[0]);
  _data.push_back(protocolNameLengthBytes[1]);

  _data.push_back('M');
  _data.push_back('Q');
  _data.push_back('T');
  _data.push_back('T');

  _data.push_back(protocolLevel[0]);
  _data.push_back(connectFlags[0]);
  _data.push_back(keepAliveBytes[0]);
  _data.push_back(keepAliveBytes[1]);
  _data.push_back(clientIdLengthBytes[0]);
  _data.push_back(clientIdLengthBytes[1]);

  _data.insert(_data.end(), clientId, clientId + clientIdLength);
  if (willTopic != nullptr) {
    _data.insert(_data.end(), willTopicLengthBytes, willTopicLengthBytes + 2);
    _data.insert(_data.end(), willTopic, willTopic + willTopicLength);

    _data.insert(_data.end(), willPayloadLengthBytes, willPayloadLengthBytes + 2);
    if (willPayload != nullptr) _data.insert(_data.end(), willPayload, willPayload + willPayloadLength);
  }
  if (username != nullptr) {
    _data.insert(_data.end(), usernameLengthBytes, usernameLengthBytes + 2);
    _data.insert(_data.end(), username, username + usernameLength);
  }
  if (password != nullptr) {
    _data.insert(_data.end(), passwordLengthBytes, passwordLengthBytes + 2);
    _data.insert(_data.end(), password, password + passwordLength);
  }
#endif
}

const uint8_t* ConnectOutPacket::data(size_t index) const {
#if ASYNCMQTT_USE_PSRAM_BUFFER
  return _psramBuffer + index;
#else
  return &_data.data()[index];
#endif
}

size_t ConnectOutPacket::size() const {
#if ASYNCMQTT_USE_PSRAM_BUFFER
  return _psramSize;
#else
  return _data.size();
#endif
}
