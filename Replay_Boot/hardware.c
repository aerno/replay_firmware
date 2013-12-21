// hardware abstraction layer for Sam7S

#include "hardware.h"
#include "messaging.h"

#define USB_USART 0

#if USB_USART==1
  #include"cdc_enumerate.h"
  extern struct _AT91S_CDC pCDC;
#endif

//
// General
//
void Hardware_Init(void)
{
  AT91C_BASE_WDTC->WDTC_WDMR = AT91C_WDTC_WDDIS; //disable watchdog
  AT91C_BASE_RSTC->RSTC_RMR = (0xA5<<24) | AT91C_RSTC_URSTEN;   //enable external user reset input

  // set up a safe default
  AT91C_BASE_PIOA->PIO_PER   = 0xFFFFFFFF; //grab all IOs
  AT91C_BASE_PIOA->PIO_PPUER = 0xFFFFFFFF; //enable all pullups (to be sure)
  AT91C_BASE_PIOA->PIO_ODR   = 0xFFFFFFFF; //disable all outputs
  AT91C_BASE_PIOA->PIO_CODR  = 0xFFFFFFFF; //set all outputs low

  //set outputs
  AT91C_BASE_PIOA->PIO_SODR = PIN_USB_PULLUP_L;
  AT91C_BASE_PIOA->PIO_SODR = PIN_FPGA_CTRL1 | PIN_FPGA_CTRL0 | PIN_CARD_CS_L;

  //output enable register
  AT91C_BASE_PIOA->PIO_OER = PIN_ACT_LED ;
  AT91C_BASE_PIOA->PIO_OER = PIN_USB_PULLUP_L;
  AT91C_BASE_PIOA->PIO_OER = PIN_FPGA_CTRL1 | PIN_FPGA_CTRL0 | PIN_CARD_CS_L;

  //pull-up disable register
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_ACT_LED ;
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_FPGA_INIT_L | PIN_FPGA_PROG_L; // open drains with external pull
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_USB_PULLUP_L;
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_FPGA_CTRL1 | PIN_FPGA_CTRL0;

  //external pulls, so disable internals
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_CARD_CLK | PIN_CARD_MOSI | PIN_CARD_MISO | PIN_CARD_CS_L ;

  // other pullups disabled when peripherals enabled
  // Enable peripheral clock in the PMC
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOA;

  // PIN_CONF_CCLK, PIN_CONF_DIN set up in SSC

  // CODER
  AT91C_BASE_PIOA->PIO_OER   = PIN_CODER_NTSC_H | PIN_CODER_CE;
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_CODER_NTSC_H | PIN_CODER_CE;

  AT91C_BASE_PIOA->PIO_CODR  = PIN_CODER_NTSC_H;
  AT91C_BASE_PIOA->PIO_CODR  = PIN_CODER_CE;

  AT91C_BASE_PIOA->PIO_OER   = PIN_DTXD_Y1 | PIN_DRXD_Y2;
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_DTXD_Y1 | PIN_DRXD_Y2;
  AT91C_BASE_PIOA->PIO_SODR  = PIN_DTXD_Y1 | PIN_DRXD_Y2;

  // FPGA RESET
  AT91C_BASE_PIOA->PIO_CODR  = PIN_FPGA_RST_L;
  AT91C_BASE_PIOA->PIO_OER   = PIN_FPGA_RST_L;
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_FPGA_RST_L;

}

void IO_DriveLow_OD(uint32_t pin)
{
  // we assume for OD pins the pin is already assigned to PIO and cleared
  AT91C_BASE_PIOA->PIO_OER = pin;
}

void IO_DriveHigh_OD(uint32_t pin)
{
  AT91C_BASE_PIOA->PIO_ODR = pin;
}

uint8_t IO_Input_H(uint32_t pin)  // returns true if pin high
{
  volatile uint32_t read;
  read = AT91C_BASE_PIOA->PIO_PDSR;
  if (!(read & pin))
    return FALSE;
  else
    return TRUE;
}

uint8_t IO_Input_L(uint32_t pin)  // returns true if pin low
{
  volatile uint32_t read;
  read = AT91C_BASE_PIOA->PIO_PDSR;
  if (!(read & pin))
    return TRUE;
  else
    return FALSE;
}

//
// IRQ
//
#define IRQ_MASK 0x00000080

