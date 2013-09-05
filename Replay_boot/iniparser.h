/** @file iniparser.h */

#ifndef INIPARSER_H_INCLUDED
#define INIPARSER_H_INCLUDED

#include "board.h"
#include "fullfat.h"

/** maximum length of a line in the INI file */
#define MAX_LINE_LEN 128

/** maximum length of a key in the LUT */
#define MAX_KEYLENGTH 15

// ===========================================================

/// our tokens we map the INI keywords to
typedef enum {
    INI_START,    ///< virtual token, signals start-of-file
    INI_UNKNOWN,  ///< virtual token, signals end-of-file or section change
    INI_SETUP,    ///< token for SETUP section
    INI_INFO,     ///< token for INFO line
    INI_BIN,      ///< token for BIN keyword (FPGA config file definition) 
    INI_CLOCK,    ///< token for CLOCK keyword (PLL chip configuration)
    INI_CODER,    ///< token for CODER keyword (TV coder configuration)
    INI_VFILTER,  ///< token for VFILTER keyword (video filter configuration)
    INI_EN_TWI,   ///< token for EN_TWI keyword (enables post-TWI usage)
    INI_EN_SPI,   ///< token for EN_SPI keyword (enables post-SPI usage)
    INI_SPI_CLK,  ///< token for INI_SPI_CLK keyword (sets SPI speed)
    INI_BUTTON,   ///< token for EN_BUTTON keyword (replay button behavior)
    INI_VIDEO,    ///< token for VIDEO keyword (video DAC configuration)
    INI_CONFIG,   ///< token for CONFIG keyword (config bits for FPGA/OSD)
    INI_CSTORE,   ///< token for CSTORE keyword (filename to store dynamic config part)
    INI_UPLOAD,   ///< token for UPLOAD section
    INI_VERIFY,   ///< token for VERIFY keyword (enables verification of any upload)
    INI_ROM,      ///< token for ROM keyword (ROM file entry for upload)
    INI_MENU,     ///< token for MENU section
    INI_DATA,     ///< token for DATA keyword (DATA set for upload)
    INI_TITLE,    ///< token for TITLE keyword (menu title)
    INI_ITEM,     ///< token for ITEM keyword (menu item for a title)
    INI_OPTION,   ///< token for OPTION keyword (option for menu item)
} ini_symbols_t;

/// used as LUT for mapping keywords to tokens
typedef struct {
  const char keyword[MAX_KEYLENGTH];  ///< keyword as string to match
  const ini_symbols_t token;          ///< token for the given keyword
  const uint8_t section;              ///< 1 if keyword must be a section name
} ini_symtab_t;

/** @brief INI FILE PARSER

    Reads and parses an INI file, calls a handler for each valid line.

    @param pFile file handle of an opened INI file (readonly is sufficient)
    @param parseHandle handler to be called for an INI file entry
    @param  pointer to a datastructure passed to the handler (usually the replay board status dataset)

    @return 0 when run was successful, others indicate the linenumber with a failure
*/
uint8_t ParseIni(FF_FILE* pFile, 
                uint8_t(*parseHandle)(void*, const ini_symbols_t, const ini_symbols_t, const char*), 
                void* config);

// ===========================================================

/// table generated by list parser
typedef struct {
  char    strval[MAX_LINE_LEN]; ///< list entry as string
  int32_t intval;               ///< list entry as number
} ini_list_t;

/** @brief LIST PARSER

    Parses a single, comma-separated value to a list// list item parser:

    We get a comma-separated list and return the string and the integer value
    of each item.

    Translation is done as: %?? (binary) 0?? (octal) 0x?? (hex) and [-]?? (decimal).

    We allow also "no" value like 1,,2,3 (which is simply intepreted as zero),
    but spaces are NOT possible for now, would require wiggling with the ini parser...

    @param value string of comma separated list of strings and numbers
    @param valueList pointer to a data structure holding the parse result
    @param maxlen maximum amount of entries to be stored in reserved data structure 

    @return number of entries found, 0 indicates an error (no list entry found)
*/
uint16_t ParseList(const char* value, ini_list_t* valueList, const uint16_t maxlen);

#endif

