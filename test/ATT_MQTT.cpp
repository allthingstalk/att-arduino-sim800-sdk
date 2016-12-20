// The MIT License (MIT)
//
// Copyright (c) 2015 Adafruit Industries
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include "ATT_MQTT.h"

#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_SAMD_MKR1000)
static char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, val);
  return sout;
}
#endif

#if defined(ESP8266)
int strncasecmp(const char * str1, const char * str2, int len) {
    int d = 0;
    while(len--) {
        int c1 = tolower(*str1++);
        int c2 = tolower(*str2++);
        if(((d = c1 - c2) != 0) || (c2 == '\0')) {
	  return d;
        }
    }
    return 0;
}
#endif


void printBuffer(uint8_t *buffer, uint16_t len) {
  DEBUG_PRINTER.print('\t');
  for (uint16_t i=0; i<len; i++) {
    if (isprint(buffer[i]))
      DEBUG_PRINTER.write(buffer[i]);
    else
      DEBUG_PRINTER.print(" ");
    DEBUG_PRINTER.print(F(" [0x"));
    if (buffer[i] < 0x10)
      DEBUG_PRINTER.print("0");
    DEBUG_PRINTER.print(buffer[i],HEX);
    DEBUG_PRINTER.print("], ");
    if (i % 8 == 7) {
      DEBUG_PRINTER.print("\n\t");
    }
  }
  DEBUG_PRINTER.println();
}

/* Not used now, but might be useful in the future
static uint8_t *stringprint(uint8_t *p, char *s) {
  uint16_t len = strlen(s);
  p[0] = len >> 8; p++;
  p[0] = len & 0xFF; p++;
  memmove(p, s, len);
  return p+len;
}
*/

static uint8_t *stringprint(uint8_t *p, const char *s, uint16_t maxlen=0) {
  // If maxlen is specified (has a non-zero value) then use it as the maximum
  // length of the source string to write to the buffer.  Otherwise write
  // the entire source string.
  uint16_t len = strlen(s);
  if (maxlen > 0 && len > maxlen) {
    len = maxlen;
  }
  /*
  for (uint8_t i=0; i<len; i++) {
    Serial.write(pgm_read_byte(s+i));
  }
  */
  p[0] = len >> 8; p++;
  p[0] = len & 0xFF; p++;
  strncpy((char *)p, s, len);
  return p+len;
}


// ATT_MQTT Definition ////////////////////////////////////////////////////

ATT_MQTT::ATT_MQTT(const char *server,
                             uint16_t port,
                             const char *cid,
                             const char *user,
                             const char *pass) {
  servername = server;
  portnum = port;
  username = user;
  password = pass;

  will_topic = 0;
  will_payload = 0;
  will_qos = 0;
  will_retain = 0;

  packet_id_counter = 0;

}


ATT_MQTT::ATT_MQTT(const char *server,
                             uint16_t port,
                             const char *user,
                             const char *pass) {
  servername = server;
  portnum = port;
  


  will_topic = 0;
  will_payload = 0;
  will_qos = 0;
  will_retain = 0;

  packet_id_counter = 0;

}

void ATT_MQTT::setCredentials(const char* cid, const char* user, const char* pwd)
{
	username = user;
	password = pwd;
	clientid = cid;
}

int8_t ATT_MQTT::connect() {
  // Connect to the server.
  if (!connectServer())
    return -1;

  // Construct and send connect packet.
  uint8_t len = connectPacket(buffer);
  if (!sendPacket(buffer, len))
    return -1;

  // Read connect response packet and verify it
  len = readFullPacket(buffer, MAXBUFFERSIZE, CONNECT_TIMEOUT_MS);
  if (len != 4)
    return -1;
  if ((buffer[0] != (MQTT_CTRL_CONNECTACK << 4)) || (buffer[1] != 2))
    return -1;
  if (buffer[3] != 0)
    return buffer[3];


  return 0;
}

int8_t ATT_MQTT::connect(const char *user, const char *pass)
{
  username = user;
  password = pass;
  return connect();
}

uint16_t ATT_MQTT::processPacketsUntil(uint8_t *buffer, uint8_t waitforpackettype, uint16_t timeout) {
  uint16_t len;
  while ((len = readFullPacket(buffer, MAXBUFFERSIZE, timeout))) {

    //DEBUG_PRINT("Packet read size: "); DEBUG_PRINTLN(len);
    // TODO: add subscription reading & call back processing here

    if ((buffer[0] >> 4) == waitforpackettype) {
      //DEBUG_PRINTLN(F("Found right packet")); 
      return len;
    } else {
      ERROR_PRINTLN(F("Dropped a packet"));
    }
  }
  return 0;
}

