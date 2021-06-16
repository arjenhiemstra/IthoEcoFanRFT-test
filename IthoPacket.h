/*
 * Author: Klusjesman, supersjimmie, modified and reworked by arjenhiemstra 
 */

#ifndef ITHOPACKET_H_
#define ITHOPACKET_H_

enum IthoCommand
{    
  IthoUnknown = 0,
    
  IthoJoin = 1,
  IthoLeave = 2,
        
  IthoStandby = 3,
  IthoLow = 4,
  IthoMedium = 5,
  IthoHigh = 6,
  IthoFull = 7,
  
  IthoTimer1 = 8,
  IthoTimer2 = 9,
  IthoTimer3 = 10,
  
  //duco c system remote
  DucoStandby = 11,
  DucoLow = 12,
  DucoMedium = 13,
  DucoHigh = 14
};

enum message_state {
  S_START,
  S_HEADER,
  S_ADDR0,
  S_ADDR1,
  S_ADDR2,
  S_PARAM0,
  S_PARAM1,
  S_OPCODE,
  S_LEN,
  S_PAYLOAD,
  S_CHECKSUM,
  S_TRAILER,
  S_COMPLETE,
  S_ERROR
};

#define F_MASK  0x03
#define F_RQ    0x00
#define F_I     0x01
#define F_W     0x02
#define F_RP    0x03

#define F_ADDR0  0x10
#define F_ADDR1  0x20
#define F_ADDR2  0x40

#define F_PARAM0 0x04
#define F_PARAM1 0x08
#define F_RSSI   0x80

// Only used for received fields
#define F_OPCODE 0x01
#define F_LEN    0x02


#define MAX_PAYLOAD 64
#define MAX_DECODED MAX_PAYLOAD+18


static char const * const MsgType[4] = { "RQ", " I"," W","RP" };

class IthoPacket
{
  public:
    IthoCommand command;
    
    int8_t state;
    
    //Message Fields
    //<HEADER> <addr0> <addr1> <addr2> <param0> <param1> <OPCODE> <LENGTH> <PAYLOAD> <CHECKSUM>  
    //<  1   > <  3  > <  3  > <  3  > <  1   > <  1   > <  2   > <  1   > <length > <   1    >  

    uint8_t header;
    uint8_t type;
    uint8_t deviceId0[3];
    uint8_t deviceId1[3];
    uint8_t deviceId2[3];
    uint8_t param0;
    uint8_t param1;
    uint8_t opcode[2];
    uint8_t len;

    uint8_t error;
    
    uint8_t payloadPos;
    uint8_t payload[MAX_PAYLOAD];

    uint8_t dataDecoded[MAX_DECODED];
    uint8_t dataDecodedChk[MAX_DECODED];
    uint8_t length;
    
    uint8_t deviceType;
    uint8_t deviceId[3];
    
    uint8_t counter;    //0-255, counter is increased on every remote button press
};


#endif /* ITHOPACKET_H_ */
