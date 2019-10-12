/*
 * Afero Device Profile header file
 * Device Description:		
 * Schema Version:	2
 */

// Module Configurations
#define AF_BOARD_MODULO_1                                         0
#define AF_BOARD_MODULO_1B                                        1
#define AF_BOARD_MODULO_2                                         2
#define AF_BOARD_PLUMO_2C                                         3
#define AF_BOARD_PLUMO_2D                                         4
#define AF_BOARD_ABELO_2A                                         5
#define AF_BOARD_ABELO_2B                                         6
#define AF_BOARD_POTENCO                                          7
#define AF_BOARD_QUANTA                                           8

#define AF_BOARD                                   AF_BOARD_POTENCO

// Data Types
#define ATTRIBUTE_TYPE_BOOLEAN                                    1
#define ATTRIBUTE_TYPE_SINT8                                      2
#define ATTRIBUTE_TYPE_SINT16                                     3
#define ATTRIBUTE_TYPE_SINT32                                     4
#define ATTRIBUTE_TYPE_SINT64                                     5
#define ATTRIBUTE_TYPE_Q_15_16                                    6
#define ATTRIBUTE_TYPE_UTF8S                                     20
#define ATTRIBUTE_TYPE_BYTES                                     21

//region Service ID 1
// Attribute GetDoubled
#define AF_GETDOUBLED                                             1
#define AF_GETDOUBLED_SZ                                          2
#define AF_GETDOUBLED_TYPE                    ATTRIBUTE_TYPE_SINT16

// Attribute Doubled
#define AF_DOUBLED                                                2
#define AF_DOUBLED_SZ                                             4
#define AF_DOUBLED_TYPE                       ATTRIBUTE_TYPE_SINT32

// Attribute GetRotated
#define AF_GETROTATED                                             3
#define AF_GETROTATED_SZ                                          4
#define AF_GETROTATED_TYPE                    ATTRIBUTE_TYPE_SINT32

// Attribute Rotate
#define AF_ROTATE                                                 4
#define AF_ROTATE_SZ                                              4
#define AF_ROTATE_TYPE                        ATTRIBUTE_TYPE_SINT32

// Attribute ToggleLED
#define AF_TOGGLELED                                              5
#define AF_TOGGLELED_SZ                                           1
#define AF_TOGGLELED_TYPE                    ATTRIBUTE_TYPE_BOOLEAN

// Attribute GetAdded
#define AF_GETADDED                                               6
#define AF_GETADDED_SZ                                            1
#define AF_GETADDED_TYPE                       ATTRIBUTE_TYPE_SINT8

// Attribute CurrentSum
#define AF_CURRENTSUM                                             7
#define AF_CURRENTSUM_SZ                                          4
#define AF_CURRENTSUM_TYPE                    ATTRIBUTE_TYPE_SINT32

// Attribute ReadVarLog
#define AF_READVARLOG                                             8
#define AF_READVARLOG_SZ                                          1
#define AF_READVARLOG_TYPE                   ATTRIBUTE_TYPE_BOOLEAN

// Attribute LastLineOfVarLog
#define AF_LASTLINEOFVARLOG                                       9
#define AF_LASTLINEOFVARLOG_SZ                                 1536
#define AF_LASTLINEOFVARLOG_TYPE               ATTRIBUTE_TYPE_UTF8S

// Attribute GetReversed
#define AF_GETREVERSED                                           10
#define AF_GETREVERSED_SZ                                      1536
#define AF_GETREVERSED_TYPE                    ATTRIBUTE_TYPE_UTF8S

// Attribute Reversed
#define AF_REVERSED                                              11
#define AF_REVERSED_SZ                                         1536
#define AF_REVERSED_TYPE                       ATTRIBUTE_TYPE_UTF8S

// Attribute CountBitsOfThis
#define AF_COUNTBITSOFTHIS                                       12
#define AF_COUNTBITSOFTHIS_SZ                                     4
#define AF_COUNTBITSOFTHIS_TYPE               ATTRIBUTE_TYPE_SINT32

// Attribute NumberOfBits
#define AF_NUMBEROFBITS                                          13
#define AF_NUMBEROFBITS_SZ                                        1
#define AF_NUMBEROFBITS_TYPE                   ATTRIBUTE_TYPE_SINT8

// Attribute RotatedR
#define AF_ROTATEDR                                              14
#define AF_ROTATEDR_SZ                                            4
#define AF_ROTATEDR_TYPE                      ATTRIBUTE_TYPE_SINT32

// Attribute RotateL
#define AF_ROTATEL                                               15
#define AF_ROTATEL_SZ                                             4
#define AF_ROTATEL_TYPE                       ATTRIBUTE_TYPE_SINT32