uint16_t ATT_MQTT::readFullPacket(uint8_t *buffer, uint16_t maxsize, uint16_t timeout) {
  // will read a packet and Do The Right Thing with length
  uint8_t *pbuff = buffer;

  uint8_t rlen;

  // read the packet type:
  rlen = readPacket(pbuff, 1, timeout);
  if (rlen != 1) return 0;

  DEBUG_PRINT(F("Packet Type:\t")); DEBUG_PRINTBUFFER(pbuff, rlen);
  pbuff++;

  uint32_t value = 0;
  uint32_t multiplier = 1;
  uint8_t encodedByte;

  do {
    rlen = readPacket(pbuff, 1, timeout);
    if (rlen != 1) return 0;
    encodedByte = pbuff[0]; // save the last read val
    pbuff++; // get ready for reading the next byte
    uint32_t intermediate = encodedByte & 0x7F;
    intermediate *= multiplier;
    value += intermediate;
    multiplier *= 128;
    if (multiplier > (128UL*128UL*128UL)) {
      DEBUG_PRINT(F("Malformed packet len\n"));
      return 0;
    }
  } while (encodedByte & 0x80);

  DEBUG_PRINT(F("Packet Length:\t")); DEBUG_PRINTLN(value);
  
  if (value > (uint32_t)(maxsize - (pbuff-buffer) - 1)) {
      DEBUG_PRINTLN(F("Packet too big for buffer"));
      rlen = readPacket(pbuff, (maxsize - (pbuff-buffer) - 1), timeout);
  } else {
    rlen = readPacket(pbuff, value, timeout);
  }
  //DEBUG_PRINT(F("Remaining packet:\t")); DEBUG_PRINTBUFFER(pbuff, rlen);

  return ((pbuff - buffer)+rlen);
}

const __FlashStringHelper* ATT_MQTT::connectErrorString(int8_t code) {
   switch (code) {
      case 1: return F("The Server does not support the level of the MQTT protocol requested");
      case 2: return F("The Client identifier is correct UTF-8 but not allowed by the Server");
      case 3: return F("The MQTT service is unavailable");
      case 4: return F("The data in the user name or password is malformed");
      case 5: return F("Not authorized to connect");
      case 6: return F("Exceeded reconnect rate limit. Please try again later.");
      case 7: return F("You have been banned from connecting. Please contact the MQTT server administrator for more details.");
      case -1: return F("Connection failed");
      case -2: return F("Failed to subscribe");
      default: return F("Unknown error");
   }
}

bool ATT_MQTT::disconnect() {

  // Construct and send disconnect packet.
  uint8_t len = disconnectPacket(buffer);
  if (! sendPacket(buffer, len))
    DEBUG_PRINTLN(F("Unable to send disconnect packet"));

  return disconnectServer();

}


bool ATT_MQTT::publish(const char *topic, const char *data, uint8_t qos) {
    return publish(topic, (uint8_t*)(data), strlen(data), qos);
}

bool ATT_MQTT::publish(const char *topic, uint8_t *data, uint16_t bLen, uint8_t qos) {
  // Construct and send publish packet.
  uint16_t len = publishPacket(buffer, topic, data, bLen, qos);
  if (!sendPacket(buffer, len))
    return false;

  // If QOS level is high enough verify the response packet.
  if (qos > 0) {
    len = readFullPacket(buffer, MAXBUFFERSIZE, PUBLISH_TIMEOUT_MS);
    DEBUG_PRINT(F("Publish QOS1+ reply:\t"));
    DEBUG_PRINTBUFFER(buffer, len);
    if (len != 4)
      return false;
    if ((buffer[0] >> 4) != MQTT_CTRL_PUBACK)
      return false;
    uint16_t packnum = buffer[2];
    packnum <<= 8;
    packnum |= buffer[3];

    // we increment the packet_id_counter right after publishing so inc here too to match
    packnum++;
    if (packnum != packet_id_counter)
      return false;
  }

  return true;
}

