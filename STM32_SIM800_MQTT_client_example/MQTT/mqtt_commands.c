#include "mqtt_commands.h"
/*uint8_t MQTT_CONNECT_string(char* buffer)
{
	// Посылаем пакет CONNECT  
  // -------------------------------------------
  // Неизменяемый заголовок
	
  // 1 байт: 
  //7-4 Message type 0001 CONNECT
  // 3  DUP          0  (посылаем в первый раз)
  //2-1 QoS level    00 (без гарантии доставки)
  // 0  Retain       0  (сообщаем брокеру, что полученное сообщение сохранять не надо)
	buffer[0] = 0x10;
  
	// 2 байт - длина пакета
  buffer[1] = 18;
  // -------------------------------------------

  // ###########################################
  // Изменяемый заголовок
  // 1-2 байты - MSB:LSB длины названия протокола
  buffer[2] = 0;
	buffer[3] = 4;
    
  // 3-6 байты - название протокола
  buffer[4] = 'M';
	buffer[5] = 'Q';
	buffer[6] = 'T';
	buffer[7] = 'T';
	  
  // 7 байт - уровень (версия пртокола)
  buffer[8] = 0x04; // версия 3.1.1
  
  // 8 байт - Connect flag
  // -------------------------------------------
  // 7 User Name Flag 0 (не используем User Name)
  // 6 Password Flag  0 (не используем Password)
  // 5 Will Retain    0
  //4-3 Will QoS      0
  // 2 Will Flag      0
  // 1 Clean Session  1
  // 0 Reserved       0
  // -------------------------------------------
  buffer[9] = 0x02;
  // 9-10 байт - MSB:LSB Keep Alive (максимальное время пересылками контрольных пакетов)
  buffer[10] = 0x00;
  buffer[11] = 0x0B; // 11 c
  // 11-12 - длина имени клинета
  buffer[12] = 0x00;
  buffer[13] = 0x06;
  // 13-18 Paiload ClientID
	buffer[14] = 'S';
	buffer[15] = 'I';
	buffer[16] = 'M';
	buffer[17] = '8';
	buffer[18] = '0';
	buffer[19] = '0';		
//	// 19-20 - длина поля User Name	
//	buffer[20] = 0x00;
//  buffer[21] = 0x08;
//	// 	esihnfwu
//  // 21-28 Paiload User Name
//	buffer[22] = 'e';
//	buffer[23] = 's';
//	buffer[24] = 'i';
//	buffer[25] = 'h';
//	buffer[26] = 'n';
//	buffer[27] = 'f';
//	buffer[28] = 'w';
//	buffer[29] = 'u';
//	// 29-30 - длина поля Password	
//	buffer[30] = 0x00;
//  buffer[31] = 0x0b;
//	//-j0DZh1DQKbO
//	// 32-42 Paiload User Password
//	buffer[32] = '-';
//	buffer[33] = 'j';
//	buffer[34] = '0';
//	buffer[35] = 'D';
//	buffer[36] = 'Z';
//	buffer[37] = 'h';
//	buffer[38] = 'l';
//	buffer[39] = 'D';
//	buffer[40] = 'Q';
//	buffer[41] = 'K';
//	buffer[42] = 'b';
//	buffer[43] = 'O';
	buffer[20] = 0x1A;// CTRL+Z
	return 21;
}*/

/*
  char* buffer - адрес строки, в которую запишется готовый пакет
	uint16_t timeKeepAlive - время в секундах, в течение которого
	                         ожидаем пакета PINGRESP
	char* ClientID - имя клиента
*/
uint8_t MQTT_CONNECT_string(char* buffer, uint16_t timeKeepAlive, char* ClientID)
{
	// Посылаем пакет CONNECT  
	// -------------------------------------------
	// Неизменяемый заголовок

	// 1 байт: 
	//7-4 Message type 0001 CONNECT
	// 3  DUP          0  (посылаем в первый раз)
	//2-1 QoS level    00 (без гарантии доставки)
	// 0  Retain       0  (сообщаем брокеру, что полученное сообщение сохранять не надо)
	buffer[0] = 0x10;

	// 2 байт - длина пакета - заполняем в конце
	// -------------------------------------------

	// ###########################################
	// Изменяемый заголовок
	// 1-2 байты - MSB:LSB длины названия протокола
	buffer[2] = 0;
	buffer[3] = 4;

	// 3-6 байты - название протокола
	buffer[4] = 'M';
	buffer[5] = 'Q';
	buffer[6] = 'T';
	buffer[7] = 'T';

	// 7 байт - уровень (версия пртокола)
	buffer[8] = 0x04; // версия 3.1.1

	// 8 байт - Connect flag
	// -------------------------------------------
	// 7 User Name Flag 0 (не используем User Name)
	// 6 Password Flag  0 (не используем Password)
	// 5 Will Retain    0
	//4-3 Will QoS      0
	// 2 Will Flag      0
	// 1 Clean Session  1
	// 0 Reserved       0
	// -------------------------------------------
	buffer[9] = 0x02;
	// 9-10 байт - MSB:LSB Keep Alive (максимальное время пересылками контрольных пакетов)
	buffer[10] = timeKeepAlive >> 8;
	buffer[11] = (timeKeepAlive << 8) >> 8;
	// ############################################################
	// payload
	uint16_t lengthClientID = (uint16_t)strlen(ClientID);
	// 11-12 - длина имени клиента
	buffer[12] = lengthClientID >> 8;
	buffer[13] = (lengthClientID << 8) >> 8;
	strcpy(buffer + 14, ClientID);
	buffer[14 + lengthClientID] = 0x1A; // CTRL+Z
	uint8_t lengthBuffer = 14 + lengthClientID + 1;
	buffer[1] = lengthBuffer - 3;
	return lengthBuffer;
}

