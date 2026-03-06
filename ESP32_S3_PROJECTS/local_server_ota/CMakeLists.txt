
idf_component_register(
    SRCS 
    "main.c"
    "../components/WIFI_Handler/WIFIHandler.c"   
    "../components/OTA/OTA.c"  
    "../components/SDCard_Handler/SDCardHandler.c"
    "../components/RTC_Handler/RTCHandler.c"
    "../components/I2C_Handler/I2CHandler.c"
    
    INCLUDE_DIRS       
	 "../components/WIFI_Handler"
	 "../components/OTA"
	 "../components/SDCard_Handler"
	 "../components/RTC_Handler"
	 "../components/I2C_Handler"
 )