#include "mbed.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
//--------------FXOS8700CQ-------------------------------------------
#include "fsl_port.h"

#include "fsl_gpio.h"

#define UINT14_MAX        16383

// FXOS8700CQ I2C address

#define FXOS8700CQ_SLAVE_ADDR0 (0x1E<<1) // with pins SA0=0, SA1=0

#define FXOS8700CQ_SLAVE_ADDR1 (0x1D<<1) // with pins SA0=1, SA1=0

#define FXOS8700CQ_SLAVE_ADDR2 (0x1C<<1) // with pins SA0=0, SA1=1

#define FXOS8700CQ_SLAVE_ADDR3 (0x1F<<1) // with pins SA0=1, SA1=1

// FXOS8700CQ internal register addresses

#define FXOS8700Q_STATUS 0x00

#define FXOS8700Q_OUT_X_MSB 0x01

#define FXOS8700Q_OUT_Y_MSB 0x03

#define FXOS8700Q_OUT_Z_MSB 0x05

#define FXOS8700Q_M_OUT_X_MSB 0x33

#define FXOS8700Q_M_OUT_Y_MSB 0x35

#define FXOS8700Q_M_OUT_Z_MSB 0x37

#define FXOS8700Q_WHOAMI 0x0D

#define FXOS8700Q_XYZ_DATA_CFG 0x0E

#define FXOS8700Q_CTRL_REG1 0x2A

#define FXOS8700Q_M_CTRL_REG1 0x5B

#define FXOS8700Q_M_CTRL_REG2 0x5C

#define FXOS8700Q_WHOAMI_VAL 0xC7

I2C i2c( PTD9,PTD8);

Serial pc(USBTX, USBRX);

int m_addr = FXOS8700CQ_SLAVE_ADDR1;

void FXOS8700CQ_readRegs(int addr, uint8_t * value, int len);

void FXOS8700CQ_writeRegs(uint8_t * value, int len);
//------------FXOS8700CQ-----------------------------------------------

// GLOBAL VARIABLES
WiFiInterface *wifi;
InterruptIn butt(SW2);
volatile bool publish = false;
volatile int message_num = 0;

void messageArrived(MQTT::MessageData& md) {
   MQTT::Message &message = md.message;
   char msg[300];
   sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
   printf(msg);
   wait_ms(1000);
   char payload[300];
   sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
   printf(payload);
}

void publish_message() {
   publish = true;
}

int main() {

   //---------------FXOS8700CQ------------------------------------------
   pc.baud(115200);
   uint8_t who_am_i, value[2], res[6];
   int16_t acc16;
   float t[3];

   // Enable the FXOS8700Q
   FXOS8700CQ_readRegs( FXOS8700Q_CTRL_REG1, &value[1], 1);
   value[1] |= 0x01;
   value[0] = FXOS8700Q_CTRL_REG1;
   FXOS8700CQ_writeRegs(value, 2);

   // Get the slave address
   FXOS8700CQ_readRegs(FXOS8700Q_WHOAMI, &who_am_i, 1);
   //----------------FXOS8700CQ-----------------------------------------

   wifi = WiFiInterface::get_default_instance();
   if (!wifi) {
      printf("ERROR: No WiFiInterface found.\r\n");
      return -1;
   }


   printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
   int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
   if (ret != 0) {
      printf("\nConnection error: %d\r\n", ret);
      return -1;
   }

   
   NetworkInterface* net = wifi;
   MQTTNetwork mqttNetwork(net);
   MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

   //TODO: revise host to your ip
   const char* host = "192.168.0.103";
   const char* topic = "Mbed";
   printf("Connecting to TCP network...\r\n");
   int rc = mqttNetwork.connect(host, 1883);
   if (rc != 0) {
      printf("Connection error.");
      return -1;
   }
   printf("Successfully connected!\r\n");

   MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
   data.MQTTVersion = 3;
   data.clientID.cstring = "Mbed";

   if ((rc = client.connect(data)) != 0){
      printf("Fail to connect MQTT\r\n");
   }
   if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
      printf("Fail to subscribe\r\n");
   }

   butt.rise(&publish_message);

   while (true) {
      if (/*publish*/true) {

            //---------------FXOS8700CQ--------------------------------------------
            FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);

            acc16 = (res[0] << 6) | (res[1] >> 2);

            if (acc16 > UINT14_MAX/2)

               acc16 -= UINT14_MAX;

            t[0] = ((float)acc16) / 4096.0f;


            acc16 = (res[2] << 6) | (res[3] >> 2);

            if (acc16 > UINT14_MAX/2)

               acc16 -= UINT14_MAX;

            t[1] = ((float)acc16) / 4096.0f;


            acc16 = (res[4] << 6) | (res[5] >> 2);

            if (acc16 > UINT14_MAX/2)

               acc16 -= UINT14_MAX;

            t[2] = ((float)acc16) / 4096.0f;

            wait(0.5);
            //------------------FXOS8700CQ-----------------------------------------

            message_num++;
            MQTT::Message message;
            char buff[100];
            sprintf(buff, "FXOS8700Q ACC: X=%1.4f(%x%x) Y=%1.4f(%x%x) Z=%1.4f(%x%x)\r\n",\

            t[0], res[0], res[1],\

            t[1], res[2], res[3],\

            t[2], res[4], res[5]);

            message.qos = MQTT::QOS0;
            message.retained = false;
            message.dup = false;
            message.payload = (void*) buff;
            message.payloadlen = strlen(buff) + 1;
            rc = client.publish(topic, message);

            printf("rc:  %d\r\n", rc);
            printf("Puslish message: %s\r\n", buff);
            client.yield(10);
            publish = false;
      }
   }
}

void FXOS8700CQ_readRegs(int addr, uint8_t * value, int len) {

   char t = addr;

   i2c.write(m_addr, &t, 1, true);

   i2c.read(m_addr, (char *)value, len);

}


void FXOS8700CQ_writeRegs(uint8_t * value, int len) {

   i2c.write(m_addr, (char *)value, len);

}
