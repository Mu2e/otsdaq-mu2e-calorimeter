
#ifndef _MZB_H_
#define _MZB_H_

#define GNU_COMP


#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include <iomanip>

#define CT_MAJOR                2       /* MU2E-SPI major release version        */
#define CT_MINOR                2       /* MU2E-SPI minor release version        */
#define APP_MSG_BUFLEN         128
#define CT_MAXP                 9       /* Number of allowed parameters    */

#define MCADC_MAX_CHANNELS       (8)    /* Maximum number of channels for multichannel ADC */

#define FEE_NUM			    	      ( 20)
#define FEE_ADC_CHANNELS 	      (  8)
#define MZB_LADC_CHANNELS				(  5)

#define MU2E_HV_MAX_V           (250)

#define APP_MAGICID (0x135BU)


// Console assigned to UART0
#define CONSOLE_baudrate      115200
#define CONSOLE_IRQHandler    UART0_IRQHandler
#define CONSOLE_UART          LPC_UART0
#define CONSOLE_IRQn          UART0_IRQn

#define MU2E_FIRMWARE_DESC        "(C) ENEA-INFN 2012-2024 - MU2E-ADC board (SiPM SPI version)"


#ifdef GNU_COMP
  typedef enum {
#else
  typedef enum MZB_OSCMDCODE_t {
#endif
	SYNTAX_ERROR  = 0,//
	ADCONVERT,      // AD128S start conversion
	ADCFG,          // Configure ADC operational mode
	ADINIT,         // Initialize ADC
	ADSTS,          // Show ADC status
	APPCONFIG,      // Show the MEZZANINE configuration
	AUTOCALIBRATE,	// Calibrate ADC on active channels
	AUTOCONFIGURE,	// Search APD sensor on I2C bus
  AUTOSTART,      // Set/clear AUTOSTART flag
	AUTOWDG,        // Watchdog flag, if set HW WDG is activated
	BUSYSRC,        // 1=Force BUSY line from RIT, 0=Read from DIRAC
	BUSYTMO,        // Timeout for BUSY line
	CALADCP1P2,     // ADC two point linear calibration
	CALLPCP1P2,     // Calibrate internal AD (LPC.AD)
	CALDACP1P2,     // DAC two point linear calibration
	CALCARD,        // Set all data calibration for APD cards
	CALDATA,        // Set/get calibration data
	CALSOURCE,      // Set/get the calibration data source 0=FLASH_ROM, 1=RAM
	CALTEMP,        // Calibrate PT1000 on LPC.AD4
	CARDSTS,        // Report the status of all the enabled FEE cards
	COMBPS,         // Set serial port baud rate
	DACSET,         // Set DAC121S DAC output in physical values
	DACSTS,         // Status of DAC devices
	DEBUGMODE,      // Show debug mode status
	DELTAT_ms,      // Set ADC sampling time
	EMACENABLE,	    // Enable DP83848C PHY device
	EMACINIT,       // Initialize DP83848C PHY device
	EMACSTS,        // Dump DP83848 PHY registers
	EMACWRITE,      // Write DP83848 PHY registers
	ERRORS,         // Report and clear error list
	ERRSTS,         // Report a detailed status of the errors
	FRFLAG,         // Set/clear Fault Recovery Flag
	GAIN,           // Set gain of the APD pre-amp
	GPINPUT,        // Report GP input status
	GPOUTPUT,       // Set level on output line #n
	HELLO,          // Hello message
	HELP,           // Display this help screen
	HVOO_AUTOSET,   // Autoset the HV on/off switch after a hot start
	HVONOFF,        // Enable/disable HV
	I2CRESET,       // Reset I2C bus bit banging the SCL line
	I2CSTS,         // Show I2C interface status
	I2CENUMERATE,   // Enumerate I2C devices
	IP4ADR,         // Set IP address
	IRQ,            // Reserved
	IRQENABLE,      // Enable/Disable IRQ on GPI
  MSGLEVEL,       // Application message level
  MPXREG,         // Read I2C I/O expander registers
	MZBDUMP,        // Dump the MEZZANINE register content
	LOGIN ,         // Start session user:ROOT
	LOGOUT,         // Terminate session
	PROCESSES,      // Show active processes
	POST,           // Power on self test
	RESTART,        // MCU reset
	RESTORE,        // Restore settings
	RIT,            // Set RIT pace rate
	ROOT,           // RESERVED
  SBLACTIVATE,    // Activate SBL to upgrade firmware by I2C
	SEU,            // Force a number of Single Bit Upset
	SHOW_TRANSIENT, // Show real time ADC data
	SKIPERROR,      // Skip error messages
	SILENT,         // Enable/disable diagnostic messsages
	SLEWRATE,       // Set HV bias slew rate in units V/s
	STATUS_REG,     // Get status register
	TEST,           // Reserved
	UPDATE,         // Save current parameters
	VERSION,        // Show firmware version
	HWWDG,          // Set and show the HW Watchdog status
	SWWDG,          // Set and show the SW Watchdog status
	DIRACWDG,       // Set and show the DIRAC Watchdog status
	DIRACRST,       // Pulse the DIRAC RESET REQUEST line (0.1s)
	WATCHDOG,       // Show status for the all implemented watchdog
  RESERVED        //
} MZB_OSCMDCODE_t;


typedef struct {
    MZB_OSCMDCODE_t code;
    const char* str;
} MZB_OSCMDCODE_MAP_t;


MZB_OSCMDCODE_MAP_t code_map[] = {
    {SYNTAX_ERROR, "SYNTAX_ERROR"},
    {ADCONVERT, "ADCONVERT"},
    {ADCFG, "ADCFG"},
    {ADINIT, "ADINIT"},
    {ADSTS, "ADSTS"},
    {APPCONFIG, "APPCONFIG"},
    {AUTOCALIBRATE, "AUTOCALIBRATE"},
    {AUTOCONFIGURE, "AUTOCONFIGURE"},
    {AUTOSTART, "AUTOSTART"},
    {AUTOWDG, "AUTOWDG"},
    {BUSYSRC, "BUSYSRC"},
    {BUSYTMO, "BUSYTMO"},
    {CALADCP1P2, "CALADCP1P2"},
    {CALLPCP1P2, "CALLPCP1P2"},
    {CALDACP1P2, "CALDACP1P2"},
    {CALCARD, "CALCARD"},
    {CALDATA, "CALDATA"},
    {CALSOURCE, "CALSOURCE"},
    {CALTEMP, "CALTEMP"},
    {CARDSTS, "CARDSTS"},
    {COMBPS, "COMBPS"},
    {DACSET, "DACSET"},
    {DACSTS, "DACSTS"},
    {DEBUGMODE, "DEBUGMODE"},
    {DELTAT_ms, "DELTAT_ms"},
    {EMACENABLE, "EMACENABLE"},
    {EMACINIT, "EMACINIT"},
    {EMACSTS, "EMACSTS"},
    {EMACWRITE, "EMACWRITE"},
    {ERRORS, "ERRORS"},
    {ERRSTS, "ERRSTS"},
    {FRFLAG, "FRFLAG"},
    {GAIN, "GAIN"},
    {GPINPUT, "GPINPUT"},
    {GPOUTPUT, "GPOUTPUT"},
    {HELLO, "HELLO"},
    {HELP, "HELP"},
    {HVOO_AUTOSET, "HVOO_AUTOSET"},
    {HVONOFF, "HVONOFF"},
    {I2CRESET, "I2CRESET"},
    {I2CSTS, "I2CSTS"},
    {I2CENUMERATE, "I2CENUMERATE"},
    {IP4ADR, "IP4ADR"},
    {IRQ, "IRQ"},
    {IRQENABLE, "IRQENABLE"},
    {MSGLEVEL, "MSGLEVEL"},
    {MPXREG, "MPXREG"},
    {MZBDUMP, "MZBDUMP"},
    {LOGIN, "LOGIN"},
    {LOGOUT, "LOGOUT"},
    {PROCESSES, "PROCESSES"},
    {POST, "POST"},
    {RESTART, "RESTART"},
    {RESTORE, "RESTORE"},
    {RIT, "RIT"},
    {ROOT, "ROOT"},
    {SBLACTIVATE, "SBLACTIVATE"},
    {SEU, "SEU"},
    {SHOW_TRANSIENT, "SHOW_TRANSIENT"},
    {SKIPERROR, "SKIPERROR"},
    {SILENT, "SILENT"},
    {SLEWRATE, "SLEWRATE"},
    {STATUS_REG, "STATUS_REG"},
    {TEST, "TEST"},
    {UPDATE, "UPDATE"},
    {VERSION, "VERSION"},
    {HWWDG, "HWWDG"},
    {SWWDG, "SWWDG"},
    {DIRACWDG, "DIRACWDG"},
    {DIRACRST, "DIRACRST"},
    {WATCHDOG, "WATCHDOG"},
    {RESERVED, "RESERVED"}
};


#ifdef GNU_COMP
  typedef struct __attribute__((__packed__)) {
#else
  typedef __packed struct EE_DATABUF_tag {
#endif
    uint16_t ridx;
    uint16_t reserved1[3];

    uint16_t updateCounter_tag;           // 'RR'
    uint16_t reserved2;
    uint32_t updateCounter;
    uint32_t stsreg;                      // Status register
    uint32_t errreg;                      // Error register
    uint32_t errcnt;
    uint32_t ch_ensts;
    uint32_t gpi;
    uint32_t gpo;
    uint16_t ladc[MZB_LADC_CHANNELS];     // LPC internal ADC
    uint16_t reserved3[8- MZB_LADC_CHANNELS];
    uint16_t lastError;
    uint8_t  slewRate;
    uint8_t  hvmfsm;                      // HV modulator state
    uint16_t auxadc[MCADC_MAX_CHANNELS];  // Auxiliary ADC
    uint8_t  flipCount[MCADC_MAX_CHANNELS];// Flip status of AIO
    uint32_t RR_checksum;                 // RR section checksum

    uint16_t apdBiasV_tag;                // 'VL'
    uint16_t reserved4;
    uint16_t apdBiasV[FEE_NUM];
    uint32_t VL_checksum;                 // VL section checksum

    uint16_t apdBiasA_tag;                // 'CA'
    uint16_t reserved5;
    uint16_t apdBiasA[FEE_NUM];
    uint32_t CA_checksum;                 // CA section checksum

    uint16_t apdTemp_tag;                 // 'TC'
    uint16_t reserved6;
    uint16_t apdTemp[FEE_NUM];
    uint32_t TC_checksum;                 // TC section checksum

    uint16_t apdPwrs_tag;                 // 'PS'
    uint16_t reserved7;
    uint16_t apdPwrs [FEE_NUM];
    uint16_t apdPwrsE[FEE_NUM];
    uint16_t apdPwrsF[FEE_NUM];
    uint16_t apdPwrsG[FEE_NUM];
    uint16_t apdPwrsH[FEE_NUM];
    uint32_t PS_checksum;                 // PS section checksum

    uint16_t apdError_tag;                // 'ER'
    uint16_t reserved8;
    uint16_t ioe_errcnt,  ioe_lastError;
    uint16_t spi_errcnt,  spi_lastError;
    uint16_t i2cm_errcnt, i2cm_lastError;
    uint16_t i2cs_errcnt, i2cs_lastError;
    uint16_t ladc_errcnt, ladc_lastError;
    #ifdef GNU_COMP
      struct __attribute__((__packed__)) {
    #else
      __packed struct {
    #endif
        uint16_t errcnt;
        uint16_t lastErr;
        uint16_t adc_errcnt;
        uint16_t adc_lastErr;
        uint16_t dac_errcnt;
        uint16_t dac_lastErr;
    } apd_err[FEE_NUM];
    uint16_t reserved8_1[2];
    uint32_t ER_checksum;                 // ER section checksum

    uint16_t apdCalib_tag;                // 'CL'
    uint16_t reserved9[3];
    #ifdef GNU_COMP
      struct __attribute__((__packed__)) {
    #else
      __packed struct {
    #endif
        double slope;
        double offset;
    } edac_cal[FEE_NUM];
    #ifdef GNU_COMP
      struct __attribute__((__packed__)) {
    #else
      __packed struct {
    #endif
        double slope;
        double offset;
    } eads0_cal[FEE_NUM];
    #ifdef GNU_COMP
      struct __attribute__((__packed__)) {
    #else
      __packed struct {
    #endif
        double slope;
        double offset;
    } eads1_cal[FEE_NUM];
    #ifdef GNU_COMP
      struct __attribute__((__packed__)) {
    #else
      __packed struct {
    #endif
        double slope;
        double offset;
    } eads2_cal[FEE_NUM];
    uint32_t CL_checksum;                 // CL section checksum

    uint16_t cmdReqIssued;
    uint16_t reserved11;

    uint16_t ch_enstsReq_tag;                // 'EN'
    uint16_t reserved12;
    uint32_t ch_enstsReq;
    uint32_t reserved12_1;
    uint32_t ensts_chksum;

    uint16_t commandReq_tag;                 // 'W1'
    uint16_t reserved13;
    uint16_t commandReq;
    uint16_t reserved14[3];
    uint32_t cmdReq_chksum;

    uint16_t biasVreq_tag;                  // 'HV'
    uint16_t reserved15;
    uint16_t biasVreq[FEE_NUM];
    uint32_t bias_chksum;

    uint16_t functionReq_tag;               // 'CD'
    uint16_t functionReq;
    float P[CT_MAXP];
    uint32_t fncReq_chksum;

    uint32_t reserved17;

    uint32_t sentinel_tag;
    uint32_t reserved18[3];
    } EE_DATABUF_t;


uint8_t* MZB_Encode_CMD_Command_raw(MZB_OSCMDCODE_t cmdCode, float* params);
unsigned ees_chksum(void *ptr, int len);
MZB_OSCMDCODE_t mz_string_to_enum(const char* str);


#endif