static inline unsigned __get_cpsr(void)
{
  unsigned long retval;
  asm volatile (" mrs  %0, cpsr" : "=r" (retval) : /* no inputs */  );
  return retval;
}

static inline void __set_cpsr(unsigned val)
{
  asm volatile (" msr  cpsr, %0" : /* no outputs */ : "r" (val)  );
}

unsigned disableIRQ(void)
{
  unsigned _cpsr;

  _cpsr = __get_cpsr();
  __set_cpsr(_cpsr | IRQ_MASK);
  return _cpsr;
}

unsigned enableIRQ(void)
{
  unsigned _cpsr;

  _cpsr = __get_cpsr();
  __set_cpsr(_cpsr & ~IRQ_MASK);
  return _cpsr;
}

//
// USART
//

// for RX, we use a software ring buffer as we are interested in any character
// as soon as it is received.
volatile uint8_t USART_rxbuf[16];
volatile int16_t USART_rxptr, USART_rdptr;

// for TX, we use a hardware buffer triggered to be sent when full or on a CR.
volatile uint8_t USART_txbuf[128];
volatile int16_t USART_txptr, USART_wrptr;

void ISR_USART(void)
{
  uint32_t isr_status = AT91C_BASE_US0->US_CSR;

  // returns if no character
  if (!(isr_status & AT91C_US_RXRDY))
    return;

  USART_rxbuf[USART_rxptr] = AT91C_BASE_US0->US_RHR;
  USART_rxptr = (USART_rxptr+1) & 15;
}

void USART_Init(unsigned long baudrate)
{
#if USB_USART==1
  USB_Open();
#else
  void (*handler)(void) = &ISR_USART;

  // disable IRQ on ARM
  disableIRQ();

  // Configure PA5 and PA6 for USART0 use
  AT91C_BASE_PIOA->PIO_PDR = AT91C_PA5_RXD0 | AT91C_PA6_TXD0;
  // disable pullup on output
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_TXD;

  // Enable the peripheral clock in the PMC
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_US0;

  // Reset and disable receiver & transmitter
  AT91C_BASE_US0->US_CR = AT91C_US_RSTRX | AT91C_US_RSTTX | AT91C_US_RXDIS | AT91C_US_TXDIS;

  // Configure USART0 mode
  AT91C_BASE_US0->US_MR = AT91C_US_USMODE_NORMAL | AT91C_US_CLKS_CLOCK | AT91C_US_CHRL_8_BITS | AT91C_US_PAR_NONE | AT91C_US_NBSTOP_1_BIT | AT91C_US_CHMODE_NORMAL;

  // Configure the USART0 @115200 bauds
  AT91C_BASE_US0->US_BRGR = BOARD_MCK / 16 / baudrate;

  // Disable AIC interrupt first
  AT91C_BASE_AIC->AIC_IDCR = 1 << AT91C_ID_US0;
  
  // Configure AIC interrupt mode and handler
  AT91C_BASE_AIC->AIC_SMR[AT91C_ID_US0] = 0;
  AT91C_BASE_AIC->AIC_SVR[AT91C_ID_US0] = (int32_t)handler;

  // Clear AIC interrupt
  AT91C_BASE_AIC->AIC_ICCR = 1 << AT91C_ID_US0;

  // Enable AIC interrupt
  AT91C_BASE_AIC->AIC_IECR = 1 << AT91C_ID_US0;

  // Enable receiver & transmitter
  AT91C_BASE_US0->US_CR = AT91C_US_RXEN | AT91C_US_TXEN;

  // Enable USART RX interrupt
  AT91C_BASE_US0->US_IER = AT91C_US_RXRDY;

  // enable IRQ on ARM
  enableIRQ();
#endif
}

void USART_update(void)
{
  #if USB_USART==1
    if (pCDC.IsConfigured(&pCDC)) 
    {
      char data[16];
      uint16_t length;
      length = pCDC.Read(&pCDC,data, 16);
      if (length) {
        if ((length+USART_rxptr)>15) {
          memcpy((void *)&(USART_rxbuf[USART_rxptr]),data,16-USART_rxptr);
          memcpy((void *)USART_rxbuf,(void *)&(data[(16-USART_rxptr)]),length-(16-USART_rxptr));
        } else {
          memcpy((void *)&(USART_rxbuf[USART_rxptr]),data,length);
        }
        USART_rxptr = (USART_rxptr+length) & 15;
      }
    }
  #endif
}