bool ATT_MQTT::will(const char *topic, const char *payload, uint8_t qos, uint8_t retain) {

  if (connected()) {
    DEBUG_PRINT(F("Will defined after connect"));
    return false;
  }

  will_topic = topic;
  will_payload = payload;
  will_qos = qos;
  will_retain = retain;

  return true;

}

bool ATT_MQTT::subscribe(const char* topic, uint8_t qos) {
	boolean success = false;
    for (uint8_t retry=0; (retry<3) && !success; retry++) { // retry until we get a suback    
		// Construct and send subscription packet.
		uint8_t len = subscribePacket(buffer, topic, qos);
		if (!sendPacket(buffer, len))
			break;

		if(MQTT_PROTOCOL_LEVEL < 3) // older versions didn't suback
			break;

		// Check for SUBACK if using MQTT 3.1.1 or higher
		// TODO: The Server is permitted to start sending PUBLISH packets matching the
		// Subscription before the Server sends the SUBACK Packet. (will really need to use callbacks - ada)

		//Serial.println("\t**looking for suback");
		if (processPacketsUntil(buffer, MQTT_CTRL_SUBACK, SUBACK_TIMEOUT_MS)) {
			success = true;
			break;
		}
		//Serial.println("\t**failed, retrying!");
    }
	return success;
}

bool ATT_MQTT::unsubscribe(const char* topic, uint8_t qos) 
{
	DEBUG_PRINTLN(F("Found matching subscription and attempting to unsubscribe."));

    // Construct and send unsubscribe packet.
    uint8_t len = unsubscribePacket(buffer, topic);

    // sending unsubscribe failed
    if (! sendPacket(buffer, len))
        return false;

    // if QoS for this subscription is 1 or 2, we need
    // to wait for the unsuback to confirm unsubscription
    if(qos > 0 && MQTT_PROTOCOL_LEVEL > 3) {

        // wait for UNSUBACK
        len = readFullPacket(buffer, MAXBUFFERSIZE, CONNECT_TIMEOUT_MS);
        DEBUG_PRINT(F("UNSUBACK:\t"));
        DEBUG_PRINTBUFFER(buffer, len);

        if ((len != 5) || (buffer[0] != (MQTT_CTRL_UNSUBACK << 4))) 
			return false;  // failure to unsubscribe
    }
    return true;
}

void ATT_MQTT::setCallback(void(*callback)(const char*,const char*,unsigned int)){
    this->callback = callback;
}

void ATT_MQTT::processPackets(int16_t timeout) 
{
	char topic[MAXBUFFERSIZE];
	char data[SUBSCRIPTIONDATALEN];
	unsigned int data_len;
	while (readSubscription(topic, data, data_len, timeout)) {
		Serial.println("**** sub packet received");
		if(this->callback != NULL){
			this->callback(topic, data, data_len);
		}
	}
}

bool ATT_MQTT::readSubscription(char* topic,  char* data, unsigned int& datalen, int16_t timeout) {
  uint16_t topiclen;

	// Check if data is available to read.
	uint16_t len = readFullPacket(buffer, MAXBUFFERSIZE, timeout); // return one full packet
	if (!len)
		return false;  // No data available, just quit.
	DEBUG_PRINT("Packet len: "); DEBUG_PRINTLN(len); 
	DEBUG_PRINTBUFFER(buffer, len);
	//*topic = (char*)buffer+4;
	topiclen = buffer[3];
	memset(topic, 0, MAXBUFFERSIZE);
	memmove(topic, buffer+4, topiclen);
	// Parse out length of packet.
	DEBUG_PRINT(F("found topic len ")); DEBUG_PRINTLN(topiclen);
  
  uint8_t packet_id_len = 0;
  uint16_t packetid = 0;
  // Check if it is QoS 1, TODO: we dont support QoS 2
  if ((buffer[0] & 0x6) == 0x2) {
    packet_id_len = 2;
    packetid = buffer[topiclen+4];
    packetid <<= 8;
    packetid |= buffer[topiclen+5];
  }

  memset(data, 0, SUBSCRIPTIONDATALEN);

  datalen = len - topiclen - packet_id_len - 4;
  if (datalen > SUBSCRIPTIONDATALEN) {
    datalen = SUBSCRIPTIONDATALEN-1; // cut it off
  }
  // extract out just the data, into the subscription object itself
  memmove(data, buffer+4+topiclen+packet_id_len, datalen);
  DEBUG_PRINT(F("Data len: ")); DEBUG_PRINTLN(datalen);
  DEBUG_PRINT(F("Data: ")); DEBUG_PRINTLN((char*)data);
  
  
  DEBUG_PRINT(F("topic: ")); DEBUG_PRINTLN(*topic);

  if ((MQTT_PROTOCOL_LEVEL > 3) &&(buffer[0] & 0x6) == 0x2) {
    uint8_t ackpacket[4];
    
    // Construct and send puback packet.
    uint8_t len = pubackPacket(ackpacket, packetid);
    if (!sendPacket(ackpacket, len))
      DEBUG_PRINT(F("Failed"));
  }
  return true;
}

