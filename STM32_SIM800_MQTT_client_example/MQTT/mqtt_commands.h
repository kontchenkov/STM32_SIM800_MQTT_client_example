#include <inttypes.h>
#include <string.h>

uint8_t MQTT_CONNECT_string(char* buffer,
	uint16_t timeKeepAlive,
	char* ClientID);

/*uint8_t MQTT_PUBLISH_string(char* bufferOUT,
															 char* bufferIN, uint8_t bufferIN_LENGTH,
															 char* topicName,  uint8_t topicNameLENGTH);*/
															 
uint8_t MQTT_PUBLISH_string(
	char* bufferOUT,
	char* bufferIN,
	char* topicName);
	
//uint8_t MQTT_SUBSCRIBE_string(char* buffer, uint16_t packet_identifier);
	uint8_t MQTT_SUBSCRIBE_string(
	char* bufferOUT,
	char* topicName, 
	uint16_t packet_identifier);

uint8_t MQTT_PINGREQ_string(char* buffer);