void USART_Putc(void* p, char c)
{ 
  // if both PDC channels are blocked, we still have to wait --> bad luck...
  while (AT91C_BASE_US0->US_TNCR) ;
  // ok, thats the simplest solution - we could still continue in some cases,
  // but I am not sure if it is worth the effort...

  USART_txbuf[USART_wrptr]=c;
  USART_wrptr=(USART_wrptr+1)&127;
  if ((c=='\n')||(!USART_wrptr)) {
    #if USB_USART==1
      if (pCDC.IsConfigured(&pCDC)) 
      {
        //pCDC.Write(&pCDC, data, length);
        pCDC.Write(&pCDC, (const char *)&(USART_txbuf[USART_txptr]), (128+USART_wrptr-USART_txptr)&127);
        USART_txptr=USART_wrptr;
      }
    #else
      // flush the buffer now (end of line, end of buffer reached or buffer full)
      if ((AT91C_BASE_US0->US_TCR==0)&&(AT91C_BASE_US0->US_TNCR==0)) {
        AT91C_BASE_US0->US_TPR = (uint32_t)&(USART_txbuf[USART_txptr]);
        AT91C_BASE_US0->US_TCR = (128+USART_wrptr-USART_txptr)&127;
        AT91C_BASE_US0->US_PTCR = AT91C_PDC_TXTEN;
        USART_txptr=USART_wrptr;
      } else if (AT91C_BASE_US0->US_TNCR==0) {
        AT91C_BASE_US0->US_TNPR = (uint32_t)&(USART_txbuf[USART_txptr]);
        AT91C_BASE_US0->US_TNCR = (128+USART_wrptr-USART_txptr)&127;      
        USART_txptr=USART_wrptr;
      }
    #endif
  }
}


uint8_t USART_Getc(void)
{
  uint8_t val;
  if (USART_rxptr!=USART_rdptr) {
    val = USART_rxbuf[USART_rdptr];
    USART_rdptr = (USART_rdptr+1)&15;
  } else {
    val=0;
  }
  return val;
}

uint8_t USART_Peekc(void)
{
  uint8_t val;
  if (USART_rxptr!=USART_rdptr) {
    val = USART_rxbuf[USART_rdptr];
  } else {
    val=0;
  }
  return val;
}

inline int16_t USART_CharAvail(void)
{
  return (16+USART_rxptr-USART_rdptr)&15;
}

int16_t USART_GetBuf(const uint8_t buf[], int16_t len)
{
  uint16_t i;

  if (USART_CharAvail()<len) return 0; // not enough chars to compare

  for (i=0;i<len;++i) {
    if (USART_rxbuf[(USART_rdptr+i)&15]!=buf[i]) break;
  }
  if (i!=len) return 0; // no match

  // got it, remove chars from buffer
  USART_rdptr = (USART_rdptr+len)&15;
  return 1;
}

//
// SPI
//
void SPI_Init(void)
{
  // Enable the peripheral clock in the PMC
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_SPI;

  // Disable SPI interface
  AT91C_BASE_SPI->SPI_CR = AT91C_SPI_SPIDIS;

  // Execute a software reset of the SPI twice
  AT91C_BASE_SPI->SPI_CR = AT91C_SPI_SWRST;
  AT91C_BASE_SPI->SPI_CR = AT91C_SPI_SWRST;

  // SPI Mode Register
  AT91C_BASE_SPI->SPI_MR = AT91C_SPI_MSTR | AT91C_SPI_MODFDIS  | (0x0E<<16);

  // SPI cs register
  AT91C_BASE_SPI->SPI_CSR[0] = AT91C_SPI_CPOL | (48<<8) | (0x00<<16) | (0x01<<24);

  // Configure pins for SPI use
  AT91C_BASE_PIOA->PIO_PDR = AT91C_PA14_SPCK | AT91C_PA13_MOSI | AT91C_PA12_MISO;

  // Enable SPI interface
  AT91C_BASE_SPI->SPI_CR = AT91C_SPI_SPIEN;

  // DMA static
  AT91C_BASE_SPI->SPI_TNCR = 0;
  AT91C_BASE_SPI->SPI_RNCR = 0;

}