void ATT_MQTT::flushIncoming(uint16_t timeout) {
  // flush input!
  DEBUG_PRINTLN(F("Flushing input buffer"));
  while (readPacket(buffer, MAXBUFFERSIZE, timeout));
}

bool ATT_MQTT::ping(uint8_t num) {
  //flushIncoming(100);

  while (num--) {
    // Construct and send ping packet.
    uint8_t len = pingPacket(buffer);
    if (!sendPacket(buffer, len))
      continue;

    // Process ping reply.
    len = processPacketsUntil(buffer, MQTT_CTRL_PINGRESP, PING_TIMEOUT_MS);
    if (buffer[0] == (MQTT_CTRL_PINGRESP << 4))
      return true;
  }

  return false;
}

// Packet Generation Functions /////////////////////////////////////////////////

// The current MQTT spec is 3.1.1 and available here:
//   http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718028
// However this connect packet and code follows the MQTT 3.1 spec here (some
// small differences in the protocol):
//   http://public.dhe.ibm.com/software/dw/webservices/ws-mqtt/mqtt-v3r1.html#connect
uint8_t ATT_MQTT::connectPacket(uint8_t *packet) {
  uint8_t *p = packet;
  uint16_t len;

  // fixed header, connection messsage no flags
  p[0] = (MQTT_CTRL_CONNECT << 4) | 0x0;
  p+=2;
  // fill in packet[1] last

#if MQTT_PROTOCOL_LEVEL == 3
    p = stringprint(p, "MQIsdp");
#elif MQTT_PROTOCOL_LEVEL == 4
    p = stringprint(p, "MQTT");
#else
    #error "MQTT level not supported"
#endif

  p[0] = MQTT_PROTOCOL_LEVEL;
  p++;

  // always clean the session
  p[0] = MQTT_CONN_CLEANSESSION;

  // set the will flags if needed
  if (will_topic && pgm_read_byte(will_topic) != 0) {

    p[0] |= MQTT_CONN_WILLFLAG;

    if(will_qos == 1)
      p[0] |= MQTT_CONN_WILLQOS_1;
    else if(will_qos == 2)
      p[0] |= MQTT_CONN_WILLQOS_2;

    if(will_retain == 1)
      p[0] |= MQTT_CONN_WILLRETAIN;

  }

  if (pgm_read_byte(username) != 0)
    p[0] |= MQTT_CONN_USERNAMEFLAG;
  if (pgm_read_byte(password) != 0)
    p[0] |= MQTT_CONN_PASSWORDFLAG;
  p++;

  p[0] = MQTT_CONN_KEEPALIVE >> 8;
  p++;
  p[0] = MQTT_CONN_KEEPALIVE & 0xFF;
  p++;

  if(MQTT_PROTOCOL_LEVEL == 3) {
    p = stringprint(p, clientid, 23);  // Limit client ID to first 23 characters.
  } else {
    if (pgm_read_byte(clientid) != 0) {
      p = stringprint(p, clientid);
    } else {
      p[0] = 0x0;
      p++;
      p[0] = 0x0;
      p++;
      DEBUG_PRINTLN(F("SERVER GENERATING CLIENT ID"));
    }
  }

  if (will_topic && pgm_read_byte(will_topic) != 0) {
    p = stringprint(p, will_topic);
    p = stringprint(p, will_payload);
  }

  if (pgm_read_byte(username) != 0) {
    p = stringprint(p, username);
  }
  if (pgm_read_byte(password) != 0) {
    p = stringprint(p, password);
  }

  len = p - packet;

  packet[1] = len-2;  // don't include the 2 bytes of fixed header data
  DEBUG_PRINTLN(F("MQTT connect packet:"));
  DEBUG_PRINTBUFFER(buffer, len);
  return len;
}