// Attribute Application Version
#define AF_APPLICATION_VERSION                                 2003
#define AF_APPLICATION_VERSION_SZ                                 8
#define AF_APPLICATION_VERSION_TYPE           ATTRIBUTE_TYPE_SINT64

// Attribute Profile Version
#define AF_PROFILE_VERSION                                     2004
#define AF_PROFILE_VERSION_SZ                                     8
#define AF_PROFILE_VERSION_TYPE               ATTRIBUTE_TYPE_SINT64

// Attribute Hub Version
#define AF_HUB_VERSION                                         2005
#define AF_HUB_VERSION_SZ                                         8
#define AF_HUB_VERSION_TYPE                   ATTRIBUTE_TYPE_SINT64

// Attribute UTC Offset Data
#define AF_SYSTEM_UTC_OFFSET_DATA                             65001
#define AF_SYSTEM_UTC_OFFSET_DATA_SZ                              8
#define AF_SYSTEM_UTC_OFFSET_DATA_TYPE         ATTRIBUTE_TYPE_BYTES

// Attribute Connected SSID
#define AF_SYSTEM_CONNECTED_SSID                              65004
#define AF_SYSTEM_CONNECTED_SSID_SZ                              33
#define AF_SYSTEM_CONNECTED_SSID_TYPE          ATTRIBUTE_TYPE_UTF8S

// Attribute Wi-Fi Bars
#define AF_SYSTEM_WI_FI_BARS                                  65005
#define AF_SYSTEM_WI_FI_BARS_SZ                                   1
#define AF_SYSTEM_WI_FI_BARS_TYPE              ATTRIBUTE_TYPE_SINT8

// Attribute Wi-Fi Steady State
#define AF_SYSTEM_WI_FI_STEADY_STATE                          65006
#define AF_SYSTEM_WI_FI_STEADY_STATE_SZ                           1
#define AF_SYSTEM_WI_FI_STEADY_STATE_TYPE      ATTRIBUTE_TYPE_SINT8

// Attribute Network Type
#define AF_SYSTEM_NETWORK_TYPE                                65008
#define AF_SYSTEM_NETWORK_TYPE_SZ                                 1
#define AF_SYSTEM_NETWORK_TYPE_TYPE            ATTRIBUTE_TYPE_SINT8

// Attribute Command
#define AF_SYSTEM_COMMAND                                     65012
#define AF_SYSTEM_COMMAND_SZ                                     64
#define AF_SYSTEM_COMMAND_TYPE                 ATTRIBUTE_TYPE_BYTES

// Attribute ASR State
#define AF_SYSTEM_ASR_STATE                                   65013
#define AF_SYSTEM_ASR_STATE_SZ                                    1
#define AF_SYSTEM_ASR_STATE_TYPE               ATTRIBUTE_TYPE_SINT8

// Attribute Linked Timestamp
#define AF_SYSTEM_LINKED_TIMESTAMP                            65015
#define AF_SYSTEM_LINKED_TIMESTAMP_SZ                             4
#define AF_SYSTEM_LINKED_TIMESTAMP_TYPE       ATTRIBUTE_TYPE_SINT32

// Attribute Reboot Reason
#define AF_SYSTEM_REBOOT_REASON                               65019
#define AF_SYSTEM_REBOOT_REASON_SZ                              100
#define AF_SYSTEM_REBOOT_REASON_TYPE           ATTRIBUTE_TYPE_UTF8S

// Attribute Network Capabilities
#define AF_SYSTEM_NETWORK_CAPABILITIES                        65022
#define AF_SYSTEM_NETWORK_CAPABILITIES_SZ                         1
#define AF_SYSTEM_NETWORK_CAPABILITIES_TYPE    ATTRIBUTE_TYPE_SINT8

// Attribute Wi-Fi Interface State
#define AF_SYSTEM_WI_FI_INTERFACE_STATE                       65024
#define AF_SYSTEM_WI_FI_INTERFACE_STATE_SZ                        1
#define AF_SYSTEM_WI_FI_INTERFACE_STATE_TYPE   ATTRIBUTE_TYPE_SINT8

// Attribute WAN Interface State
#define AF_SYSTEM_WAN_INTERFACE_STATE                         65037
#define AF_SYSTEM_WAN_INTERFACE_STATE_SZ                          1
#define AF_SYSTEM_WAN_INTERFACE_STATE_TYPE     ATTRIBUTE_TYPE_SINT8

// Attribute Device Capability
#define AF_SYSTEM_DEVICE_CAPABILITY                           65066
#define AF_SYSTEM_DEVICE_CAPABILITY_SZ                            8
#define AF_SYSTEM_DEVICE_CAPABILITY_TYPE       ATTRIBUTE_TYPE_BYTES
//endregion Service ID 1