unsigned char SPI(unsigned char outByte)
{
  volatile uint32_t t = AT91C_BASE_SPI->SPI_RDR;  // warning, but is a must!
  while (!(AT91C_BASE_SPI->SPI_SR & AT91C_SPI_TDRE));
  AT91C_BASE_SPI->SPI_TDR = outByte;
  while (!(AT91C_BASE_SPI->SPI_SR & AT91C_SPI_RDRF));
  return((unsigned char)AT91C_BASE_SPI->SPI_RDR);
}

void SPI_WriteBufferSingle(void *pBuffer, uint32_t length)
{
  // single bank only, assume idle on entry
  AT91C_BASE_SPI->SPI_TPR  = (uint32_t) pBuffer;
  AT91C_BASE_SPI->SPI_TCR  = length;
  AT91C_BASE_SPI->SPI_PTCR = AT91C_PDC_TXTEN;

  uint32_t timeout = Timer_Get(100);      // 100 ms timeout
  while ((AT91C_BASE_SPI->SPI_SR & AT91C_SPI_ENDTX ) != AT91C_SPI_ENDTX) {
    if (Timer_Check(timeout)) {
      DEBUG(1,"SPI:WriteBufferSingle DMA Timeout!");
      AT91C_BASE_SPI->SPI_PTCR = AT91C_PDC_TXTDIS | AT91C_PDC_RXTDIS;
      break;
    }
  }
}

void SPI_ReadBufferSingle(void *pBuffer, uint32_t length)
{
  // we do not care what we send out (current buffer contents), the FPGA will ignore
  AT91C_BASE_SPI->SPI_TPR  = (uint32_t) pBuffer;
  AT91C_BASE_SPI->SPI_TCR  = length;
  AT91C_BASE_SPI->SPI_RPR  = (uint32_t) pBuffer;
  AT91C_BASE_SPI->SPI_RCR  = length;
  AT91C_BASE_SPI->SPI_PTCR = AT91C_PDC_TXTEN | AT91C_PDC_RXTEN;

  uint32_t timeout = Timer_Get(100);      // 100 ms timeout
  while ((AT91C_BASE_SPI->SPI_SR & (AT91C_SPI_ENDTX | AT91C_SPI_ENDRX)) != (AT91C_SPI_ENDTX | AT91C_SPI_ENDRX) ) {
    if (Timer_Check(timeout)) {
      WARNING("SPI:ReadBufferSingle DMA Timeout!");
      AT91C_BASE_SPI->SPI_PTCR = AT91C_PDC_TXTDIS | AT91C_PDC_RXTDIS;
      break;
    }
  }
}

void SPI_Wait4XferEnd(void)
{
  while (!(AT91C_BASE_SPI->SPI_SR & AT91C_SPI_TXEMPTY));
}

void SPI_EnableCard(void)
{
  AT91C_BASE_PIOA->PIO_CODR = PIN_CARD_CS_L;
}

void SPI_DisableCard(void)
{
  SPI_Wait4XferEnd();
  AT91C_BASE_PIOA->PIO_SODR = PIN_CARD_CS_L;
  SPI(0xFF);
  SPI_Wait4XferEnd();
}

void SPI_EnableFpga(void)
{
  AT91C_BASE_PIOA->PIO_CODR = PIN_FPGA_CTRL0;
}

void SPI_DisableFpga(void)
{
  SPI_Wait4XferEnd();
  AT91C_BASE_PIOA->PIO_SODR = PIN_FPGA_CTRL0;
}

void SPI_EnableOsd(void)
{
  AT91C_BASE_PIOA->PIO_CODR = PIN_FPGA_CTRL1;
}

void SPI_DisableOsd(void)
{
  SPI_Wait4XferEnd();
  AT91C_BASE_PIOA->PIO_SODR = PIN_FPGA_CTRL1;
}

