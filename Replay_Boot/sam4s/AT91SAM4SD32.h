#define __SAM4SD32B__ // __SAM4SD32C__
#include "cmsis/sam4s/include/sam4s.h"

#define AT91C_BASE_WDTC   WDT
#define WDTC_WDMR     WDT_MR
#define AT91C_WDTC_WDDIS  WDT_MR_WDDIS

#define AT91C_BASE_RSTC   RSTC
#define RSTC_RMR      RSTC_MR
#define AT91C_RSTC_URSTEN RSTC_MR_URSTEN

#define AT91C_BASE_PIOA   PIOA
#define PIO_PPUER     PIO_PUER
#define PIO_PPUDR     PIO_PUDR

#define AT91C_BASE_PMC    PMC
#define PMC_PCER      PMC_PCER0

#define AT91C_ID_PIOA   ID_PIOA

#define AT91C_PIO_PA0   PIO_PA0

#define AT91C_PIO_PA3   PIO_PA3
#define AT91C_PIO_PA4   PIO_PA4
#define AT91C_PIO_PA6   PIO_PA6
#define AT91C_PIO_PA8   PIO_PA8
#define AT91C_PIO_PA9   PIO_PA9

#define AT91C_PIO_PA10    PIO_PA10
#define AT91C_PIO_PA11    PIO_PA11
#define AT91C_PIO_PA12    PIO_PA12
#define AT91C_PIO_PA13    PIO_PA13
#define AT91C_PIO_PA14    PIO_PA14
#define AT91C_PIO_PA15    PIO_PA15
#define AT91C_PIO_PA16    PIO_PA16
#define AT91C_PIO_PA17    PIO_PA17
#define AT91C_PIO_PA18    PIO_PA18
#define AT91C_PIO_PA19    PIO_PA19
#define AT91C_PIO_PA20    PIO_PA20
#define AT91C_PIO_PA21    PIO_PA21
#define AT91C_PIO_PA22    PIO_PA22
#define AT91C_PIO_PA23    PIO_PA23
#define AT91C_PIO_PA24    PIO_PA24
#define AT91C_PIO_PA25    PIO_PA25
#define AT91C_PIO_PA26    PIO_PA26
#define AT91C_PIO_PA27    PIO_PA27
#define AT91C_PIO_PA28    PIO_PA28
#define AT91C_PIO_PA29    PIO_PA29
#define AT91C_PIO_PA30    PIO_PA30
#define AT91C_PIO_PA31    PIO_PA31

#define AT91C_PA5_RXD0    PIO_PA5A_RXD0
#define AT91C_PA6_TXD0    PIO_PA6A_TXD0

#define AT91C_BASE_SPI    SPI
#define AT91C_SPI_SPIDIS  SPI_CR_SPIDIS
#define AT91C_ID_SPI    ID_SPI

#define AT91C_SPI_SWRST   SPI_CR_SWRST
#define AT91C_SPI_MSTR    SPI_MR_MSTR
#define AT91C_SPI_MODFDIS SPI_MR_MODFDIS
#define AT91C_SPI_CPOL    SPI_CSR_CPOL
#define AT91C_SPI_SPIEN   SPI_CR_SPIEN
#define AT91C_SPI_TDRE    SPI_SR_TDRE
#define AT91C_SPI_RDRF    SPI_SR_RDRF
#define AT91C_SPI_ENDTX   SPI_SR_ENDTX
#define AT91C_SPI_ENDRX   SPI_SR_ENDRX

#define AT91C_PA14_SPCK   PIO_PA14A_SPCK
#define AT91C_PA13_MOSI   PIO_PA13A_MOSI
#define AT91C_PA12_MISO   PIO_PA12A_MISO

#define AT91C_BASE_US0    USART0
#define AT91C_US_RXRDY    US_IER_RXRDY
#define AT91C_ID_US0    ID_USART0
#define AT91C_US_RSTRX    US_CR_RSTRX
#define AT91C_US_RSTTX    US_CR_RSTTX
#define AT91C_US_RXDIS    US_CR_RXDIS
#define AT91C_US_TXDIS    US_CR_TXDIS
#define AT91C_US_RXEN   US_CR_RXEN
#define AT91C_US_TXEN   US_CR_TXEN
#define AT91C_US_USMODE_NORMAL  US_MR_USART_MODE_NORMAL
#define AT91C_US_CLKS_CLOCK   US_MR_USCLKS_MCK
#define AT91C_US_CHRL_8_BITS  US_MR_CHRL_8_BIT
#define AT91C_US_PAR_NONE   US_MR_PAR_NO
#define AT91C_US_NBSTOP_1_BIT US_MR_NBSTOP_1_BIT
#define AT91C_US_CHMODE_NORMAL  US_MR_CHMODE_NORMAL

#define AT91C_PDC_TXTEN     PERIPH_PTCR_TXTEN
#define AT91C_PDC_TXTDIS    PERIPH_PTCR_TXTDIS
#define AT91C_PDC_RXTDIS    PERIPH_PTCR_RXTDIS
#define AT91C_PDC_RXTEN     PERIPH_PTCR_RXTEN

#define AT91C_BASE_SSC      SSC
#define AT91C_ID_SSC      ID_SSC
#define AT91C_SSC_CKO_DATA_TX SSC_RCMR_CKO_TRANSFER
#define AT91C_SSC_CKS_DIV   SSC_RCMR_CKS_MCK
#define AT91C_SSC_CKS_TK    SSC_RCMR_CKS_TK
#define AT91C_SSC_ENDTX     SSC_SR_ENDTX
#define AT91C_SSC_MSBF      SSC_TFMR_MSBF
#define AT91C_SSC_RXDIS     SSC_CR_RXDIS
#define AT91C_SSC_RXEN      SSC_CR_RXEN
#define AT91C_SSC_START_CONTINOUS SSC_RCMR_START_CONTINUOUS
#define AT91C_SSC_START_TX    SSC_RCMR_START_TRANSMIT
#define AT91C_SSC_SWRST     SSC_CR_SWRST
#define AT91C_SSC_TXDIS     SSC_CR_TXDIS
#define AT91C_SSC_TXEN      SSC_CR_TXEN
#define AT91C_SSC_TXRDY     SSC_SR_TXRDY
#define AT91C_SPI_TXEMPTY   SSC_SR_TXEMPTY

#define AT91C_BASE_TWI      TWI0
#define AT91C_ID_TWI      ID_TWI0
#define AT91C_TWI_MREAD     TWI_MMR_MREAD
#define AT91C_TWI_MSDIS     TWI_CR_MSDIS
#define AT91C_TWI_MSEN      TWI_CR_MSEN
#define AT91C_TWI_RXRDY     TWI_SR_RXRDY
#define AT91C_TWI_START     TWI_CR_START
#define AT91C_TWI_STOP      TWI_CR_STOP
#define AT91C_TWI_SWRST     TWI_CR_SWRST
#define AT91C_TWI_TXCOMP    TWI_SR_TXCOMP
#define AT91C_TWI_TXRDY     TWI_SR_TXRDY

//#define AT91C_BASE_AIC  ??
//#define AT91C_BASE_PITC ??
//#define AT91C_PITC_PICNT ??
//#define AT91C_PITC_PITEN ??
//#define AT91C_PITC_PIV ??
//#define PIO_ASR ?? PIO_ABCDSR ??
