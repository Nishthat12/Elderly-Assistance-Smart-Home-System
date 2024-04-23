#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <ESP_Mail_Client.h>
#include <WiFiClient.h>

#define WIFI_SSID "nishu"
#define WIFI_PASSWORD "invisiblestring"

/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

/* The sign in credentials */
#define AUTHOR_EMAIL "espnishtha@gmail.com"
#define AUTHOR_PASSWORD "vxig dqks gdff jtwo"

/* Recipient's email*/
#define RECIPIENT_EMAIL "nishtha.taktewale12@gmail.com"

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

#define DOOR_SENSOR_PIN  19  // ESP32 pin GPIO19 connected to door sensor's pin

int doorState;
int doorState1=0;
unsigned long doorOpenTime = 0;
const unsigned long doorOpenThreshold = 10000; // Threshold for door open time in milliseconds (e.g., 60 seconds)

void setup() {
  Serial.begin(9600);                     // initialize serial
  while (!Serial)
    delay(10);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP); // set ESP32 pin to input pull-up mode

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Set callback for SMTP events
  smtp.callback(smtpCallback);
}

void loop() {
  doorState = digitalRead(DOOR_SENSOR_PIN); // read state

  if (doorState == HIGH) {
    // Door is open
    if (doorOpenTime == 0) {
      // Start timing if door was just opened
      doorOpenTime = millis();
    } else {
      // Check if door has been open for too long
      unsigned long elapsedTime = millis() - doorOpenTime;
      if (elapsedTime >= doorOpenThreshold && doorState1==0) {
        Serial.println("The door has been open for too long!");
        /*  Set the network reconnection option */
        MailClient.networkReconnect(true);
        /** Enable the debug via Serial port
          * 0 for no debugging
          * 1 for basic level debugging
          *
          * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
          */x
        smtp.debug(1);

        /* Set the callback function to get the sending results */
        smtp.callback(smtpCallback);

        /* Declare the Session_Config for user defined session credentials */
        Session_Config config;

        /* Set the session config */
        config.server.host_name = SMTP_HOST;
        config.server.port = SMTP_PORT;
        config.login.email = AUTHOR_EMAIL;
        config.login.password = AUTHOR_PASSWORD;
        config.login.user_domain = "";

        /*
        Set the NTP config time
        For times east of the Prime Meridian use 0-12
        For times west of the Prime Meridian add 12 to the offset.
        Ex. American/Denver GMT would be -6. 6 + 12 = 18
        See https://en.wikipedia.org/wiki/Time_zone for a list of the GMT/UTC timezone offsets
        */
        config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
        config.time.gmt_offset = 5;
        config.time.day_light_offset = 0;

        /* Declare the message class */
        SMTP_Message message;

        /* Set the message headers */
        message.sender.name = F("ESP");
        message.sender.email = AUTHOR_EMAIL;
        message.subject = F("Urgent: Front Door has been open for too long");
        message.addRecipient(F("Caregiver"), RECIPIENT_EMAIL);
          
        /*Send HTML message*/
        /*String htmlMsg = "<div style=\"color:#2f4468;\"><h1>Hello World!</h1><p>- Sent from ESP board</p></div>";
        message.html.content = htmlMsg.c_str();
        message.html.content = htmlMsg.c_str();
        message.text.charSet = "us-ascii";
        message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;*/

        
        //Send raw text message
        String textMsg = "Dear Caregiver, The front door has been left open for too long. Please check on it. Thank You.";
        message.text.content = textMsg.c_str();
        message.text.charSet = "us-ascii";
        message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
        
        message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
        message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;


        /* Connect to the server */
        if (!smtp.connect(&config)){
          ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
          return;
        }

        if (!smtp.isLoggedIn()){
          Serial.println("\nNot yet logged in.");
        }
        else{
          if (smtp.isAuthenticated())
            Serial.println("\nSuccessfully logged in.");
          else
            Serial.println("\nConnected with no Auth.");
        }

        /* Start sending Email and close the session */
        if (!MailClient.sendMail(&smtp, &message)){
          ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        }

        doorState1 = 1;
      }
    }
  } else {
    // Door is closed
    doorOpenTime = 0; // Reset the timer when door is closed
    doorState1 = 0;
  }

  delay(1000); // Adjust delay as needed
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
      
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}
