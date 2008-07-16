#ifndef CONFIG_H_
#define CONFIG_H_

#define CONFIG_UART_DEBUG y

#define CONFIG_UART_BAUDRATE 38400

#define CONFIG_UART_BUF_SHIFT 6

#define CONFIG_DEADLOCK_ME_HARDER y

// SD stuff

#define CONFIG_SDHC_SUPPORT 1
#define CONFIG_SD_AUTO_RETRIES 10
#  define SD_SUPPLY_VOLTAGE (1L<<18)  /* 3.0V - 3.1V */
#endif /*CONFIG_H_*/