// SSC
void SSC_Configure_Boot(void)
{
  // Enables CCLK and DIN as outputs. Note CCLK is wired to RK and we need to set
  // loop mode to get this to work.
  //
  // Enable SSC peripheral clock
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_SSC;

  // Reset, disable receiver & transmitter
  AT91C_BASE_SSC->SSC_CR   = AT91C_SSC_RXDIS | AT91C_SSC_TXDIS | AT91C_SSC_SWRST;
  AT91C_BASE_SSC->SSC_PTCR = AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS;

  // Configure clock frequency
  AT91C_BASE_SSC->SSC_CMR = BOARD_MCK / (2 * 10000000); // 10MHz
  // Configure TX

  // Enable pull ups
  AT91C_BASE_PIOA->PIO_PPUER = PIN_CONF_CCLK | PIN_CONF_DIN;

  // Configure TX
  AT91C_BASE_SSC->SSC_TCMR = AT91C_SSC_START_CONTINOUS |AT91C_SSC_CKO_DATA_TX | AT91C_SSC_CKS_DIV; // transmit clock mode
  AT91C_BASE_SSC->SSC_TFMR = 0x7 << 16 | AT91C_SSC_MSBF | 0x7; // transmit frame mode

  // Configure RX
  AT91C_BASE_SSC->SSC_RCMR = AT91C_SSC_START_TX | AT91C_SSC_CKO_DATA_TX | AT91C_SSC_CKS_TK;   // receive clock mode*/
  AT91C_BASE_SSC->SSC_RFMR = 0x7 << 16 | 1<<5 |  AT91C_SSC_MSBF | 0x7 ; // receive frame mode*/

  // Assign clock and data to SSC (peripheral A)
  AT91C_BASE_PIOA->PIO_ASR = PIN_CONF_CCLK | PIN_CONF_DIN;
  AT91C_BASE_PIOA->PIO_PDR = PIN_CONF_CCLK | PIN_CONF_DIN;

  // Disable PIO drive (we need to float later)
  AT91C_BASE_PIOA->PIO_ODR = PIN_CONF_CCLK | PIN_CONF_DIN;

  // IO_Init is part of the SSC group, external pullup
  AT91C_BASE_PIOA->PIO_ODR  = PIN_FPGA_INIT_L;
  AT91C_BASE_PIOA->PIO_CODR = PIN_FPGA_INIT_L;
  AT91C_BASE_PIOA->PIO_PER  = PIN_FPGA_INIT_L;

  // DMA static
  AT91C_BASE_SSC->SSC_TNCR = 0;
  AT91C_BASE_SSC->SSC_RNCR = 0;
}

void SSC_EnableTxRx(void)
{
  AT91C_BASE_SSC->SSC_CR = AT91C_SSC_TXEN |  AT91C_SSC_RXEN;

  // Assign clock and data to SSC (peripheral A)
  /*
  AT91C_BASE_PIOA->PIO_ASR = PIN_CONF_DIN;
  AT91C_BASE_PIOA->PIO_PDR = PIN_CONF_DIN;
  */
}

void SSC_DisableTxRx(void)
{
  AT91C_BASE_SSC->SSC_CR = AT91C_SSC_TXDIS | AT91C_SSC_RXDIS;

  /*
  // drive DIN high
  AT91C_BASE_PIOA->PIO_SODR = PIN_CONF_DIN;
  AT91C_BASE_PIOA->PIO_PER  = PIN_CONF_DIN;
  AT91C_BASE_PIOA->PIO_OER  = PIN_CONF_DIN;
  */
}

void SSC_Write(uint32_t frame)
{
  while ((AT91C_BASE_SSC->SSC_SR & AT91C_SSC_TXRDY) == 0);
  AT91C_BASE_SSC->SSC_THR = frame;
}

void SSC_WaitDMA(void)
{
  // wait for buffer to be sent
  while ((AT91C_BASE_SSC->SSC_SR & AT91C_SSC_ENDTX ) != AT91C_SSC_ENDTX) {}; // no timeout
}

void SSC_WriteBufferSingle(void *pBuffer, uint32_t length, uint32_t wait)
{
  // single bank only, assume idle on entry
  AT91C_BASE_SSC->SSC_TPR = (unsigned int) pBuffer;
  AT91C_BASE_SSC->SSC_TCR = length;
  AT91C_BASE_SSC->SSC_PTCR = AT91C_PDC_TXTEN;
  // wait for transmission only if requested
  if (wait) SSC_WaitDMA();
}

