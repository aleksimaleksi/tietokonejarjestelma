/* C Standard library */
#include <stdio.h>
#include <stdbool.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/i2c/I2CCC26XX.h>




/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"

#include "sensors/buzzer.h" //buzzer


/* Task */
#define STACKSIZE 1024
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// JTKJ: Teht�v� 3. Tilakoneen esittely
// JTKJ: Exercise 3. Definition of the state machine
enum state { READ_DATA, PRINT_DATA, DATA_READY};
enum state programState = READ_DATA;

enum stepstate { ON, OFF};
enum stepstate programStepstate = ON;

// JTKJ: Teht�v� 3. Valoisuuden globaali muuttuja
// JTKJ: Exercise 3. Global variable for ambient light
double ambientLight = -1000.0;
int debuglength = 31;
char debug_str[31];


char morse(float *x, float *y, float *z);
void buzzer_out(char input);
char erase();
int mode = 0;

char message = 'X';
char input;

int count = 0;

//mpu data values
float accx, accy, accz, posx, posy, posz;

// JTKJ: Teht�v� 1. Lis�� painonappien RTOS-muuttujat ja alustus
// JTKJ: Exercise 1. Add pins RTOS-variables and configuration here
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};


char erase() {
    return message = 'X';
}

char morse(float *x, float *y, float *z) {
    char operation;

    if (*y > 1.6) {operation = '-';}
    if (*y < -1.6) {operation = '.';}
    if (*z < -2.9) {operation = 's';}

    switch(operation)
    {
    case '-':
        programState = DATA_READY;
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(5000);
        Task_sleep(400000 / Clock_tickPeriod);
        buzzerClose();
        return message = '-';
    case '.':
        programState = DATA_READY;
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(5000);
        Task_sleep(200000 / Clock_tickPeriod);
        buzzerClose();
        return message = '.';
    case 's':
        programState = DATA_READY;
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(6000);
        Task_sleep(100000 / Clock_tickPeriod);
        buzzerClose();
        return message = ' ';
    }
    }

void buzzer_out(char input) {

    switch(input) {

    case '-':
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(4000);
        Task_sleep(300000 / Clock_tickPeriod);
        buzzerClose();
        Task_sleep(100000 / Clock_tickPeriod);

    case '.':
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(4000);
        Task_sleep(100000 / Clock_tickPeriod);
        buzzerClose();
        Task_sleep(100000 / Clock_tickPeriod);


    case ' ':

        Task_sleep(300000 / Clock_tickPeriod);
    }
}



void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    // JTKJ: Teht�v� 1. Vilkuta jompaa kumpaa ledi�
    // JTKJ: Exercise 1. Blink either led of the device
    // Vaihdetaan led-pinnin tilaa negaatiolla
    uint_t pinValue = PIN_getOutputValue( Board_LED1 );
    pinValue = !pinValue;
    PIN_setOutputValue( ledHandle, Board_LED1, pinValue );

    if (programStepstate == OFF) {
        programStepstate =ON ;
    } else {
        programStepstate = OFF;
    }
 }


const int bufferamount = 30;
uint8_t uartBuffer[bufferamount]; // Vastaanottopuskuri

// Käsittelijäfunktio
static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {

   // Nyt meillä on siis haluttu määrä merkkejä käytettävissä
   // rxBuf-taulukossa, pituus len, jota voimme käsitellä halutusti
   // Tässä ne annetaan argumentiksi toiselle funktiolle (esimerkin vuoksi)
//   tehdaan_jotain_nopeasti(rxBuf,len);


   // Käsittelijän viimeisenä asiana siirrytään odottamaan uutta keskeytystä..
   UART_read(handle, rxBuf, 1);

}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {

    //?
    UART_Handle uart;
    UART_Params uartParams;
    //?
    // JTKJ: Teht�v� 4. Lis�� UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = &uartFxn;
    uartParams.baudRate = 9600; // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1



    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
          System_abort("Error opening the UART");
    }
    UART_read(uart, uartBuffer, 1);

    while (1) {

        if (programState == PRINT_DATA) {
            //sprintf(debug_str,"x:%4.2f, y:%4.2f, z:%4.2f\n\r",accx,accy,accz);
            sprintf(debug_str,"%c\n\r\0",message);


            System_printf(debug_str);
            UART_write(uart, debug_str, debuglength);
            erase();
            programState = READ_DATA;
        }

        // Once per second, you can modify this
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {


    //I2C_Handle      i2c;
    //I2C_Params      i2cParams;
    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
       i2cMPUParams.bitRate = I2C_400kHz;
       // Note the different configuration below
       i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

       PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

       // MPU open i2c
         i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
         if (i2cMPU == NULL) {
             System_abort("Error Initializing I2CMPU\n");
         }

    Task_sleep(100000 / Clock_tickPeriod);
    //mpu9250_setup(&i2c);

    // MPU setup and calibration
       System_printf("MPU9250: Setup and calibration...\n");
       System_flush();

       mpu9250_setup(&i2cMPU);

       System_printf("MPU9250: Setup and calibration OK\n");
       System_flush();



    while (1) {
        if (programStepstate == OFF) {
            int i = 0;
            for (;i < bufferamount; i++ ) {
                buzzer_out(uartBuffer[i]);
            }
        }

        if ((programState == READ_DATA || programState == DATA_READY) && programStepstate == ON) {
            mpu9250_get_data(&i2cMPU , &accx , &accy , &accz , &posx , &posy , &posz);

            morse(&accx , &accy , &accz);

            if (message == ' ' || message == '-' || message == '.') {
                programState = PRINT_DATA;
            }

        }

        // Once per second, you can modify this
        Task_sleep(10000 / Clock_tickPeriod);

}
}



 main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    Board_initI2C();

    
    // JTKJ: Teht�v� 2. Ota i2c-v�yl� k�ytt��n ohjelmassa
    // JTKJ: Exercise 2. Initialize i2c bus
    // JTKJ: Teht�v� 4. Ota UART k�ytt��n ohjelmassa
    // JTKJ: Exercise 4. Initialize UART


    Board_initUART();


    // JTKJ: Teht�v� 1. Ota painonappi ja ledi ohjelman k�ytt��n
    //       Muista rekister�id� keskeytyksen k�sittelij� painonapille

    // Ledi käyttöön ohjelmassa
    ledHandle = PIN_open( &ledState, ledConfig );
    if(!ledHandle) {
       System_abort("Error initializing LED pin\n");
    }

    // Painonappi käyttöön ohjelmassa
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pin\n");
    }

    // Painonapille keskeytyksen käsittellijä
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }

    // JTKJ: Exercise 1. Open the button and led pins
    //       Remember to register the above interrupt handler for button


    // Open MPU power pin
      hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
      if (hMpuPin == NULL) {
          System_abort("Pin open failed!");
      }

      programState = READ_DATA;
      /* Task */
      Task_Params_init(&sensorTaskParams);
      sensorTaskParams.stackSize = STACKSIZE;
      sensorTaskParams.stack = &sensorTaskStack;
      sensorTaskParams.priority=2;
      sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
      if (sensorTaskHandle == NULL) {
          System_abort("Task create failed!");
      }

      Task_Params_init(&uartTaskParams);
      uartTaskParams.stackSize = STACKSIZE;
      uartTaskParams.stack = &uartTaskStack;
      uartTaskParams.priority=2;
      uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
      if (uartTaskHandle == NULL) {
          System_abort("Task create failed!");
      }



    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