// as per http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718040
uint16_t ATT_MQTT::publishPacket(uint8_t *packet, const char *topic,
                                     uint8_t *data, uint16_t bLen, uint8_t qos) {
  uint8_t *p = packet;
  uint16_t len=0;

  // calc length of non-header data
  len += 2;               // two bytes to set the topic size
  len += strlen(topic); // topic length
  if(qos > 0) { 
    len += 2; // qos packet id
  }
  len += bLen; // payload length

  // Now you can start generating the packet!
  p[0] = MQTT_CTRL_PUBLISH << 4 | qos << 1;
  p++;

  // fill in packet[1] last
  do {
    uint8_t encodedByte = len % 128;
    len /= 128;
    // if there are more data to encode, set the top bit of this byte
    if ( len > 0 ) {
      encodedByte |= 0x80;
    }
    p[0] = encodedByte;
    p++;
  } while ( len > 0 );

  // topic comes before packet identifier
  p = stringprint(p, topic);

  // add packet identifier. used for checking PUBACK in QOS > 0
  if(qos > 0) {
    p[0] = (packet_id_counter >> 8) & 0xFF;
    p[1] = packet_id_counter & 0xFF;
    p+=2;

    // increment the packet id
    packet_id_counter++;
  }

  memmove(p, data, bLen);
  p+= bLen;
  len = p - packet;
  DEBUG_PRINTLN(F("MQTT publish packet:"));
  DEBUG_PRINTBUFFER(buffer, len);
  return len;
}

uint8_t ATT_MQTT::subscribePacket(uint8_t *packet, const char *topic,
                                       uint8_t qos) {
  uint8_t *p = packet;
  uint16_t len;

  p[0] = MQTT_CTRL_SUBSCRIBE << 4 | MQTT_QOS_1 << 1;
  // fill in packet[1] last
  p+=2;

  // packet identifier. used for checking SUBACK
  p[0] = (packet_id_counter >> 8) & 0xFF;
  p[1] = packet_id_counter & 0xFF;
  p+=2;

  // increment the packet id
  packet_id_counter++;

  p = stringprint(p, topic);

  p[0] = qos;
  p++;

  len = p - packet;
  packet[1] = len-2; // don't include the 2 bytes of fixed header data
  DEBUG_PRINTLN(F("MQTT subscription packet:"));
  DEBUG_PRINTBUFFER(buffer, len);
  return len;
}



uint8_t ATT_MQTT::unsubscribePacket(uint8_t *packet, const char *topic) {

  uint8_t *p = packet;
  uint16_t len;

  p[0] = MQTT_CTRL_UNSUBSCRIBE << 4 | 0x1;
  // fill in packet[1] last
  p+=2;

  // packet identifier. used for checking UNSUBACK
  p[0] = (packet_id_counter >> 8) & 0xFF;
  p[1] = packet_id_counter & 0xFF;
  p+=2;

  // increment the packet id
  packet_id_counter++;

  p = stringprint(p, topic);

  len = p - packet;
  packet[1] = len-2; // don't include the 2 bytes of fixed header data
  DEBUG_PRINTLN(F("MQTT unsubscription packet:"));
  DEBUG_PRINTBUFFER(buffer, len);
  return len;

}

uint8_t ATT_MQTT::pingPacket(uint8_t *packet) {
  packet[0] = MQTT_CTRL_PINGREQ << 4;
  packet[1] = 0;
  DEBUG_PRINTLN(F("MQTT ping packet:"));
  DEBUG_PRINTBUFFER(buffer, 2);
  return 2;
}

uint8_t ATT_MQTT::pubackPacket(uint8_t *packet, uint16_t packetid) {
  packet[0] = MQTT_CTRL_PUBACK << 4;
  packet[1] = 2;
  packet[2] = packetid >> 8;
  packet[3] = packetid;
  DEBUG_PRINTLN(F("MQTT puback packet:"));
  DEBUG_PRINTBUFFER(buffer, 4);
  return 4;
}

uint8_t ATT_MQTT::disconnectPacket(uint8_t *packet) {
  packet[0] = MQTT_CTRL_DISCONNECT << 4;
  packet[1] = 0;
  DEBUG_PRINTLN(F("MQTT disconnect packet:"));
  DEBUG_PRINTBUFFER(buffer, 2);
  return 2;
}