//
// TWI
//
void TWI_Configure(void)
{
  volatile uint32_t read;
  // PA4  SCL                     output  (TWI)
  // PA3  SDA                     output  (TWI)
  // Enable SSC peripheral clock
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_TWI;

  // disable pull ups
  AT91C_BASE_PIOA->PIO_PPUDR = PIN_SCL | PIN_SDA;
  // assign clock and data
  AT91C_BASE_PIOA->PIO_ASR = PIN_SCL | PIN_SDA;
  AT91C_BASE_PIOA->PIO_PDR = PIN_SCL | PIN_SDA;

  // Reset the TWI
  AT91C_BASE_TWI->TWI_CR = AT91C_TWI_SWRST;
  read = AT91C_BASE_TWI->TWI_RHR;
  // Disable
  AT91C_BASE_TWI->TWI_CR = AT91C_TWI_MSDIS;

  // Enable Master mode
  AT91C_BASE_TWI->TWI_CR = AT91C_TWI_MSEN;
  // Set clock. Note, max speed 100KHz
  AT91C_BASE_TWI->TWI_CWGR = 0;  // stop clock
  AT91C_BASE_TWI->TWI_CWGR = (0x2 << 16) | (0x77 << 8) | 0x77; // ~50KHz
}

void TWI_Stop(void)
{
  AT91C_BASE_TWI->TWI_CR = AT91C_TWI_STOP;
}

uint8_t TWI_ReadByte(void)
{
  return AT91C_BASE_TWI->TWI_RHR;
}

void TWI_WriteByte(uint8_t byte)
{
  AT91C_BASE_TWI->TWI_THR = byte;
}

uint8_t TWI_ByteReceived(void)
{
  return ((AT91C_BASE_TWI->TWI_SR & AT91C_TWI_RXRDY) == AT91C_TWI_RXRDY);
}

uint8_t TWI_ByteSent(void)
{
  return ((AT91C_BASE_TWI->TWI_SR & AT91C_TWI_TXRDY) == AT91C_TWI_TXRDY);
}

uint8_t TWI_TransferComplete(void)
{
  return ((AT91C_BASE_TWI->TWI_SR & AT91C_TWI_TXCOMP) == AT91C_TWI_TXCOMP);
}

void TWI_StartRead(uint8_t DevAddr, uint8_t IntAddrSize, uint16_t IntAddr)
{
  // send address. bit 0 is write_h (MMR)
  AT91C_BASE_TWI->TWI_MMR = (IntAddrSize << 8) | AT91C_TWI_MREAD | (DevAddr << 16);

  // Set internal address bytes
  AT91C_BASE_TWI->TWI_IADR = IntAddr;

  // Send START condition
  AT91C_BASE_TWI->TWI_CR = AT91C_TWI_START;
}

void TWI_StartWrite(uint8_t DevAddr, uint8_t IntAddrSize, uint16_t IntAddr, uint8_t WriteData)
{
  // send address. bit 0 is write_h (MMR)
  AT91C_BASE_TWI->TWI_MMR = (IntAddrSize << 8) | (DevAddr << 16);

  // Set internal address bytes
  AT91C_BASE_TWI->TWI_IADR = IntAddr;

  // Write first byte to send (does start bit)
  TWI_WriteByte(WriteData);
}

//
// Timer
//
void Timer_Init(void)
{
  AT91C_BASE_PITC->PITC_PIMR = AT91C_PITC_PITEN | ((BOARD_MCK/16/1000-1) & AT91C_PITC_PIV); //counting period 1ms
}

uint32_t Timer_Get(uint32_t offset)
{
  // note max timer is 4096mS with this setting
  uint32_t systimer = (AT91C_BASE_PITC->PITC_PIIR & AT91C_PITC_PICNT);
  systimer += offset<<20;
  return (systimer); //valid bits [31:20]
}

uint32_t Timer_Check(uint32_t time)
{
  uint32_t systimer = (AT91C_BASE_PITC->PITC_PIIR & AT91C_PITC_PICNT);
  /*calculate difference*/
  time -= systimer;
  /*check if <t> has passed*/
  return(time>(1<<31));
}

void Timer_Wait(uint32_t time)
{
  time =Timer_Get(time);
  while (!Timer_Check(time));
}

//
// Debug
//
void DumpBuffer(const uint8_t *pBuffer, uint32_t size)
{
    DEBUG(2,"DumpBuffer:");
    uint32_t i;
    for (i=0; i < size; i+=16) {
      DEBUG(2,"0x%08X: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", i,
      pBuffer[i+0], pBuffer[i+1],pBuffer[i+2], pBuffer[i+3],pBuffer[i+4], pBuffer[i+5],pBuffer[i+6], pBuffer[i+7],
      pBuffer[i+8], pBuffer[i+9],pBuffer[i+10], pBuffer[i+11],pBuffer[i+12], pBuffer[i+13],pBuffer[i+14], pBuffer[i+15]);
    }
}