/*
	Функция, которая формирует строку, посылающую пакет PINGREQ
	buffer - строка, в которую помещаем результат
 */
uint8_t MQTT_PINGREQ_string(char* buffer)
{
	// Посылаем пакет PINGREQ 
  // -------------------------------------------
  buffer[0] = 0xC0;
  // 2 байт - длина пакета
  buffer[1] = 0;
 	buffer[2] = 0x1A;// CTRL+Z
	return 3;
}

/*
	Функция, которая формирует строку, посылающую пакет PUBLISH
	bufferOUT - строка, в которую помещаем результат
  bufferIN - строка, содержащая собственно сообщение, которое нужно передать
  topicName - строка, содержащая название темы
 */
uint8_t MQTT_PUBLISH_string(
	char* bufferOUT,
	char* bufferIN,
	char* topicName)
{
	uint16_t bufferIN_LENGTH = strlen(bufferIN);
	uint16_t topicNameLENGTH = strlen(topicName);
	// Посылаем пакет PUBLISH 
	// -------------------------------------------
	// Неизменяемый заголовок

	// 1 байт: 
	//7-4 Message type 0011 Publish
	// 3  DUP          0  (посылаем в первый раз)
	//2-1 QoS level    00 (без гарантии доставки)
	// 0  Retain       0  (сообщаем брокеру, что полученное сообщение сохранять не надо)
	bufferOUT[0] = 0x30;

	// 2 байт - длина пакета
	bufferOUT[1] = 2 + topicNameLENGTH + bufferIN_LENGTH;
	// -------------------------------------------

	// Изменяемый заголовок
	// 1-2 байты - MSB:LSB длины поля темы
	bufferOUT[2] = topicNameLENGTH >> 8;
	// заполняем поле темы
	bufferOUT[3] = (topicNameLENGTH << 8) >> 8;
	strcpy(bufferOUT + 4, topicName);
	// копируем передаваемое сообщение
	strcpy(bufferOUT + 4 + topicNameLENGTH, bufferIN);
	bufferOUT[4 + topicNameLENGTH + bufferIN_LENGTH] = 0x1A;// CTRL+Z
	return 5 + topicNameLENGTH + bufferIN_LENGTH;
}

/*
	Функция, которая формирует строку, посылающую пакет SUBSCRIBE
	bufferOUT - строка, в которую помещаем результат
  topicName - строка, содержащая название темы
  packet_identifier - идентификатор пакета
Каждый раз, когда клиент посылает новый пакет
SUBSCRIBE, этот пакет обязан содержать не задействованный на текущий момент идентификатор пакета
(Packet Identifier). Если клиент осуществляет повторную посылку некоторого пакета,
он обязан использовать тот же самый идентификатор пакета, который был при предыдущих
попытках пересылки. Идентификатор пакета становится доступным для повторного использования после 
получения клиентом подтверждения доставки пакета. В случае пакета SUBSCRIBE подтверждением служит пакет SUBACK.
 */
uint8_t MQTT_SUBSCRIBE_string(
	char* bufferOUT,
	char* topicName, 
	uint16_t packet_identifier)
{
	// Посылаем пакет SUBSCRIBE 
	// -------------------------------------------
	// Неизменяемый заголовок

	// 1 байт: 
	//7-4 Message type 1000 Subscribe
	//3-0 Reserved     0010
	bufferOUT[0] = 0x82;

	// 2 байт - длина пакета - заполняем в конце
	
	// -------------------------------------------

	// ###########################################
	// Изменяемый заголовок
	// 1-2 байты - MSB:LSB Packet idetifier
	bufferOUT[2] = packet_identifier >> 8;
	bufferOUT[3] = (packet_identifier << 8) >> 8;
	// 3-4 байты - длина поля Paiload
	uint16_t topicNameLENGTH = strlen(topicName);
	bufferOUT[4] = topicNameLENGTH >> 8;
	bufferOUT[5] = (topicNameLENGTH << 8)>>8;
	// Payload
	// название очереди	
	strcpy(bufferOUT + 6, topicName);
	// Requested QoS
	bufferOUT[topicNameLENGTH + 6] = 0x00;
	bufferOUT[topicNameLENGTH + 7] = 0x1A;// CTRL+Z
	bufferOUT[1] = topicNameLENGTH + 5;
	return topicNameLENGTH + 8;
}


uint8_t MQTT_DISCONNECT_string(char* buffer)
{
	// Посылаем пакет DISCONNECT 
  // -------------------------------------------
  // Неизменяемый заголовок
	
  // 1 байт: 
  //7-4 Message type 1110 DISCONNECT
  //3-0 Reserved     0010
	buffer[0] = 0xE0;
  
	// 2 байт - длина пакета
  buffer[1] = 0x00;
  // -------------------------------------------
	buffer[2] = 0x1A;// CTRL+Z
	return 3;
}
