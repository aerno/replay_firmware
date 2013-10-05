#ifndef HARDWARE_H_INCLUDED
#define HARDWARE_H_INCLUDED
#include "board.h"


#define ACTLED_OFF AT91C_BASE_PIOA->PIO_SODR = PIN_ACT_LED;
#define ACTLED_ON  AT91C_BASE_PIOA->PIO_CODR = PIN_ACT_LED;

void Hardware_Init(void);
void IO_DriveLow_OD(uint32_t pin);
void IO_DriveHigh_OD(uint32_t pin);
uint8_t IO_Input_H(uint32_t pin);
uint8_t IO_Input_L(uint32_t pin);

//
void USART_Init(unsigned long baudrate);
void USART_Putc(void* p, char c);
uint8_t USART_Getc(void);
uint8_t USART_Peekc(void);
int16_t USART_CharAvail(void);
int16_t USART_GetBuf(const uint8_t buf[], int16_t len);

//
void SPI_Init(void);
unsigned char SPI(unsigned char outByte) __attribute__ ((section (".fastrun")));
void SPI_WriteBufferSingle(void *pBuffer, uint32_t length);
void SPI_ReadBufferSingle(void *pBuffer, uint32_t length);

void SPI_Wait4XferEnd(void) __attribute__ ((section (".fastrun")));
void SPI_EnableCard(void) __attribute__ ((section (".fastrun")));
void SPI_DisableCard(void) __attribute__ ((section (".fastrun")));
void SPI_EnableFpga(void);
void SPI_DisableFpga(void);
void SPI_EnableOsd(void);
void SPI_DisableOsd(void);
//
void SSC_Configure_Boot(void);
void SSC_EnableTxRx(void);
void SSC_DisableTxRx(void);
void SSC_Write(uint32_t frame);
void SSC_WriteBufferSingle(void *pBuffer, uint32_t length);
//
void TWI_Configure(void);
void TWI_Stop(void);
uint8_t TWI_ReadByte(void);
void TWI_WriteByte(uint8_t byte);
uint8_t TWI_ByteReceived(void);
uint8_t TWI_ByteSent(void);
uint8_t TWI_TransferComplete(void);
void TWI_StartRead(uint8_t DevAddr, uint8_t IntAddrSize, uint16_t IntAddr);
void TWI_StartWrite(uint8_t DevAddr, uint8_t IntAddrSize, uint16_t IntAddr, uint8_t WriteData);

//
void Timer_Init(void);
uint32_t Timer_Get(uint32_t offset);
uint32_t Timer_Check(uint32_t t);
void Timer_Wait(uint32_t time);
//
void DumpBuffer(const uint8_t *pBuffer, uint32_t size);

#endif
